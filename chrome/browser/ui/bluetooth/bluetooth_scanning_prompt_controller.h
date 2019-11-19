// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_SCANNING_PROMPT_CONTROLLER_H_
#define CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_SCANNING_PROMPT_CONTROLLER_H_

#include <stddef.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "content/public/browser/bluetooth_scanning_prompt.h"

// BluetoothScanningPromptController is a prompt that presents a list of
// Bluetooth device names. It can be used by Bluetooth Scanning API to
// show example nearby Bluetooth devices to user. It is owned by
// ChooserBubbleDelegate.
class BluetoothScanningPromptController : public ChooserController {
 public:
  BluetoothScanningPromptController(
      content::RenderFrameHost* owner,
      const content::BluetoothScanningPrompt::EventHandler& event_handler);
  ~BluetoothScanningPromptController() override;

  // ChooserController:
  bool ShouldShowHelpButton() const override;
  base::string16 GetNoOptionsText() const override;
  base::string16 GetOkButtonLabel() const override;
  base::string16 GetCancelButtonLabel() const override;
  bool BothButtonsAlwaysEnabled() const override;
  bool TableViewAlwaysDisabled() const override;
  size_t NumOptions() const override;
  base::string16 GetOption(size_t index) const override;
  void Select(const std::vector<size_t>& indices) override;
  void Cancel() override;
  void Close() override;
  void OpenHelpCenterUrl() const override;

  // Shows a new device in the permission prompt or updates its information.
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const base::string16& device_name);

  // Called when |event_handler_| is no longer valid and should not be used
  // any more.
  void ResetEventHandler();

 private:
  std::vector<std::string> device_ids_;
  std::unordered_map<std::string, base::string16> device_id_to_name_map_;
  // Maps from device name to number of devices with that name.
  std::unordered_map<base::string16, int> device_name_counts_;

  content::BluetoothScanningPrompt::EventHandler event_handler_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothScanningPromptController);
};

#endif  // CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_SCANNING_PROMPT_CONTROLLER_H_
