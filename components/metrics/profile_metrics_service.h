// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PROFILE_METRICS_SERVICE_H_
#define COMPONENTS_METRICS_PROFILE_METRICS_SERVICE_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/keyed_service/core/keyed_service.h"

namespace metrics {

using ProfileMetricsContext = std::optional<size_t>;

// A service responsible for logging per-profile metrics. Code wishing to log a
// per-profile metric should use a method from this service, rather than calling
// the base metrics histogram macros directly.
//
// This service is responsible for logging both the global histogram (always),
// and the per-profile histogram (if the profile is suitable for per-profile
// histograms, on supported platforms).
//
// The histograms should be defined in the XML file using the `ProfileIndex`
// variant from tools/metrics/histograms/metadata/profile/histograms.xml
class ProfileMetricsService : public KeyedService {
 public:
  explicit ProfileMetricsService(ProfileMetricsContext context = std::nullopt);

  ProfileMetricsService(const ProfileMetricsService&) = delete;
  ProfileMetricsService& operator=(const ProfileMetricsService&) = delete;

  // These methods exactly mirror those in base/metrics/histogram_functions.h.
  // See usage comments in those methods for more details.
  //
  // TODO(crbug.com/417921579): Add the other methods.
  template <typename T>
  void UmaHistogramEnumeration(std::string_view name, T sample) {
    base::UmaHistogramEnumeration(name, sample);
    if (!histogram_suffix_.empty()) {
      base::UmaHistogramEnumeration(base::StrCat({name, histogram_suffix_}),
                                    sample);
    }
  }

 private:
  const ProfileMetricsContext profile_metrics_context_;

  // The metrics suffix to append to the base histogram name for per-profile
  // histograms, eg. ".Profile1".
  // May be empty, in which case no per-profile histograms are logged.
  const std::string histogram_suffix_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_PROFILE_METRICS_SERVICE_H_
