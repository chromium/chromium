// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/fake_bluetooth_chooser_controller.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace permissions {

FakeBluetoothChooserController::FakeBluetoothChooserController(
    std::vector<FakeDevice> devices)
    : ChooserController(
          l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_PROMPT,
                                     u"example.com")),
      devices_(std::move(devices)) {}

FakeBluetoothChooserController::~FakeBluetoothChooserController() = default;

bool FakeBluetoothChooserController::ShouldShowIconBeforeText() const {
  return true;
}

bool FakeBluetoothChooserController::ShouldShowReScanButton() const {
  return true;
}

std::u16string FakeBluetoothChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

std::u16string FakeBluetoothChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_PAIR_BUTTON_TEXT);
}

std::pair<std::u16string, std::u16string>
FakeBluetoothChooserController::GetThrobberLabelAndTooltip() const {
  return {
      l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING_LABEL),
      l10n_util::GetStringUTF16(
          IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING_LABEL_TOOLTIP)};
}

bool FakeBluetoothChooserController::TableViewAlwaysDisabled() const {
  return table_view_always_disabled_;
}

size_t FakeBluetoothChooserController::NumOptions() const {
  return devices_.size();
}

int FakeBluetoothChooserController::GetSignalStrengthLevel(size_t index) const {
  return devices_.at(index).signal_strength;
}

std::u16string FakeBluetoothChooserController::GetOption(size_t index) const {
  return base::ASCIIToUTF16(devices_.at(index).name);
}

bool FakeBluetoothChooserController::IsConnected(size_t index) const {
  return devices_.at(index).connected;
}

bool FakeBluetoothChooserController::IsPaired(size_t index) const {
  return devices_.at(index).paired;
}

bool FakeBluetoothChooserController::ShouldShowAdapterOffView() const {
  return true;
}

int FakeBluetoothChooserController::GetAdapterOffMessageId() const {
  return IDS_BLUETOOTH_DEVICE_CHOOSER_ADAPTER_OFF;
}

int FakeBluetoothChooserController::GetTurnAdapterOnLinkTextMessageId() const {
  return IDS_BLUETOOTH_DEVICE_CHOOSER_TURN_ON_BLUETOOTH_LINK_TEXT;
}

bool FakeBluetoothChooserController::ShouldShowAdapterUnauthorizedView() const {
  return true;
}

int FakeBluetoothChooserController::GetBluetoothUnauthorizedMessageId() const {
  return IDS_BLUETOOTH_DEVICE_CHOOSER_AUTHORIZE_BLUETOOTH;
}

int FakeBluetoothChooserController::GetAuthorizeBluetoothLinkTextMessageId()
    const {
  return IDS_BLUETOOTH_DEVICE_CHOOSER_AUTHORIZE_BLUETOOTH_LINK_TEXT;
}

void FakeBluetoothChooserController::SetBluetoothStatus(
    BluetoothStatus status) {
  const bool available = status != BluetoothStatus::UNAVAILABLE;
  view()->OnAdapterEnabledChanged(available);
  if (available)
    view()->OnRefreshStateChanged(status == BluetoothStatus::SCANNING);
}

void FakeBluetoothChooserController::SetBluetoothPermission(
    bool has_permission) {
  view()->OnAdapterAuthorizationChanged(has_permission);
}

void FakeBluetoothChooserController::AddDevice(FakeDevice device) {
  devices_.push_back(device);
  view()->OnOptionAdded(devices_.size() - 1);
}

void FakeBluetoothChooserController::RemoveDevice(size_t index) {
  DCHECK_GT(devices_.size(), index);
  devices_.erase(devices_.begin() + index);
  view()->OnOptionRemoved(index);
}

void FakeBluetoothChooserController::UpdateDevice(size_t index,
                                                  FakeDevice new_device) {
  DCHECK_GT(devices_.size(), index);
  devices_[index] = new_device;
  view()->OnOptionUpdated(index);
}

}  // namespace permissions
