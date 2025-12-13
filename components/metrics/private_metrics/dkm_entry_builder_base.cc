// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/dkm_entry_builder_base.h"

#include <memory>
#include <string>

#include "components/metrics/private_metrics/dkm_recorder.h"
#include "components/metrics/private_metrics/mojom/private_metrics_interface.mojom.h"

namespace metrics::private_metrics::internal {

DkmEntryBuilderBase::DkmEntryBuilderBase(DkmEntryBuilderBase&&) = default;

DkmEntryBuilderBase& DkmEntryBuilderBase::operator=(DkmEntryBuilderBase&&) =
    default;

DkmEntryBuilderBase::~DkmEntryBuilderBase() = default;

DkmEntryBuilderBase::DkmEntryBuilderBase(ukm::SourceIdObj source_id,
                                         uint64_t event_hash)
    : entry_(mojom::PrivateMetricsEntry::New()) {
  entry_->type = mojom::PrivateMetricsEntry::Type::kDkm;
  entry_->source_id = source_id.ToInt64();
  entry_->event_hash = event_hash;
}

void DkmEntryBuilderBase::SetMetricInternal(uint64_t metric_hash,
                                            int64_t value) {
  entry_->metrics.insert_or_assign(metric_hash, value);
}

void DkmEntryBuilderBase::AddToStudiesOfInterestInternal(
    std::string_view study_name) {
  entry_->studies_of_interest.insert_or_assign(std::string(study_name), true);
}

void DkmEntryBuilderBase::Record(DkmRecorder* recorder) {
  if (recorder) {
    recorder->AddEntry(std::move(entry_));
  } else {
    entry_.reset();
  }
}

mojom::PrivateMetricsEntryPtr* DkmEntryBuilderBase::GetEntryForTesting() {
  return &entry_;
}

}  // namespace metrics::private_metrics::internal
