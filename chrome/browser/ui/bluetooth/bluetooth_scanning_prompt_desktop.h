// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_SCANNING_PROMPT_DESKTOP_H_
#define CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_SCANNING_PROMPT_DESKTOP_H_

#include "base/macros.h"
#include "components/bubble/bubble_reference.h"
#include "content/public/browser/bluetooth_scanning_prompt.h"

class BluetoothScanningPromptController;

// Represents a Bluetooth scanning prompt to ask the user permission to
// allow a site to receive Bluetooth advertisement packets from Bluetooth
// devices. This implementation is for desktop.
class BluetoothScanningPromptDesktop : public content::BluetoothScanningPrompt {
 public:
  explicit BluetoothScanningPromptDesktop(
      BluetoothScanningPromptController* bluetooth_scanning_prompt_controller);
  ~BluetoothScanningPromptDesktop() override;

  // content::BluetoothScanningPrompt:
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const base::string16& device_name) override;

  // Sets a reference to the bubble being displayed so that it can be closed if
  // this object is destroyed.
  void set_bubble(base::WeakPtr<BubbleController> bubble) {
    bubble_ = std::move(bubble);
  }

 private:
  // Weak. DeviceChooserContentView owns it.
  BluetoothScanningPromptController* bluetooth_scanning_prompt_controller_;

  base::WeakPtr<BubbleController> bubble_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothScanningPromptDesktop);
};

#endif  // CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_SCANNING_PROMPT_DESKTOP_H_
