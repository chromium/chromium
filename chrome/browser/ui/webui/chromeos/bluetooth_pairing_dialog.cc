// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/bluetooth_pairing_dialog.h"

#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/bluetooth_dialog_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/bluetooth_pairing_dialog_resources.h"
#include "chrome/grit/bluetooth_pairing_dialog_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

constexpr int kBluetoothPairingDialogHeight = 375;

void AddBluetoothStrings(content::WebUIDataSource* html_source) {
  struct {
    const char* name;
    int id;
  } localized_strings[] = {
      {"bluetoothPairDeviceTitle", IDS_SETTINGS_BLUETOOTH_PAIR_DEVICE_TITLE},
      {"ok", IDS_OK},
      {"cancel", IDS_CANCEL},
      {"close", IDS_CLOSE},
  };
  for (const auto& entry : localized_strings)
    html_source->AddLocalizedString(entry.name, entry.id);
  chromeos::bluetooth_dialog::AddLocalizedStrings(html_source);
}

}  // namespace

// static
SystemWebDialogDelegate* BluetoothPairingDialog::ShowDialog(
    const std::string& address,
    const std::u16string& name_for_display,
    bool paired,
    bool connected) {
  std::string cannonical_address =
      device::CanonicalizeBluetoothAddress(address);
  if (cannonical_address.empty()) {
    LOG(ERROR) << "BluetoothPairingDialog: Invalid address: " << address;
    return nullptr;
  }
  auto* instance = SystemWebDialogDelegate::FindInstance(cannonical_address);
  if (instance) {
    instance->Focus();
    return instance;
  }

  BluetoothPairingDialog* dialog = new BluetoothPairingDialog(
      cannonical_address, name_for_display, paired, connected);
  dialog->ShowSystemDialog();
  return dialog;
}

BluetoothPairingDialog::BluetoothPairingDialog(
    const std::string& address,
    const std::u16string& name_for_display,
    bool paired,
    bool connected)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIBluetoothPairingURL),
                              std::u16string() /* title */),
      address_(address) {
  device_data_.SetString("address", address);
  device_data_.SetString("name", name_for_display);
  device_data_.SetBoolean("paired", paired);
  device_data_.SetBoolean("connected", connected);
}

BluetoothPairingDialog::~BluetoothPairingDialog() = default;

const std::string& BluetoothPairingDialog::Id() {
  return address_;
}

void BluetoothPairingDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(SystemWebDialogDelegate::kDialogWidth,
                kBluetoothPairingDialogHeight);
}

std::string BluetoothPairingDialog::GetDialogArgs() const {
  std::string data;
  base::JSONWriter::Write(device_data_, &data);
  return data;
}

// BluetoothPairingUI

BluetoothPairingDialogUI::BluetoothPairingDialogUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIBluetoothPairingHost);

  AddBluetoothStrings(source);
  source->AddLocalizedString("title", IDS_SETTINGS_BLUETOOTH_PAIR_DEVICE_TITLE);
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kBluetoothPairingDialogResources,
                      kBluetoothPairingDialogResourcesSize),
      IDR_BLUETOOTH_PAIRING_DIALOG_BLUETOOTH_PAIRING_DIALOG_CONTAINER_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

BluetoothPairingDialogUI::~BluetoothPairingDialogUI() = default;

}  // namespace chromeos
