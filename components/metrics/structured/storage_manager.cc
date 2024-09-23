// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/storage_manager.h"

namespace metrics::structured {

StorageManager::StorageManager() = default;

StorageManager::~StorageManager() = default;

void StorageManager::NotifyOnFlushed(const FlushedKey& key) {
  if (delegate_) {
    delegate_->OnFlushed(key);
  }
}

void StorageManager::NotifyOnDeleted(const FlushedKey& key,
                                     DeleteReason reason) {
  if (delegate_) {
    delegate_->OnDeleted(key, reason);
  }
}
}  // namespace metrics::structured
