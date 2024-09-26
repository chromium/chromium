// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_entry_builder.h"

#include "base/metrics/metrics_hashes.h"

namespace dwa {

DwaEntryBuilder::DwaEntryBuilder(std::string_view event_name)
    : dwa::internal::DwaEntryBuilderBase(base::HashMetricName(event_name)) {}

DwaEntryBuilder::~DwaEntryBuilder() = default;

void DwaEntryBuilder::SetContent(std::string_view content) {
  SetContentInternal(base::HashMetricName(content));
}

void DwaEntryBuilder::SetMetric(std::string_view metric_name, int64_t value) {
  SetMetricInternal(base::HashMetricName(metric_name), value);
}

void DwaEntryBuilder::AddToStudiesOfInterest(std::string_view study_name) {
  AddToStudiesOfInterestInternal(study_name);
}

}  // namespace dwa
