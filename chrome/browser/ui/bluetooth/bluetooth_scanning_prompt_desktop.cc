// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/bluetooth_scanning_prompt_desktop.h"

#include "base/check.h"
#include "chrome/browser/ui/bluetooth/bluetooth_scanning_prompt_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"

BluetoothScanningPromptDesktop::BluetoothScanningPromptDesktop(
    content::RenderFrameHost* frame,
    const content::BluetoothScanningPrompt::EventHandler& event_handler) {
  auto controller =
      std::make_unique<BluetoothScanningPromptController>(frame, event_handler);
  bluetooth_scanning_prompt_controller_ = controller->GetWeakPtr();
  close_closure_ =
      chrome::ShowDeviceChooserDialog(frame, std::move(controller));
}

BluetoothScanningPromptDesktop::~BluetoothScanningPromptDesktop() {
  // This satisfies the WebContentsDelegate::ShowBluetoothScanningPrompt()
  // requirement that the EventHandler can be destroyed any time after the
  // BluetoothScanningPrompt instance.
  if (bluetooth_scanning_prompt_controller_)
    bluetooth_scanning_prompt_controller_->ResetEventHandler();
  if (close_closure_)
    std::move(close_closure_).Run();
}

void BluetoothScanningPromptDesktop::AddOrUpdateDevice(
    const std::string& device_id,
    bool should_update_name,
    const std::u16string& device_name) {
  if (bluetooth_scanning_prompt_controller_) {
    bluetooth_scanning_prompt_controller_->AddOrUpdateDevice(
        device_id, should_update_name, device_name);
  }
}
