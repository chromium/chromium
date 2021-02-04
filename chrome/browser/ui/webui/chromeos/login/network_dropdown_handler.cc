// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_dropdown_handler.h"

#include "chrome/browser/ui/webui/chromeos/internet_config_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// JS API callbacks names.
const char kJsApiLaunchInternetDetailDialog[] = "launchInternetDetailDialog";
const char kJsApiLaunchAddWiFiNetworkDialog[] = "launchAddWiFiNetworkDialog";
const char kJsApiShowNetworkConfig[] = "showNetworkConfig";
const char kJsApiShowNetworkDetails[] = "showNetworkDetails";

}  // namespace

namespace chromeos {

NetworkDropdownHandler::NetworkDropdownHandler(
    JSCallsContainer* js_calls_container)
    : BaseWebUIHandler(js_calls_container) {}

NetworkDropdownHandler::~NetworkDropdownHandler() = default;

void NetworkDropdownHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void NetworkDropdownHandler::Initialize() {}

void NetworkDropdownHandler::RegisterMessages() {
  AddCallback(kJsApiLaunchInternetDetailDialog,
              &NetworkDropdownHandler::HandleLaunchInternetDetailDialog);
  AddCallback(kJsApiLaunchAddWiFiNetworkDialog,
              &NetworkDropdownHandler::HandleLaunchAddWiFiNetworkDialog);
  AddRawCallback(kJsApiShowNetworkDetails,
                 &NetworkDropdownHandler::HandleShowNetworkDetails);
  AddRawCallback(kJsApiShowNetworkConfig,
                 &NetworkDropdownHandler::HandleShowNetworkConfig);
}

void NetworkDropdownHandler::HandleLaunchInternetDetailDialog() {
  // Empty string opens the internet detail dialog for the default network.
  InternetDetailDialog::ShowDialog("");
}

void NetworkDropdownHandler::HandleLaunchAddWiFiNetworkDialog() {
  // Make sure WiFi is enabled.
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  if (handler->GetTechnologyState(NetworkTypePattern::WiFi()) !=
      NetworkStateHandler::TECHNOLOGY_ENABLED) {
    handler->SetTechnologyEnabled(NetworkTypePattern::WiFi(), true,
                                  network_handler::ErrorCallback());
  }
  chromeos::InternetConfigDialog::ShowDialogForNetworkType(
      ::onc::network_type::kWiFi);
}

void NetworkDropdownHandler::HandleShowNetworkDetails(
    const base::ListValue* args) {
  std::string type, guid;
  args->GetString(0, &type);
  args->GetString(1, &guid);
  if (type == ::onc::network_type::kCellular) {
    // Make sure Cellular is enabled.
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    if (handler->GetTechnologyState(NetworkTypePattern::Cellular()) !=
        NetworkStateHandler::TECHNOLOGY_ENABLED) {
      handler->SetTechnologyEnabled(NetworkTypePattern::Cellular(), true,
                                    network_handler::ErrorCallback());
    }
  }
  InternetDetailDialog::ShowDialog(guid);
}

void NetworkDropdownHandler::HandleShowNetworkConfig(
    const base::ListValue* args) {
  std::string guid;
  args->GetString(0, &guid);
  chromeos::InternetConfigDialog::ShowDialogForNetworkId(guid);
}

}  // namespace chromeos
