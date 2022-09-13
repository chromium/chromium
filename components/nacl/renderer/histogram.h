// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains a set of histogram support functions for logging behavior
// seen while loading NaCl plugins.

#ifndef COMPONENTS_NACL_RENDERER_HISTOGRAM_H_
#define COMPONENTS_NACL_RENDERER_HISTOGRAM_H_

#include <stdint.h>

#include <string>
#include "base/time/time.h"
#include "components/nacl/renderer/ppb_nacl_private.h"

namespace nacl {

void HistogramCustomCounts(const std::string& name,
                           int32_t sample,
                           int32_t min,
                           int32_t max,
                           uint32_t bucket_count);

void HistogramEnumerate(const std::string& name,
                        int32_t sample,
                        int32_t boundary_value);

void HistogramEnumerateLoadStatus(PP_NaClError error_code,
                                  bool is_installed);

void HistogramEnumerateOsArch(const std::string& sandbox_isa);

// Records values up to 20 seconds.
void HistogramTimeSmall(const std::string& name, int64_t sample);
// Records values up to 3 minutes, 20 seconds.
void HistogramTimeMedium(const std::string& name, int64_t sample);
// Records values up to 33 minutes.
void HistogramTimeLarge(const std::string& name, int64_t sample);
// Records values up to 12 minutes.
void HistogramTimeTranslation(const std::string& name, int64_t sample_ms);

void HistogramStartupTimeSmall(const std::string& name,
                               base::TimeDelta td,
                               int64_t nexe_size);
void HistogramStartupTimeMedium(const std::string& name,
                                base::TimeDelta td,
                                int64_t nexe_size);
void HistogramSizeKB(const std::string& name, int32_t sample);
void HistogramHTTPStatusCode(const std::string& name, int32_t status);
void HistogramEnumerateManifestIsDataURI(bool is_data_uri);
void HistogramKBPerSec(const std::string& name, int64_t kb, int64_t us);
void HistogramRatio(const std::string& name,
                    int64_t numerator,
                    int64_t denominator);

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_HISTOGRAM_H_
