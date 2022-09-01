// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_TELEMETRY_SAMPLER_POOL_BASE_H_
#define COMPONENTS_REPORTING_METRICS_TELEMETRY_SAMPLER_POOL_BASE_H_

#include "base/strings/string_piece_forward.h"
#include "components/reporting/metrics/configured_sampler.h"

namespace reporting {

// Base class to access telemetry samplers using their names.
class TelemetrySamplerPoolBase {
 public:
  virtual ~TelemetrySamplerPoolBase() = default;

  // Get the configured telemetry sampler associated with `sampler_name`,
  // returns nullptr if `sampler_name` is not associated with any samplers.
  virtual ConfiguredSampler* GetConfiguredTelemetrySampler(
      base::StringPiece sampler_name) = 0;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_TELEMETRY_SAMPLER_POOL_BASE_H_
