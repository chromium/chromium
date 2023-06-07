// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DEBUGGER_RWLOCK_H_
#define COMPONENTS_VIZ_SERVICE_DEBUGGER_RWLOCK_H_

#include <atomic>
#include <cstdint>

#include "components/viz/service/viz_service_export.h"

namespace rwlock {
// Read-Write lock.
class VIZ_SERVICE_EXPORT RWLock {
 public:
  RWLock();

  void ReadLock();
  void ReadUnlock();
  void WriteLock();
  void WriteUnLock();

 private:
  std::atomic<int32_t> state_ = 0;
};

}  // namespace rwlock

#endif  // COMPONENTS_VIZ_SERVICE_DEBUGGER_RWLOCK_H_
