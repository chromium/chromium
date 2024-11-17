// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/bluetooth_scanning_prompt_controller.h"

#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace permissions {

BluetoothScanningPromptController::BluetoothScanningPromptController(
    content::RenderFrameHost* owner,
    const content::BluetoothScanningPrompt::EventHandler& event_handler,
    std::u16string title)
    : ChooserController(title), event_handler_(event_handler) {}

BluetoothScanningPromptController::~BluetoothScanningPromptController() =
    default;

bool BluetoothScanningPromptController::ShouldShowHelpButton() const {
  return false;
}

std::u16string BluetoothScanningPromptController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_SCANNING_PROMPT_NO_DEVICES_FOUND_PROMPT);
}

std::u16string BluetoothScanningPromptController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_SCANNING_PROMPT_ALLOW_BUTTON_TEXT);
}

std::u16string BluetoothScanningPromptController::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_SCANNING_PROMPT_BLOCK_BUTTON_TEXT);
}

std::pair<std::u16string, std::u16string>
BluetoothScanningPromptController::GetThrobberLabelAndTooltip() const {
  return {
      l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING_LABEL),
      l10n_util::GetStringUTF16(
          IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING_LABEL_TOOLTIP)};
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

std::u16string BluetoothScanningPromptController::GetOption(
    size_t index) const {
  DCHECK_LT(index, device_ids_.size());
  const std::string& device_id = device_ids_[index];
  const auto& device_name_it = device_id_to_name_map_.find(device_id);
  CHECK(device_name_it != device_id_to_name_map_.end(),
        base::NotFatalUntil::M130);
  const auto& it = device_name_counts_.find(device_name_it->second);
  CHECK(it != device_name_counts_.end(), base::NotFatalUntil::M130);
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
    const std::u16string& device_name) {
  std::u16string device_name_for_display = device_name;
  if (device_name_for_display.empty()) {
    device_name_for_display = l10n_util::GetStringFUTF16(
        IDS_BLUETOOTH_SCANNING_DEVICE_UNKNOWN, base::UTF8ToUTF16(device_id));
  }

  auto name_it = device_id_to_name_map_.find(device_id);
  if (name_it != device_id_to_name_map_.end()) {
    if (should_update_name) {
      std::u16string previous_device_name = name_it->second;
      name_it->second = device_name_for_display;

      const auto& it = device_name_counts_.find(previous_device_name);
      CHECK(it != device_name_counts_.end(), base::NotFatalUntil::M130);
      DCHECK_GT(it->second, 0);

      if (--(it->second) == 0)
        device_name_counts_.erase(it);

      ++device_name_counts_[device_name_for_display];
    }

    auto device_id_it = base::ranges::find(device_ids_, device_id);

    CHECK(device_id_it != device_ids_.end(), base::NotFatalUntil::M130);
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

base::WeakPtr<BluetoothScanningPromptController>
BluetoothScanningPromptController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace permissions
