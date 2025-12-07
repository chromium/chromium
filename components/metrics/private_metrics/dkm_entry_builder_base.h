// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_DKM_ENTRY_BUILDER_BASE_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_DKM_ENTRY_BUILDER_BASE_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "components/metrics/private_metrics/mojom/private_metrics_interface.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace metrics::private_metrics {

class DkmRecorder;

namespace internal {

// An internal base class for the generated DkmEntry builder objects and the
// DkmEntryBuilder class. DkmEntryBuilder is reserved for the case where it is
// not appropriate to use the auto-generated class. This class should not be
// used directly.
class DkmEntryBuilderBase {
 public:
  DkmEntryBuilderBase(const DkmEntryBuilderBase&) = delete;
  DkmEntryBuilderBase(DkmEntryBuilderBase&&);
  DkmEntryBuilderBase& operator=(const DkmEntryBuilderBase&) = delete;
  DkmEntryBuilderBase& operator=(DkmEntryBuilderBase&&);

  virtual ~DkmEntryBuilderBase();

  // Records the complete entry into the recorder. If recorder is null, the
  // entry is simply discarded. The `entry_` is used up by this call so
  // further calls to this will do nothing.
  void Record(DkmRecorder* recorder);

  // Return a pointer to internal PrivateMetricsEntryPtr for testing.
  mojom::PrivateMetricsEntryPtr* GetEntryForTesting();

 protected:
  DkmEntryBuilderBase(ukm::SourceIdObj source_id, uint64_t event_hash);

  // Add metric to the entry. A metric contains a metric hash and value.
  void SetMetricInternal(uint64_t metric_hash, int64_t value);

  // Add study name to the set of studies of interest.
  void AddToStudiesOfInterestInternal(std::string_view study_name);

 private:
  mojom::PrivateMetricsEntryPtr entry_;
};

}  // namespace internal

}  // namespace metrics::private_metrics

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_DKM_ENTRY_BUILDER_BASE_H_
