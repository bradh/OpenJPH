/****************************************************************************/
// This software is released under the 2-Clause BSD license, included
// below.
//
// Copyright (c) 2019, Aous Naman 
// Copyright (c) 2019, Kakadu Software Pty Ltd, Australia
// Copyright (c) 2019, The University of New South Wales, Australia
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/****************************************************************************/
// This file is part of the OpenJPH software implementation.
// File: ojph_transform_sse.cpp
// Author: Aous Naman
// Date: 28 August 2019
/****************************************************************************/

#include <cstdio>

#include "ojph_transform.h"
#include "ojph_transform_local.h"

#ifdef OJPH_COMPILER_MSVC
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

namespace ojph {
  namespace local {

    //////////////////////////////////////////////////////////////////////////
    void sse_irrev_vert_wvlt_step(const float* src1, const float* src2,
                                  float *dst, int step_num, int repeat)
    {
      __m128 factor = _mm_set1_ps(LIFTING_FACTORS::steps[step_num]);
      for (int i = (repeat + 3) >> 2; i > 0; --i, dst+=4, src1+=4, src2+=4)
      {
        __m128 s1 = _mm_load_ps(src1);
        __m128 s2 = _mm_load_ps(src2);
        __m128 d = _mm_load_ps(dst);
        d = _mm_add_ps(d, _mm_mul_ps(factor, _mm_add_ps(s1, s2)));
        _mm_store_ps(dst, d);
      }
    }

    /////////////////////////////////////////////////////////////////////////
    void sse_irrev_vert_wvlt_K(const float* src, float* dst,
                               bool L_analysis_or_H_synthesis, int repeat)
    {
      float f = LIFTING_FACTORS::K_inv;
      f = L_analysis_or_H_synthesis ? f : LIFTING_FACTORS::K;
      __m128 factor = _mm_set1_ps(f);
      for (int i = (repeat + 3) >> 2; i > 0; --i, dst+=4, src+=4)
      {
        __m128 s = _mm_load_ps(src);
        _mm_store_ps(dst, _mm_mul_ps(factor, s));
      }
    }

    /////////////////////////////////////////////////////////////////////////
    void sse_irrev_horz_wvlt_fwd_tx(float* src, float *ldst, float *hdst,
                                    int width, bool even)
    {
      if (width > 1)
      {
        const int L_width = (width + (even ? 1 : 0)) >> 1;
        const int H_width = (width + (even ? 0 : 1)) >> 1;

        //extension
        src[-1] = src[1];
        src[width] = src[width-2];
        // predict
        float factor = LIFTING_FACTORS::steps[0];
        const float* sp = src + (even ? 1 : 0);
        float *dph = hdst;
        for (int i = H_width; i > 0; --i, sp+=2)
          *dph++ = sp[0] + factor * (sp[-1] + sp[1]);

        // extension
        hdst[-1] = hdst[0];
        hdst[H_width] = hdst[H_width-1];
        // update
        factor = LIFTING_FACTORS::steps[1];
        sp = src + (even ? 0 : 1);
        const float* sph = hdst + (even ? 0 : 1);
        float *dpl = ldst;
        for (int i = L_width; i > 0; --i, sp+=2, sph++)
          *dpl++ = sp[0] + factor * (sph[-1] + sph[0]);

        //extension
        ldst[-1] = ldst[0];
        ldst[L_width] = ldst[L_width-1];
        //predict
        factor = LIFTING_FACTORS::steps[2];
        const float* spl = ldst + (even ? 1 : 0);
        dph = hdst;
        for (int i = H_width; i > 0; --i, spl++)
          *dph++ += factor * (spl[-1] + spl[0]);

        // extension
        hdst[-1] = hdst[0];
        hdst[H_width] = hdst[H_width-1];
        // update
        factor = LIFTING_FACTORS::steps[3];
        sph = hdst + (even ? 0 : 1);
        dpl = ldst;
        for (int i = L_width; i > 0; --i, sph++)
          *dpl++ += factor * (sph[-1] + sph[0]);

        //multipliers
        float *dp = ldst;
        for (int i = L_width; i > 0; --i, dp++)
          *dp *= LIFTING_FACTORS::K_inv;
        dp = hdst;
        for (int i = H_width; i > 0; --i, dp++)
          *dp *= LIFTING_FACTORS::K;
      }
      else
      {
        if (even)
          ldst[0] = src[0];
        else
          hdst[0] = src[0];
      }
    }

    /////////////////////////////////////////////////////////////////////////
    void sse_irrev_horz_wvlt_bwd_tx(float* dst, float *lsrc, float *hsrc,
                                    int width, bool even)
    {
      if (width > 1)
      {
        const int L_width = (width + (even ? 1 : 0)) >> 1;
        const int H_width = (width + (even ? 0 : 1)) >> 1;

        //multipliers
        float *dp = lsrc;
        for (int i = L_width; i > 0; --i, dp++)
          *dp *= LIFTING_FACTORS::K;
        dp = hsrc;
        for (int i = H_width; i > 0; --i, dp++)
          *dp *= LIFTING_FACTORS::K_inv;

        //extension
        hsrc[-1] = hsrc[0];
        hsrc[H_width] = hsrc[H_width-1];
        //inverse update
        float factor = LIFTING_FACTORS::steps[7];
        const float *sph = hsrc + (even ? 0 : 1);
        float *dpl = lsrc;
        for (int i = L_width; i > 0; --i, dpl++, sph++)
          *dpl += factor * (sph[-1] + sph[0]);

        //extension
        lsrc[-1] = lsrc[0];
        lsrc[L_width] = lsrc[L_width-1];
        //inverse perdict
        factor = LIFTING_FACTORS::steps[6];
        const float *spl = lsrc + (even ? 0 : -1);
        float *dph = hsrc;
        for (int i = H_width; i > 0; --i, dph++, spl++)
          *dph += factor * (spl[0] + spl[1]);

        //extension
        hsrc[-1] = hsrc[0];
        hsrc[H_width] = hsrc[H_width-1];
        //inverse update
        factor = LIFTING_FACTORS::steps[5];
        sph = hsrc + (even ? 0 : 1);
        dpl = lsrc;
        for (int i = L_width; i > 0; --i, dpl++, sph++)
          *dpl += factor * (sph[-1] + sph[0]);

        //extension
        lsrc[-1] = lsrc[0];
        lsrc[L_width] = lsrc[L_width-1];
        //inverse perdict and combine
        factor = LIFTING_FACTORS::steps[4];
        dp = dst + (even ? 0 : -1);
        spl = lsrc + (even ? 0 : -1);
        sph = hsrc;
        for (int i = L_width+(even?0:1); i > 0; --i, spl++, sph++)
        {
          *dp++ = *spl;
          *dp++ = *sph + factor * (spl[0] + spl[1]);
        }
      }
      else
      {
        if (even)
          dst[0] = lsrc[0];
        else
          dst[0] = hsrc[0];
      }
    }
  }
}
