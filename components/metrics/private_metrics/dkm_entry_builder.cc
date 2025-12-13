// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/dkm_entry_builder.h"

#include "base/metrics/metrics_hashes.h"

namespace metrics::private_metrics {

DkmEntryBuilder::DkmEntryBuilder(ukm::SourceIdObj source_id,
                                 std::string_view event_name)
    : internal::DkmEntryBuilderBase(source_id,
                                    base::HashMetricName(event_name)) {}

DkmEntryBuilder::~DkmEntryBuilder() = default;

void DkmEntryBuilder::SetMetric(std::string_view metric_name, int64_t value) {
  SetMetricInternal(base::HashMetricName(metric_name), value);
}

void DkmEntryBuilder::AddToStudiesOfInterest(std::string_view study_name) {
  AddToStudiesOfInterestInternal(study_name);
}

}  // namespace metrics::private_metrics
