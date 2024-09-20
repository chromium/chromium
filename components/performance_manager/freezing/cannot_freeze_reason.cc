// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/cannot_freeze_reason.h"

namespace performance_manager {

const char* CannotFreezeReasonToString(CannotFreezeReason reason) {
  switch (reason) {
    case CannotFreezeReason::kVisible:
      return "visible";
    case CannotFreezeReason::kRecentlyVisible:
      return "recently visible";
    case CannotFreezeReason::kAudible:
      return "audible";
    case CannotFreezeReason::kRecentlyAudible:
      return "recently audible";
    case CannotFreezeReason::kHoldingWebLock:
      return "holding Web Lock";
    case CannotFreezeReason::kHoldingIndexedDBLock:
      return "holding IndexedDB lock";
    case CannotFreezeReason::kConnectedToUsbDevice:
      return "connected to USB device";
    case CannotFreezeReason::kConnectedToBluetoothDevice:
      return "connected to Bluetooth device";
    case CannotFreezeReason::kConnectedToHidDevice:
      return "connected to HID device";
    case CannotFreezeReason::kConnectedToSerialPort:
      return "connected to serial port";
    case CannotFreezeReason::kCapturingVideo:
      return "capturing video";
    case CannotFreezeReason::kCapturingAudio:
      return "capturing audio";
    case CannotFreezeReason::kBeingMirrored:
      return "being mirrored";
    case CannotFreezeReason::kCapturingWindow:
      return "capturing window";
    case CannotFreezeReason::kCapturingDisplay:
      return "capturing display";
    case CannotFreezeReason::kWebRTC:
      return "has an active WebRTC connection";
    case CannotFreezeReason::kLoading:
      return "loading";
  }
}

}  // namespace performance_manager
