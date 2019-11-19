// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/bluetooth_scanning_prompt_desktop.h"

#include "base/logging.h"
#include "chrome/browser/ui/bluetooth/bluetooth_scanning_prompt_controller.h"
#include "components/bubble/bubble_controller.h"

BluetoothScanningPromptDesktop::BluetoothScanningPromptDesktop(
    BluetoothScanningPromptController* bluetooth_scanning_prompt_controller)
    : bluetooth_scanning_prompt_controller_(
          bluetooth_scanning_prompt_controller) {
  DCHECK(bluetooth_scanning_prompt_controller_);
}

BluetoothScanningPromptDesktop::~BluetoothScanningPromptDesktop() {
  // This satisfies the WebContentsDelegate::ShowBluetoothScanningPrompt()
  // requirement that the EventHandler can be destroyed any time after the
  // BluetoothScanningPrompt instance.
  bluetooth_scanning_prompt_controller_->ResetEventHandler();
  if (bubble_)
    bubble_->CloseBubble(BUBBLE_CLOSE_FORCED);
}

void BluetoothScanningPromptDesktop::AddOrUpdateDevice(
    const std::string& device_id,
    bool should_update_name,
    const base::string16& device_name) {
  bluetooth_scanning_prompt_controller_->AddOrUpdateDevice(
      device_id, should_update_name, device_name);
}
