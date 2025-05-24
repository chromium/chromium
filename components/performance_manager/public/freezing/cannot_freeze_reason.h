// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FREEZING_CANNOT_FREEZE_REASON_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FREEZING_CANNOT_FREEZE_REASON_H_

#include "base/containers/enum_set.h"

namespace performance_manager::freezing {

// List of reasons not to freeze a browsing instance.
//
// The reasons to not freeze a browsing instance overlap with the reasons to not
// discard a tab (DiscardEligibilityPolicy::CanDiscard). We could look into ways
// to share logic.
enum class CannotFreezeReason {
  kVisible = 0,
  kMin = kVisible,  // Lower bound for EnumSet.
  kRecentlyVisible,
  kAudible,
  kRecentlyAudible,
  kFreezingOriginTrialOptOut,
  kHoldingWebLock,
  kHoldingIndexedDBLock,
  kHoldingBlockingIndexedDBLock,
  kConnectedToUsbDevice,
  kConnectedToBluetoothDevice,
  kConnectedToHidDevice,
  kConnectedToSerialPort,
  kCapturingVideo,
  kCapturingAudio,
  kBeingMirrored,
  kCapturingWindow,
  kCapturingDisplay,
  kWebRTC,
  kLoading,
  kNotificationPermission,
  kOptedOut,
  kMostRecentlyUsed,
  kMax = kMostRecentlyUsed,  // Upper bound for EnumSet.
};

using CannotFreezeReasonSet = base::EnumSet<CannotFreezeReason,
                                            CannotFreezeReason::kMin,
                                            CannotFreezeReason::kMax>;

const char* CannotFreezeReasonToString(CannotFreezeReason reason);

}  // namespace performance_manager::freezing

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FREEZING_CANNOT_FREEZE_REASON_H_
