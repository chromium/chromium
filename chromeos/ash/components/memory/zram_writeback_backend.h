// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_ZRAM_WRITEBACK_BACKEND_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_ZRAM_WRITEBACK_BACKEND_H_

#include <cstdint>
#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace ash::memory {

enum class ZramWritebackMode : int {
  kModeNone = 0,
  kModeIdle = 1,
  kModeHuge = 2,
  kModeHugeIdle = 4
};

// The backend is responsible for the "how" things happens.
class COMPONENT_EXPORT(ASH_MEMORY) ZramWritebackBackend {
 public:
  virtual ~ZramWritebackBackend() = default;

  using IntCallback = base::OnceCallback<void(bool, int64_t)>;
  virtual void EnableWriteback(uint64_t size_mb, IntCallback cb) = 0;
  virtual void SetWritebackLimit(uint64_t size_pages, IntCallback cb) = 0;

  using Callback = base::OnceCallback<void(bool)>;
  virtual void InitiateWriteback(ZramWritebackMode mode, Callback cb) = 0;
  virtual void MarkIdle(base::TimeDelta age, Callback cb) = 0;
  virtual bool WritebackAlreadyEnabled() = 0;

  // If and only if writeback already enabled returns true, then
  // GetPreviousBackingSize will return the size (in MB) of the backing
  // device that's currently configured.
  virtual void GetCurrentBackingDevSize(IntCallback cb) = 0;

  // Returns the value currently stored, this can be read AFTER an initiate
  // writeback to determine how many pages were written back. This will
  // return -1 if there is no limit set.
  virtual int64_t GetCurrentWritebackLimitPages() = 0;
  virtual int64_t GetZramDiskSizeBytes() = 0;

  static std::unique_ptr<ZramWritebackBackend> Get();
  static bool IsSupported();
};
}  // namespace ash::memory

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_ZRAM_WRITEBACK_BACKEND_H_
