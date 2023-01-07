// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_ZRAM_WRITEBACK_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_ZRAM_WRITEBACK_CONTROLLER_H_

#include <cstdint>
#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/memory/zram_writeback_backend.h"
#include "chromeos/ash/components/memory/zram_writeback_policy.h"

namespace ash::memory {

// The controller bridges the policy and backend.
class COMPONENT_EXPORT(ASH_MEMORY) ZramWritebackController {
 public:
  ~ZramWritebackController();
  ZramWritebackController(const ZramWritebackController&) = delete;
  ZramWritebackController& operator=(const ZramWritebackController&) = delete;

  static std::unique_ptr<ZramWritebackController> Create();
  static bool IsSupportedAndEnabled();
  void Start();
  void Stop();

 private:
  friend class ZramWritebackControllerTest;
  ZramWritebackController(std::unique_ptr<ZramWritebackPolicy> policy,
                          std::unique_ptr<ZramWritebackBackend> backend);

  void PeriodicWriteback();
  void ReadyToWriteback();
  void OnEnableWriteback(bool result, int64_t writeback_size_mb);
  void OnSetWritebackLimit(bool result, int64_t num_pages);
  void CompleteInitialization(uint64_t zram_size_mb,
                              uint64_t writeback_size_mb);
  void OnWritebackComplete(ZramWritebackMode mode, bool result);
  void OnMarkIdle(bool result);
  void ResetCurrentlyWritingBack();

  base::RepeatingTimer timer_;
  bool currently_writing_back_ = false;
  uint64_t current_writeback_limit_ = 0;
  ZramWritebackMode current_writeback_mode_ = ZramWritebackMode::kModeNone;

  std::unique_ptr<ZramWritebackPolicy> policy_;
  std::unique_ptr<ZramWritebackBackend> backend_;

  base::WeakPtrFactory<ZramWritebackController> weak_factory_{this};
};

}  // namespace ash::memory

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_ZRAM_WRITEBACK_CONTROLLER_H_
