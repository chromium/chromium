// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_DKM_ENTRY_BUILDER_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_DKM_ENTRY_BUILDER_H_

#include <string_view>

#include "components/metrics/private_metrics/dkm_entry_builder_base.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace metrics::private_metrics {

// A generic builder object for recording entries in a DkmRecorder, when the
// recording code does not statically know the names of the events/metrics.
// Metrics must still be described in dkm.xml, and this will trigger a DCHECK
// if used to record metrics not described there.
//
// Where possible, prefer using generated objects from dkm_builders.h in the
// dkm::builders namespace instead.
//
// The example usage is:
// metrics::private_metrics::DkmEntryBuilder builder(source_id, "PageLoad");
// builder.SetMetric("NavigationStart", navigation_start_time);
// builder.SetMetric("FirstPaint", first_paint_time);
// builder.AddToStudiesOfInterest("Study1");
// builder.Record(dkm_recorder);
class DkmEntryBuilder final : public internal::DkmEntryBuilderBase {
 public:
  DkmEntryBuilder(ukm::SourceIdObj source_id, std::string_view event_name);

  DkmEntryBuilder(const DkmEntryBuilder&) = delete;
  DkmEntryBuilder& operator=(const DkmEntryBuilder&) = delete;

  ~DkmEntryBuilder() override;

  // Adds metric to the current entry. A metric contains a metric name and
  // value.
  void SetMetric(std::string_view metric_name, int64_t value);

  // Adds study name to the set of studies of interest.
  void AddToStudiesOfInterest(std::string_view study_name);
};

}  // namespace metrics::private_metrics

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_DKM_ENTRY_BUILDER_H_
