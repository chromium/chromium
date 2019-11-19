// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/bluetooth_scanning_prompt_controller.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

BluetoothScanningPromptController::BluetoothScanningPromptController(
    content::RenderFrameHost* owner,
    const content::BluetoothScanningPrompt::EventHandler& event_handler)
    : ChooserController(owner,
                        IDS_BLUETOOTH_SCANNING_PROMPT_ORIGIN,
                        IDS_BLUETOOTH_SCANNING_PROMPT_ORIGIN),
      event_handler_(event_handler) {}

BluetoothScanningPromptController::~BluetoothScanningPromptController() {}

bool BluetoothScanningPromptController::ShouldShowHelpButton() const {
  return false;
}

base::string16 BluetoothScanningPromptController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_SCANNING_PROMPT_NO_DEVICES_FOUND_PROMPT);
}

base::string16 BluetoothScanningPromptController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_SCANNING_PROMPT_ALLOW_BUTTON_TEXT);
}

base::string16 BluetoothScanningPromptController::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_SCANNING_PROMPT_BLOCK_BUTTON_TEXT);
}

bool BluetoothScanningPromptController::BothButtonsAlwaysEnabled() const {
  return true;
}

bool BluetoothScanningPromptController::TableViewAlwaysDisabled() const {
  return true;
}

size_t BluetoothScanningPromptController::NumOptions() const {
  return device_ids_.size();
}

base::string16 BluetoothScanningPromptController::GetOption(
    size_t index) const {
  DCHECK_LT(index, device_ids_.size());
  const std::string& device_id = device_ids_[index];
  const auto& device_name_it = device_id_to_name_map_.find(device_id);
  DCHECK(device_name_it != device_id_to_name_map_.end());
  const auto& it = device_name_counts_.find(device_name_it->second);
  DCHECK(it != device_name_counts_.end());
  return it->second == 1
             ? device_name_it->second
             : l10n_util::GetStringFUTF16(
                   IDS_DEVICE_CHOOSER_DEVICE_NAME_WITH_ID,
                   device_name_it->second, base::UTF8ToUTF16(device_id));
}

void BluetoothScanningPromptController::Select(
    const std::vector<size_t>& indices) {
  DCHECK(indices.empty());

  if (event_handler_.is_null())
    return;

  event_handler_.Run(content::BluetoothScanningPrompt::Event::kAllow);
}

void BluetoothScanningPromptController::Cancel() {
  if (event_handler_.is_null())
    return;

  event_handler_.Run(content::BluetoothScanningPrompt::Event::kBlock);
}

void BluetoothScanningPromptController::Close() {
  if (event_handler_.is_null())
    return;

  event_handler_.Run(content::BluetoothScanningPrompt::Event::kCanceled);
}

void BluetoothScanningPromptController::OpenHelpCenterUrl() const {}

void BluetoothScanningPromptController::AddOrUpdateDevice(
    const std::string& device_id,
    bool should_update_name,
    const base::string16& device_name) {
  base::string16 device_name_for_display = device_name;
  if (device_name_for_display.empty()) {
    device_name_for_display = l10n_util::GetStringFUTF16(
        IDS_BLUETOOTH_SCANNING_DEVICE_UNKNOWN, base::UTF8ToUTF16(device_id));
  }

  auto name_it = device_id_to_name_map_.find(device_id);
  if (name_it != device_id_to_name_map_.end()) {
    if (should_update_name) {
      base::string16 previous_device_name = name_it->second;
      name_it->second = device_name_for_display;

      const auto& it = device_name_counts_.find(previous_device_name);
      DCHECK(it != device_name_counts_.end());
      DCHECK_GT(it->second, 0);

      if (--(it->second) == 0)
        device_name_counts_.erase(it);

      ++device_name_counts_[device_name_for_display];
    }

    auto device_id_it =
        std::find(device_ids_.begin(), device_ids_.end(), device_id);

    DCHECK(device_id_it != device_ids_.end());
    if (view())
      view()->OnOptionUpdated(device_id_it - device_ids_.begin());
    return;
  }

  device_ids_.push_back(device_id);
  device_id_to_name_map_.insert({device_id, device_name_for_display});
  ++device_name_counts_[device_name_for_display];
  if (view())
    view()->OnOptionAdded(device_ids_.size() - 1);
}

void BluetoothScanningPromptController::ResetEventHandler() {
  event_handler_.Reset();
}
