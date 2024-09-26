// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_entry_builder_base.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/containers/flat_set.h"
#include "base/metrics/metrics_hashes.h"
#include "components/metrics/dwa/mojom/dwa_interface.mojom.h"

namespace dwa::internal {

DwaEntryBuilderBase::DwaEntryBuilderBase(DwaEntryBuilderBase&&) = default;

DwaEntryBuilderBase& DwaEntryBuilderBase::operator=(DwaEntryBuilderBase&&) =
    default;

DwaEntryBuilderBase::~DwaEntryBuilderBase() = default;

DwaEntryBuilderBase::DwaEntryBuilderBase(uint64_t event_hash)
    : entry_(metrics::dwa::mojom::DwaEntry::New()) {
  entry_->event_hash = event_hash;
}

void DwaEntryBuilderBase::SetContentInternal(uint64_t content_hash) {
  entry_->content_hash = content_hash;
}

void DwaEntryBuilderBase::SetMetricInternal(uint64_t metric_hash,
                                            int64_t value) {
  entry_->metrics.insert_or_assign(metric_hash, value);
}

void DwaEntryBuilderBase::AddToStudiesOfInterestInternal(
    std::string_view study_name) {
  entry_->studies_of_interest.insert_or_assign(std::string(study_name), true);
}

void DwaEntryBuilderBase::Record(metrics::dwa::DwaRecorder* recorder) {
  if (recorder) {
    recorder->AddEntry(std::move(entry_));
  } else {
    entry_.reset();
  }
}

metrics::dwa::mojom::DwaEntryPtr* DwaEntryBuilderBase::GetEntryForTesting() {
  return &entry_;
}

}  // namespace dwa::internal
