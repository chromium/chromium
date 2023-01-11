// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_ZRAM_WRITEBACK_POLICY_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_ZRAM_WRITEBACK_POLICY_H_

#include <cstdint>
#include <memory>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace ash::memory {

// The policy is responsible for when things happen, and how much, the backend
// does the actual work.
class COMPONENT_EXPORT(ASH_MEMORY) ZramWritebackPolicy {
 public:
  virtual ~ZramWritebackPolicy() = default;

  static std::unique_ptr<ZramWritebackPolicy> Get();

  virtual void Initialize(uint64_t zram_disk_size_mb,
                          uint64_t writeback_size_mb) = 0;

  virtual bool CanWritebackHugeIdle() = 0;
  virtual bool CanWritebackIdle() = 0;
  virtual bool CanWritebackHuge() = 0;

  // GetCurrentWritebackIdleTime will return the age the controller should pass
  // to the backend for idle pages, this idle time applies to both Idle and Huge
  // Idle writeback modes.
  virtual base::TimeDelta GetCurrentWritebackIdleTime() = 0;

  // GetAllowedWritebackLimit returns the number of pages the backend should
  // writeback (this interval).
  virtual uint64_t GetAllowedWritebackLimit() = 0;

  // GetWritebackTimerInterval is the period in which the controller should
  // query the policy.
  virtual base::TimeDelta GetWritebackTimerInterval() = 0;
};

}  // namespace ash::memory

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_ZRAM_WRITEBACK_POLICY_H_
