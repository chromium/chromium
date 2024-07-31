// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/bluetooth_chooser_controller.h"

#include "base/check_op.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace permissions {

BluetoothChooserController::BluetoothChooserController(
    content::RenderFrameHost* owner,
    const content::BluetoothChooser::EventHandler& event_handler,
    std::u16string title)
    : ChooserController(title), event_handler_(event_handler) {}

BluetoothChooserController::~BluetoothChooserController() {
  if (event_handler_) {
    event_handler_.Run(content::BluetoothChooserEvent::CANCELLED,
                       std::string());
  }
}

bool BluetoothChooserController::ShouldShowIconBeforeText() const {
  return true;
}

bool BluetoothChooserController::ShouldShowReScanButton() const {
  return true;
}

std::u16string BluetoothChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

std::u16string BluetoothChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_PAIR_BUTTON_TEXT);
}

std::pair<std::u16string, std::u16string>
BluetoothChooserController::GetThrobberLabelAndTooltip() const {
  return {
      l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING_LABEL),
      l10n_util::GetStringUTF16(
          IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING_LABEL_TOOLTIP)};
}

size_t BluetoothChooserController::NumOptions() const {
  return devices_.size();
}

int BluetoothChooserController::GetSignalStrengthLevel(size_t index) const {
  return devices_[index].signal_strength_level;
}

bool BluetoothChooserController::IsConnected(size_t index) const {
  return devices_[index].is_connected;
}

bool BluetoothChooserController::IsPaired(size_t index) const {
  return devices_[index].is_paired;
}

