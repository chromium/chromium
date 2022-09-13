// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_MOCK_REMOVABLE_STORAGE_OBSERVER_H_
#define COMPONENTS_STORAGE_MONITOR_MOCK_REMOVABLE_STORAGE_OBSERVER_H_

#include "components/storage_monitor/removable_storage_observer.h"
#include "components/storage_monitor/storage_info.h"

namespace storage_monitor {

class MockRemovableStorageObserver : public RemovableStorageObserver {
 public:
  MockRemovableStorageObserver();
  ~MockRemovableStorageObserver() override;

  void OnRemovableStorageAttached(const StorageInfo& info) override;

  void OnRemovableStorageDetached(const StorageInfo& info) override;

  int attach_calls() { return attach_calls_; }

  int detach_calls() { return detach_calls_; }

  const StorageInfo& last_attached() {
    return last_attached_;
  }

  const StorageInfo& last_detached() {
    return last_detached_;
  }

 private:
  int attach_calls_;
  int detach_calls_;
  StorageInfo last_attached_;
  StorageInfo last_detached_;
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_MOCK_REMOVABLE_STORAGE_OBSERVER_H_
