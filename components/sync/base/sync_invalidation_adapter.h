// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SYNC_INVALIDATION_ADAPTER_H_
#define COMPONENTS_SYNC_BASE_SYNC_INVALIDATION_ADAPTER_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "components/sync/base/sync_invalidation.h"

namespace syncer {

class SyncInvalidationAdapter : public SyncInvalidation {
 public:
  SyncInvalidationAdapter(const std::string& payload,
                          std::optional<int64_t> version);
  ~SyncInvalidationAdapter() override;

  // Implementation of SyncInvalidation.
  bool IsUnknownVersion() const override;
  const std::string& GetPayload() const override;
  int64_t GetVersion() const override;
  void Acknowledge() override;
  void Drop() override;

 private:
  const std::string payload_;
  const std::optional<int64_t> version_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_SYNC_INVALIDATION_ADAPTER_H_
