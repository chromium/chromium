// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/mock_removable_storage_observer.h"

namespace storage_monitor {

MockRemovableStorageObserver::MockRemovableStorageObserver()
    : attach_calls_(0), detach_calls_(0) {
}

MockRemovableStorageObserver::~MockRemovableStorageObserver() {
}

void MockRemovableStorageObserver::OnRemovableStorageAttached(
    const StorageInfo& info) {
  attach_calls_++;
  last_attached_ = info;
}

void MockRemovableStorageObserver::OnRemovableStorageDetached(
    const StorageInfo& info) {
  detach_calls_++;
  last_detached_ = info;
}

}  // namespace storage_monitor
