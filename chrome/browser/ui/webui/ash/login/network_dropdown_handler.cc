// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/network_dropdown_handler.h"

#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/internet/internet_config_dialog.h"
#include "chrome/browser/ui/webui/ash/internet/internet_detail_dialog.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// JS API callbacks names.
const char kJsApiLaunchInternetDetailDialog[] = "launchInternetDetailDialog";
const char kJsApiLaunchAddWiFiNetworkDialog[] = "launchAddWiFiNetworkDialog";
const char kJsApiShowNetworkConfig[] = "showNetworkConfig";
const char kJsApiShowNetworkDetails[] = "showNetworkDetails";

}  // namespace

namespace ash {

NetworkDropdownHandler::NetworkDropdownHandler() = default;
NetworkDropdownHandler::~NetworkDropdownHandler() = default;

void NetworkDropdownHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void NetworkDropdownHandler::DeclareJSCallbacks() {
  AddCallback(kJsApiLaunchInternetDetailDialog,
              &NetworkDropdownHandler::HandleLaunchInternetDetailDialog);
  AddCallback(kJsApiLaunchAddWiFiNetworkDialog,
              &NetworkDropdownHandler::HandleLaunchAddWiFiNetworkDialog);
  AddCallback(kJsApiShowNetworkDetails,
              &NetworkDropdownHandler::HandleShowNetworkDetails);
  AddCallback(kJsApiShowNetworkConfig,
              &NetworkDropdownHandler::HandleShowNetworkConfig);
}

void NetworkDropdownHandler::HandleLaunchInternetDetailDialog() {
  // Empty string opens the internet detail dialog for the default network.
  InternetDetailDialog::ShowDialog(
      "", LoginDisplayHost::default_host()->GetNativeWindow());
}

void NetworkDropdownHandler::HandleLaunchAddWiFiNetworkDialog() {
  // Make sure WiFi is enabled.
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  TechnologyStateController* controller =
      NetworkHandler::Get()->technology_state_controller();
  if (handler->GetTechnologyState(NetworkTypePattern::WiFi()) !=
      NetworkStateHandler::TECHNOLOGY_ENABLED) {
    controller->SetTechnologiesEnabled(NetworkTypePattern::WiFi(), true,
                                       network_handler::ErrorCallback());
  }
  InternetConfigDialog::ShowDialogForNetworkType(
      ::onc::network_type::kWiFi,
      LoginDisplayHost::default_host()->GetNativeWindow());
}

void NetworkDropdownHandler::HandleShowNetworkDetails(const std::string& type,
                                                      const std::string& guid) {
  if (type == ::onc::network_type::kCellular) {
    // Make sure Cellular is enabled.
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    TechnologyStateController* controller =
        NetworkHandler::Get()->technology_state_controller();
    if (handler->GetTechnologyState(NetworkTypePattern::Cellular()) !=
        NetworkStateHandler::TECHNOLOGY_ENABLED) {
      controller->SetTechnologiesEnabled(NetworkTypePattern::Cellular(), true,
                                         network_handler::ErrorCallback());
    }
  }
  InternetDetailDialog::ShowDialog(
      guid, LoginDisplayHost::default_host()->GetNativeWindow());
}

void NetworkDropdownHandler::HandleShowNetworkConfig(const std::string& guid) {
  InternetConfigDialog::ShowDialogForNetworkId(
      guid, LoginDisplayHost::default_host()->GetNativeWindow());
}

}  // namespace ash
