// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"

#include "base/values.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/screens/network_screen.h"
#include "chrome/browser/ui/webui/ash/cellular_setup/cellular_setup_localized_strings_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/strings/network/network_element_localized_strings_provider.h"

namespace ash {

NetworkScreenHandler::NetworkScreenHandler() : BaseScreenHandler(kScreenId) {}

NetworkScreenHandler::~NetworkScreenHandler() = default;

void NetworkScreenHandler::ShowScreenWithData(base::Value::Dict data) {
  ShowInWebUI(std::move(data));
}

void NetworkScreenHandler::ShowError(const std::u16string& message) {
  CallExternalAPI("setError", message);
}

void NetworkScreenHandler::ClearErrors() {
  CallExternalAPI("setError", std::string());
}

void NetworkScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF("networkSectionTitle", IDS_NETWORK_SELECTION_TITLE,
                ui::GetChromeOSDeviceName());
  builder->AddF("networkSectionSubtitle", IDS_NETWORK_SELECTION_SUBTITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("proxySettingsListItemName",
               IDS_NETWORK_PROXY_SETTINGS_LIST_ITEM_NAME);
  builder->Add("addWiFiListItemName", IDS_NETWORK_ADD_WI_FI_LIST_ITEM_NAME);

  builder->Add("networkScreenQuickStart",
               IDS_LOGIN_QUICK_START_SETUP_NETWORK_SCREEN_ENTRY_POINT);

  builder->Add("networkScreenConnectingToWifiTitle",
               IDS_LOGIN_QUICK_START_WIFI_TRANSFER_TITLE);
  builder->Add("networkScreenQuickStartTransferWifiSubtitle",
               IDS_LOGIN_QUICK_START_WIFI_TRANSFER_SUBTITLE);
  builder->Add("networkScreenQuickStartWiFiErrorTitle",
               IDS_LOGIN_QUICK_START_WIFI_ERROR_TITLE);
  builder->Add("networkScreenQuickStartWiFiErrorSubtitle",
               IDS_LOGIN_QUICK_START_WIFI_ERROR_SUBTITLE);
  builder->AddF("quickStartNetworkNeededSubtitle",
                IDS_LOGIN_QUICK_START_NETWORK_NEEDED_SUBTITLE,
                ui::GetChromeOSDeviceName());

  ui::network_element::AddLocalizedValuesToBuilder(builder);
  cellular_setup::AddLocalizedValuesToBuilder(builder);
}

void NetworkScreenHandler::GetAdditionalParameters(base::Value::Dict* dict) {
  cellular_setup::AddNonStringLoadTimeDataToDict(dict);
}

void NetworkScreenHandler::SetQuickStartEntryPointVisibility(bool visible) {
  CallExternalAPI("setQuickStartEntryPointVisibility", visible);
}

base::WeakPtr<NetworkScreenView> NetworkScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