std::u16string BluetoothChooserController::GetOption(size_t index) const {
  DCHECK_LT(index, devices_.size());
  const std::string& device_id = devices_[index].id;
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

void BluetoothChooserController::RefreshOptions() {
  if (event_handler_.is_null())
    return;
  ClearAllDevices();
  event_handler_.Run(content::BluetoothChooserEvent::RESCAN, std::string());
}

void BluetoothChooserController::Select(const std::vector<size_t>& indices) {
  DCHECK_EQ(1u, indices.size());
  size_t index = indices[0];
  if (event_handler_.is_null()) {
    return;
  }
  DCHECK_LT(index, devices_.size());
  event_handler_.Run(content::BluetoothChooserEvent::SELECTED,
                     devices_[index].id);
  event_handler_.Reset();
}

void BluetoothChooserController::Cancel() {
  if (event_handler_.is_null())
    return;
  event_handler_.Run(content::BluetoothChooserEvent::CANCELLED, std::string());
  event_handler_.Reset();
}

void BluetoothChooserController::Close() {
  if (event_handler_.is_null())
    return;
  event_handler_.Run(content::BluetoothChooserEvent::CANCELLED, std::string());
  event_handler_.Reset();
}

void BluetoothChooserController::OnAdapterPresenceChanged(
    content::BluetoothChooser::AdapterPresence presence) {
  ClearAllDevices();
  switch (presence) {
    case content::BluetoothChooser::AdapterPresence::ABSENT:
      NOTREACHED_IN_MIGRATION();
      break;
    case content::BluetoothChooser::AdapterPresence::POWERED_OFF:
      if (view()) {
        view()->OnAdapterEnabledChanged(
            false /* Bluetooth adapter is turned off */);
      }
      break;
    case content::BluetoothChooser::AdapterPresence::POWERED_ON:
      if (view()) {
        view()->OnAdapterEnabledChanged(
            true /* Bluetooth adapter is turned on */);
      }
      break;
    case content::BluetoothChooser::AdapterPresence::UNAUTHORIZED:
      if (view()) {
        view()->OnAdapterAuthorizationChanged(/*authorized=*/false);
      }
      break;
  }
}

void BluetoothChooserController::OnDiscoveryStateChanged(
    content::BluetoothChooser::DiscoveryState state) {
  switch (state) {
    case content::BluetoothChooser::DiscoveryState::DISCOVERING:
      if (view()) {
        view()->OnRefreshStateChanged(
            true /* Refreshing options is in progress */);
      }
      break;
    case content::BluetoothChooser::DiscoveryState::IDLE:
    case content::BluetoothChooser::DiscoveryState::FAILED_TO_START:
      if (view()) {
        view()->OnRefreshStateChanged(
            false /* Refreshing options is complete */);
      }
      break;
  }
}

void BluetoothChooserController::AddOrUpdateDevice(
    const std::string& device_id,
    bool should_update_name,
    const std::u16string& device_name,
    bool is_gatt_connected,
    bool is_paired,
    int signal_strength_level) {
  auto name_it = device_id_to_name_map_.find(device_id);
  if (name_it != device_id_to_name_map_.end()) {
    if (should_update_name) {
      std::u16string previous_device_name = name_it->second;
      name_it->second = device_name;

      const auto& it = device_name_counts_.find(previous_device_name);
      CHECK(it != device_name_counts_.end(), base::NotFatalUntil::M130);
      DCHECK_GT(it->second, 0);

      if (--(it->second) == 0)
        device_name_counts_.erase(it);

      ++device_name_counts_[device_name];
    }

    auto device_it =
        base::ranges::find(devices_, device_id, &BluetoothDeviceInfo::id);

    CHECK(device_it != devices_.end(), base::NotFatalUntil::M130);
    // When Bluetooth device scanning stops, the |signal_strength_level|
    // is -1, and in this case, should still use the previously stored
    // signal strength level value.
    if (signal_strength_level != -1)
      device_it->signal_strength_level = signal_strength_level;
    device_it->is_connected = is_gatt_connected;
    device_it->is_paired = is_paired;
    if (view())
      view()->OnOptionUpdated(device_it - devices_.begin());
    return;
  }

  devices_.push_back(
      {device_id, signal_strength_level, is_gatt_connected, is_paired});
  device_id_to_name_map_.insert({device_id, device_name});
  ++device_name_counts_[device_name];
  if (view())
    view()->OnOptionAdded(devices_.size() - 1);
}

void BluetoothChooserController::RemoveDevice(const std::string& device_id) {
  const auto& name_it = device_id_to_name_map_.find(device_id);
  if (name_it == device_id_to_name_map_.end())
    return;

  auto device_it =
      base::ranges::find(devices_, device_id, &BluetoothDeviceInfo::id);

  if (device_it != devices_.end()) {
    size_t index = device_it - devices_.begin();
    devices_.erase(device_it);

    const auto& it = device_name_counts_.find(name_it->second);
    CHECK(it != device_name_counts_.end(), base::NotFatalUntil::M130);
    DCHECK_GT(it->second, 0);

    if (--(it->second) == 0)
      device_name_counts_.erase(it);

    device_id_to_name_map_.erase(name_it);

    if (view())
      view()->OnOptionRemoved(index);
  }
}

void BluetoothChooserController::ResetEventHandler() {
  event_handler_.Reset();
}

base::WeakPtr<BluetoothChooserController>
BluetoothChooserController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void BluetoothChooserController::ClearAllDevices() {
  devices_.clear();
  device_id_to_name_map_.clear();
  device_name_counts_.clear();
}

bool BluetoothChooserController::ShouldShowAdapterOffView() const {
  return true;
}

int BluetoothChooserController::GetAdapterOffMessageId() const {
  return IDS_BLUETOOTH_DEVICE_CHOOSER_ADAPTER_OFF;
}

int BluetoothChooserController::GetTurnAdapterOnLinkTextMessageId() const {
  return IDS_BLUETOOTH_DEVICE_CHOOSER_TURN_ON_BLUETOOTH_LINK_TEXT;
}

bool BluetoothChooserController::ShouldShowAdapterUnauthorizedView() const {
  return true;
}

int BluetoothChooserController::GetBluetoothUnauthorizedMessageId() const {
  return IDS_BLUETOOTH_DEVICE_CHOOSER_AUTHORIZE_BLUETOOTH;
}

int BluetoothChooserController::GetAuthorizeBluetoothLinkTextMessageId() const {
  return IDS_BLUETOOTH_DEVICE_CHOOSER_AUTHORIZE_BLUETOOTH_LINK_TEXT;
}

}  // namespace permissions
