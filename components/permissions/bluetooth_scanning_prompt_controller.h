// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_BLUETOOTH_SCANNING_PROMPT_CONTROLLER_H_
#define COMPONENTS_PERMISSIONS_BLUETOOTH_SCANNING_PROMPT_CONTROLLER_H_

#include <stddef.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/permissions/chooser_controller.h"
#include "content/public/browser/bluetooth_scanning_prompt.h"

namespace content {
class RenderFrameHost;
}

namespace permissions {

// BluetoothScanningPromptController is a prompt that presents a list of
// Bluetooth device names. It can be used by Bluetooth Scanning API to
// show example nearby Bluetooth devices to user.
class BluetoothScanningPromptController : public ChooserController {
 public:
  BluetoothScanningPromptController(
      content::RenderFrameHost* owner,
      const content::BluetoothScanningPrompt::EventHandler& event_handler,
      std::u16string title);

  BluetoothScanningPromptController(const BluetoothScanningPromptController&) =
      delete;
  BluetoothScanningPromptController& operator=(
      const BluetoothScanningPromptController&) = delete;

  ~BluetoothScanningPromptController() override;

  // ChooserController:
  bool ShouldShowHelpButton() const override;
  std::u16string GetNoOptionsText() const override;
  std::u16string GetOkButtonLabel() const override;
  std::u16string GetCancelButtonLabel() const override;
  std::pair<std::u16string, std::u16string> GetThrobberLabelAndTooltip()
      const override;
  bool BothButtonsAlwaysEnabled() const override;
  bool TableViewAlwaysDisabled() const override;
  size_t NumOptions() const override;
  std::u16string GetOption(size_t index) const override;
  void Select(const std::vector<size_t>& indices) override;
  void Cancel() override;
  void Close() override;
  void OpenHelpCenterUrl() const override;

  // Shows a new device in the permission prompt or updates its information.
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const std::u16string& device_name);

  // Called when |event_handler_| is no longer valid and should not be used
  // any more.
  void ResetEventHandler();

  // Get a weak pointer to this controller.
  base::WeakPtr<BluetoothScanningPromptController> GetWeakPtr();

 private:
  std::vector<std::string> device_ids_;
  std::unordered_map<std::string, std::u16string> device_id_to_name_map_;
  // Maps from device name to number of devices with that name.
  std::unordered_map<std::u16string, int> device_name_counts_;

  content::BluetoothScanningPrompt::EventHandler event_handler_;

  base::WeakPtrFactory<BluetoothScanningPromptController> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_BLUETOOTH_SCANNING_PROMPT_CONTROLLER_H_
