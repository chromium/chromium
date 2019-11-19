// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/non_blocking_sync_common.h"

#include "base/trace_event/memory_usage_estimator.h"

namespace syncer {

CommitRequestData::CommitRequestData() {}

CommitRequestData::~CommitRequestData() {}

CommitResponseData::CommitResponseData() {}

CommitResponseData::CommitResponseData(const CommitResponseData& other) =
    default;

CommitResponseData::~CommitResponseData() {}

UpdateResponseData::UpdateResponseData() {}

UpdateResponseData::~UpdateResponseData() {}

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
