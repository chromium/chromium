// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_SCANNING_PROMPT_DESKTOP_H_
#define CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_SCANNING_PROMPT_DESKTOP_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/bluetooth_scanning_prompt.h"

class BluetoothScanningPromptController;

namespace content {
class RenderFrameHost;
}  // namespace content

// Represents a Bluetooth scanning prompt to ask the user permission to
// allow a site to receive Bluetooth advertisement packets from Bluetooth
// devices. This implementation is for desktop.
class BluetoothScanningPromptDesktop : public content::BluetoothScanningPrompt {
 public:
  explicit BluetoothScanningPromptDesktop(
      content::RenderFrameHost* frame,
      const content::BluetoothScanningPrompt::EventHandler& event_handler);
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
  base::OnceClosure close_closure_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothScanningPromptDesktop);
};

#endif  // CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_SCANNING_PROMPT_DESKTOP_H_
