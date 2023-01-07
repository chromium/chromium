// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/bluetooth_scanning_prompt_desktop.h"

#include "components/permissions/bluetooth_scanning_prompt_controller.h"

namespace permissions {

BluetoothScanningPromptDesktop::BluetoothScanningPromptDesktop(
    content::RenderFrameHost* frame,
    const content::BluetoothScanningPrompt::EventHandler& event_handler,
    std::u16string title,
    base::OnceCallback<
        base::OnceClosure(std::unique_ptr<permissions::ChooserController>)>
        show_dialog_callback) {
  auto controller =
      std::make_unique<permissions::BluetoothScanningPromptController>(
          frame, event_handler, title);
  bluetooth_scanning_prompt_controller_ = controller->GetWeakPtr();
  close_closure_runner_.ReplaceClosure(
      std::move(show_dialog_callback).Run(std::move(controller)));
}

BluetoothScanningPromptDesktop::~BluetoothScanningPromptDesktop() {
  // This satisfies the WebContentsDelegate::ShowBluetoothScanningPrompt()
  // requirement that the EventHandler can be destroyed any time after the
  // BluetoothScanningPrompt instance.
  if (bluetooth_scanning_prompt_controller_)
    bluetooth_scanning_prompt_controller_->ResetEventHandler();
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

}  // namespace permissions
