// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_FREEZING_CANNOT_FREEZE_REASON_H_
#define COMPONENTS_PERFORMANCE_MANAGER_FREEZING_CANNOT_FREEZE_REASON_H_

namespace performance_manager {

// List of reasons not to freeze a browsing instance.
//
// The reasons to not freeze a browsing instance overlap with the reasons to not
// discard a tab (PageDiscardingHelper::CanDiscard). We could look into ways to
// share logic.
enum class CannotFreezeReason {
  kVisible = 0,
  kRecentlyVisible,
  kAudible,
  kRecentlyAudible,
  kHoldingWebLock,
  kHoldingIndexedDBLock,
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
};

const char* CannotFreezeReasonToString(CannotFreezeReason reason);

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_FREEZING_CANNOT_FREEZE_REASON_H_
