// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_invalidation_adapter.h"

#include "base/check.h"

namespace syncer {

SyncInvalidationAdapter::SyncInvalidationAdapter(const std::string& payload,
                                                 std::optional<int64_t> version)
    : payload_(payload), version_(version) {}

SyncInvalidationAdapter::~SyncInvalidationAdapter() = default;

bool SyncInvalidationAdapter::IsUnknownVersion() const {
  return !version_.has_value();
}

const std::string& SyncInvalidationAdapter::GetPayload() const {
  return payload_;
}

int64_t SyncInvalidationAdapter::GetVersion() const {
  DCHECK(version_.has_value());
  return version_.value();
}

void SyncInvalidationAdapter::Acknowledge() {}

void SyncInvalidationAdapter::Drop() {}

}  // namespace syncer
