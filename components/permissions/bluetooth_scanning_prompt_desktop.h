// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_BLUETOOTH_SCANNING_PROMPT_DESKTOP_H_
#define COMPONENTS_PERMISSIONS_BLUETOOTH_SCANNING_PROMPT_DESKTOP_H_

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/bluetooth_scanning_prompt.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace permissions {

class BluetoothScanningPromptController;
class ChooserController;

// Represents a Bluetooth scanning prompt to ask the user permission to
// allow a site to receive Bluetooth advertisement packets from Bluetooth
// devices. This implementation is for desktop.
class BluetoothScanningPromptDesktop : public content::BluetoothScanningPrompt {
 public:
  // The OnceClosure returned by |show_dialog_callback| can be invoked to close
  // the dialog. It should be a no-op to invoke the closure if the dialog has
  // already been closed by the user.
  BluetoothScanningPromptDesktop(
      content::RenderFrameHost* frame,
      const content::BluetoothScanningPrompt::EventHandler& event_handler,
      std::u16string title,
      base::OnceCallback<
          base::OnceClosure(std::unique_ptr<permissions::ChooserController>)>
          show_dialog_callback);

  BluetoothScanningPromptDesktop(const BluetoothScanningPromptDesktop&) =
      delete;
  BluetoothScanningPromptDesktop& operator=(
      const BluetoothScanningPromptDesktop&) = delete;

  ~BluetoothScanningPromptDesktop() override;

  // content::BluetoothScanningPrompt:
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const std::u16string& device_name) override;

 private:
  // DeviceChooserContentView owns the controller.
  base::WeakPtr<BluetoothScanningPromptController>
      bluetooth_scanning_prompt_controller_;

  // Closes the displayed UI, if there is one. This is used to ensure the UI
  // closes if this controller is destroyed.
  base::ScopedClosureRunner close_closure_runner_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_BLUETOOTH_SCANNING_PROMPT_DESKTOP_H_
