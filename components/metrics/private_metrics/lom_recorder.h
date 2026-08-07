// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_LOM_RECORDER_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_LOM_RECORDER_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "components/metrics/private_metrics/puma_histogram_functions.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/metrics_proto/histogram_event.pb.h"
#include "third_party/metrics_proto/private_metrics/private_user_metrics.pb.h"

namespace metrics::private_metrics {

class COMPONENT_EXPORT(PRIVATE_METRICS_RECORDERS) LomRecorder {
 public:
  LomRecorder();
  ~LomRecorder();

  LomRecorder(const LomRecorder&) = delete;
  LomRecorder& operator=(const LomRecorder&) = delete;

  // Provides access to a global LomRecorder instance for recording metrics.
  static LomRecorder* Get();

  // Records a boolean sample.
  // `profile_id` is the ID of the profile to associate the metric with.
  // There is no difference in behavior between std::nullopt and 0
  // (both associate the metric with the default (global) profile where profile
  // ID is 0).
  void RecordBoolean(PumaType puma_type,
                     std::string_view name,
                     bool sample,
                     std::optional<uint64_t> profile_id = std::nullopt);

  // Records an exact linear sample.
  // `profile_id` is the ID of the profile to associate the metric with.
  // There is no difference in behavior between std::nullopt and 0
  // (both associate the metric with the default (global) profile where profile
  // ID is 0).
  void RecordExactLinear(PumaType puma_type,
                         std::string_view name,
                         int sample,
                         int exclusive_max,
                         std::optional<uint64_t> profile_id = std::nullopt);

  // Clears and returns all recorded histogram events grouped by profile.
  std::vector<::private_metrics::ProfileKeyedHistogramEvent>
  TakeHistogramEvents();

 private:
  void RecordSample(std::string_view name,
                    int64_t sample,
                    int64_t min,
                    int64_t max,
                    std::optional<uint64_t> profile_id);

  SEQUENCE_CHECKER(sequence_checker_);

  // Maps a profile name hash to its corresponding HistogramEventProto.
  // The key is the hash of the profile name, or 0 if it is the default (global)
  // profile.
  using ProfileKeyedHistogramMap =
      absl::flat_hash_map<uint64_t, metrics::HistogramEventProto>;

  // Maps a histogram name hash to its profile-keyed histogram events.
  // The key is the hash of the histogram name.
  absl::flat_hash_map<uint64_t, ProfileKeyedHistogramMap> histograms_;
};

}  // namespace metrics::private_metrics

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_LOM_RECORDER_H_
