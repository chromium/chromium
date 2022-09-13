// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/commit_and_get_updates_types.h"

#include "base/trace_event/memory_usage_estimator.h"

namespace syncer {

CommitRequestData::CommitRequestData() = default;

CommitRequestData::~CommitRequestData() = default;

CommitResponseData::CommitResponseData() = default;

CommitResponseData::CommitResponseData(const CommitResponseData& other) =
    default;

CommitResponseData::CommitResponseData(CommitResponseData&&) = default;

CommitResponseData& CommitResponseData::operator=(const CommitResponseData&) =
    default;

CommitResponseData& CommitResponseData::operator=(CommitResponseData&&) =
    default;

CommitResponseData::~CommitResponseData() = default;

FailedCommitResponseData::FailedCommitResponseData() = default;

FailedCommitResponseData::FailedCommitResponseData(
    const FailedCommitResponseData& other) = default;

FailedCommitResponseData::FailedCommitResponseData(FailedCommitResponseData&&) =
    default;

FailedCommitResponseData& FailedCommitResponseData::operator=(
    const FailedCommitResponseData&) = default;

FailedCommitResponseData& FailedCommitResponseData::operator=(
    FailedCommitResponseData&&) = default;

FailedCommitResponseData::~FailedCommitResponseData() = default;

UpdateResponseData::UpdateResponseData() = default;

UpdateResponseData::~UpdateResponseData() = default;

size_t EstimateMemoryUsage(const CommitRequestData& value) {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  memory_usage += EstimateMemoryUsage(value.entity);
  memory_usage += EstimateMemoryUsage(value.specifics_hash);
  return memory_usage;
}

size_t EstimateMemoryUsage(const UpdateResponseData& value) {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  memory_usage += EstimateMemoryUsage(value.entity);
  memory_usage += EstimateMemoryUsage(value.encryption_key_name);
  return memory_usage;
}

}  // namespace syncer
