// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/zram_writeback_controller.h"

#include <algorithm>
#include <cstdint>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/page_size.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/ash/components/memory/memory.h"
#include "chromeos/ash/components/memory/swap_configuration.h"
#include "chromeos/ash/components/memory/zram_writeback_backend.h"
#include "chromeos/ash/components/memory/zram_writeback_policy.h"

namespace ash::memory {

// The ZramWritebackController handles zram writeback. For more information on
// how zram writeback works, see the kernel documentation:
// https://www.kernel.org/doc/Documentation/admin-guide/blockdev/zram.rst,
// specifically the section on writeback.
//
// Zram Writeback involves a multistep process which is coordinated by the
// controller. The process is as follows:
//
// 1. The WritebackController registers a timer event which is dictated by the
//    policy, this value comes from GetWritebackTimerInterval(). The timer will
//    fire into PeriodicWriteback.
// 2. On a periodic writeback event the controller will query the policy to
//    determine the number of pages allowed to be written back. This value might
//    be zero if the policy has determined that writeback should not happen
// 3. If the policy provides a non-zero writeback limit, this value will be set
//    via the backend by a call to SetWritebackLimit(). The callback will land
//    in OnSetWritebackLimit.
// 4. In OnSetWritebackLimit we will query the policy to determine if we will be
//    writing back any idle pages. This will happen via calls to
//    CanWriteback(HugeIdle|Idle). If either is true, the policy will be queried
//    to determine the idle age by a call to GetCurrentWritebackIdleTime(). This
//    value will then be passed to the backend to be set in the kernel sysfs
//    file.
// 5. At this point writeback will begin by calls to the backend
//    InitiateWriteback() with the writeback types allowed, the ordering (if
//    enabled) will be: Huge Idle, Idle, Huge. If after any of these writebacks
//    the current writeback limit falls to zero, which is determined by calling
//    the backend's GetCurrentWritebackLimit() method) we stop without
//    proceeding on to the next type.
ZramWritebackController::ZramWritebackController(
    std::unique_ptr<ZramWritebackPolicy> policy,
    std::unique_ptr<ZramWritebackBackend> backend)
    : policy_(std::move(policy)), backend_(std::move(backend)) {}

// static
std::unique_ptr<ZramWritebackController> ZramWritebackController::Create() {
  return base::WrapUnique(new ZramWritebackController(
      ZramWritebackPolicy::Get(), ZramWritebackBackend::Get()));
}

ZramWritebackController::~ZramWritebackController() = default;

void ZramWritebackController::OnEnableWriteback(bool result, int64_t size_mb) {
  if (!result) {
    LOG(ERROR) << "Unable to enable zram writeback";
    return;
  }

  // Finally we can complete the policy initialization.
  CompleteInitialization(backend_->GetZramDiskSizeBytes() >> 20, size_mb);
}

void ZramWritebackController::ResetCurrentlyWritingBack() {
  current_writeback_mode_ = ZramWritebackMode::kModeNone;
  currently_writing_back_ = false;
  current_writeback_limit_ = 0;
}

void ZramWritebackController::CompleteInitialization(
    uint64_t zram_size_mb,
    uint64_t writeback_size_mb) {
  policy_->Initialize(zram_size_mb, writeback_size_mb);
  timer_.Start(FROM_HERE, policy_->GetWritebackTimerInterval(),
               base::BindRepeating(&ZramWritebackController::PeriodicWriteback,
                                   weak_factory_.GetWeakPtr()));
}

// static
bool ZramWritebackController::IsSupportedAndEnabled() {
  return ZramWritebackBackend::IsSupported() &&
         base::FeatureList::IsEnabled(kCrOSEnableZramWriteback);
}

void ZramWritebackController::Stop() {
  timer_.Stop();
}

void ZramWritebackController::Start() {
  if (backend_->WritebackAlreadyEnabled()) {
    // The fact that it was already enabled is fine, we just need to query the
    // size of the writeback device and then we can initialize the parameters of
    // the policy.
    backend_->GetCurrentBackingDevSize(
        base::BindOnce(&ZramWritebackController::OnEnableWriteback,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  backend_->EnableWriteback(
      1024, base::BindOnce(&ZramWritebackController::OnEnableWriteback,
                           weak_factory_.GetWeakPtr()));
}

void ZramWritebackController::OnWritebackComplete(ZramWritebackMode mode,
                                                  bool result) {
  if (!result) {
    ResetCurrentlyWritingBack();
    return;
  }

  // Now let's figure out how much work we actually did, even in the failure
  // case we might write back some pages.
  uint64_t cur_limit = backend_->GetCurrentWritebackLimitPages();
  current_writeback_limit_ = cur_limit;
  if (current_writeback_limit_ == 0) {
    ResetCurrentlyWritingBack();
    return;
  }

  // Move on to the next writeback phase.
  if (current_writeback_mode_ == ZramWritebackMode::kModeHugeIdle &&
      policy_->CanWritebackIdle())
    current_writeback_mode_ = ZramWritebackMode::kModeIdle;
  else if ((current_writeback_mode_ == ZramWritebackMode::kModeIdle ||
            current_writeback_mode_ == ZramWritebackMode::kModeHugeIdle) &&
           policy_->CanWritebackHuge()) {
    current_writeback_mode_ = ZramWritebackMode::kModeHuge;
  } else {
    // We're done.
    ResetCurrentlyWritingBack();
    return;
  }

  ReadyToWriteback();
}

void ZramWritebackController::ReadyToWriteback() {
  backend_->InitiateWriteback(
      current_writeback_mode_,
      base::BindOnce(&ZramWritebackController::OnWritebackComplete,
                     weak_factory_.GetWeakPtr(), current_writeback_mode_));
}

void ZramWritebackController::OnMarkIdle(bool result) {
  if (!result) {
    LOG(ERROR) << "Failed to mark pages as idle";
    ResetCurrentlyWritingBack();
    return;
  }

  current_writeback_mode_ = policy_->CanWritebackHugeIdle()
                                ? ZramWritebackMode::kModeHugeIdle
                                : ZramWritebackMode::kModeIdle;
  ReadyToWriteback();
}

void ZramWritebackController::OnSetWritebackLimit(bool result,
                                                  int64_t number_pages) {
  if (!result || !number_pages) {
    LOG(ERROR) << "Failed to set writeback limit";
    ResetCurrentlyWritingBack();
    return;
  }

  current_writeback_limit_ = number_pages;

  // We need to start by doing an idle mark sweep (if enabled).
  if (policy_->CanWritebackHugeIdle() || policy_->CanWritebackIdle()) {
    base::TimeDelta idle_age = policy_->GetCurrentWritebackIdleTime();
    if (idle_age == base::TimeDelta::Max()) {
      ResetCurrentlyWritingBack();
      return;
    }

    backend_->MarkIdle(idle_age,
                       base::BindOnce(&ZramWritebackController::OnMarkIdle,
                                      weak_factory_.GetWeakPtr()));
    return;
  }

  // If we do not allow writing back idle or huge the policy should have never
  // given us a page limit anyway.
  DCHECK(policy_->CanWritebackHuge());

  // Since we can't writeback idle, we start writing back huge pages.
  current_writeback_mode_ = ZramWritebackMode::kModeHuge;
  ReadyToWriteback();
}

void ZramWritebackController::PeriodicWriteback() {
  if (currently_writing_back_) {
    return;
  }

  currently_writing_back_ = true;
  current_writeback_limit_ = policy_->GetAllowedWritebackLimit();
  if (!current_writeback_limit_) {
    ResetCurrentlyWritingBack();
    return;
  }

  backend_->SetWritebackLimit(
      current_writeback_limit_,
      base::BindOnce(&ZramWritebackController::OnSetWritebackLimit,
                     weak_factory_.GetWeakPtr()));
}

}  // namespace ash::memory
