// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/internet_handler.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/tether/tether_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/mojom/net.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/onc/onc_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/api/vpn_provider/vpn_service.h"
#include "extensions/browser/api/vpn_provider/vpn_service_factory.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/events/event_constants.h"

namespace chromeos {

namespace {

const char kAddThirdPartyVpnMessage[] = "addThirdPartyVpn";
const char kConfigureThirdPartyVpnMessage[] = "configureThirdPartyVpn";
const char kShowCellularSetupUI[] = "showCellularSetupUI";
const char kRequestGmsCoreNotificationsDisabledDeviceNames[] =
    "requestGmsCoreNotificationsDisabledDeviceNames";
const char kSendGmsCoreNotificationsDisabledDeviceNames[] =
    "sendGmsCoreNotificationsDisabledDeviceNames";

Profile* GetProfileForPrimaryUser() {
  return ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetPrimaryUser());
}

}  // namespace

namespace settings {

InternetHandler::InternetHandler(Profile* profile) : profile_(profile) {
  DCHECK(profile_);

  TetherService* tether_service = TetherService::Get(profile);
  gms_core_notifications_state_tracker_ =
      tether_service ? tether_service->GetGmsCoreNotificationsStateTracker()
                     : nullptr;
  if (gms_core_notifications_state_tracker_)
    gms_core_notifications_state_tracker_->AddObserver(this);
}

InternetHandler::~InternetHandler() {
  if (gms_core_notifications_state_tracker_)
    gms_core_notifications_state_tracker_->RemoveObserver(this);
}

void InternetHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kAddThirdPartyVpnMessage,
      base::BindRepeating(&InternetHandler::AddThirdPartyVpn,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kConfigureThirdPartyVpnMessage,
      base::BindRepeating(&InternetHandler::ConfigureThirdPartyVpn,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kRequestGmsCoreNotificationsDisabledDeviceNames,
      base::BindRepeating(
          &InternetHandler::RequestGmsCoreNotificationsDisabledDeviceNames,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kShowCellularSetupUI,
      base::BindRepeating(&InternetHandler::ShowCellularSetupUI,
                          base::Unretained(this)));
}

void InternetHandler::OnJavascriptAllowed() {}

void InternetHandler::OnJavascriptDisallowed() {}

void InternetHandler::OnGmsCoreNotificationStateChanged() {
  SetGmsCoreNotificationsDisabledDeviceNames();
}

void InternetHandler::AddThirdPartyVpn(const base::ListValue* args) {
  std::string app_id;
  if (args->GetSize() < 1 || !args->GetString(0, &app_id)) {
    NOTREACHED() << "Invalid args for: " << kAddThirdPartyVpnMessage;
    return;
  }
  if (app_id.empty()) {
    NET_LOG(ERROR) << "Empty app id for " << kAddThirdPartyVpnMessage;
    return;
  }
  if (profile_ != GetProfileForPrimaryUser() || profile_->IsChild()) {
    NET_LOG(ERROR)
        << "Only the primary user and non-child accounts can add VPNs";
    return;
  }

  // Request to launch Arc VPN provider.
  const auto* arc_app_list_prefs = ArcAppListPrefs::Get(profile_);
  if (arc_app_list_prefs && arc_app_list_prefs->GetApp(app_id)) {
    arc::LaunchApp(profile_, app_id, ui::EF_NONE,
                   arc::UserInteractionType::APP_STARTED_FROM_SETTINGS);
    return;
  }

  // Request that the third-party VPN provider identified by |provider_id|
  // show its "add network" dialog.
  VpnServiceFactory::GetForBrowserContext(GetProfileForPrimaryUser())
      ->SendShowAddDialogToExtension(app_id);
}

void InternetHandler::ConfigureThirdPartyVpn(const base::ListValue* args) {
  std::string guid;
  if (args->GetSize() < 1 || !args->GetString(0, &guid)) {
    NOTREACHED() << "Invalid args for: " << kConfigureThirdPartyVpnMessage;
    return;
  }
  if (profile_ != GetProfileForPrimaryUser()) {
    NET_LOG(ERROR) << "Only the primary user can configure VPNs";
    return;
  }

  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  if (!network) {
    NET_LOG(ERROR) << "ConfigureThirdPartyVpn: Network not found: " << guid;
    return;
  }
  if (network->type() != shill::kTypeVPN) {
    NET_LOG(ERROR) << "ConfigureThirdPartyVpn: Network is not a VPN: " << guid;
    return;
  }

  if (network->GetVpnProviderType() == shill::kProviderThirdPartyVpn) {
    // Request that the third-party VPN provider used by the |network| show a
    // configuration dialog for it.
    VpnServiceFactory::GetForBrowserContext(profile_)
        ->SendShowConfigureDialogToExtension(network->vpn_provider()->id,
                                             network->name());
    return;
  }

  if (network->GetVpnProviderType() == shill::kProviderArcVpn) {
    auto* net_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc::ArcServiceManager::Get()->arc_bridge_service()->net(),
        ConfigureAndroidVpn);
    if (!net_instance) {
      NET_LOG(ERROR) << "ConfigureThirdPartyVpn: API is unavailable";
      return;
    }
    net_instance->ConfigureAndroidVpn();
    return;
  }

  NET_LOG(ERROR) << "ConfigureThirdPartyVpn: Unsupported VPN type: "
                 << network->GetVpnProviderType() << " For: " << guid;
}

void InternetHandler::RequestGmsCoreNotificationsDisabledDeviceNames(
    const base::ListValue* args) {
  AllowJavascript();
  SetGmsCoreNotificationsDisabledDeviceNames();
}

void InternetHandler::ShowCellularSetupUI(const base::ListValue* args) {
  std::string guid;
  if (args->GetSize() < 1 || !args->GetString(0, &guid)) {
    NOTREACHED() << "Invalid args for: " << kConfigureThirdPartyVpnMessage;
    return;
  }
  chromeos::NetworkConnect::Get()->ShowMobileSetup(guid);
}

void InternetHandler::SetGmsCoreNotificationsDisabledDeviceNames() {
  if (!gms_core_notifications_state_tracker_) {
    // No device names should be present in the list if
    // |gms_core_notifications_state_tracker_| is null.
    DCHECK(device_names_without_notifications_.empty());
    return;
  }

  device_names_without_notifications_.clear();

  const std::vector<std::string> device_names =
      gms_core_notifications_state_tracker_
          ->GetGmsCoreNotificationsDisabledDeviceNames();
  for (const auto& device_name : device_names) {
    device_names_without_notifications_.emplace_back(
        std::make_unique<base::Value>(device_name));
  }
  SendGmsCoreNotificationsDisabledDeviceNames();
}

void InternetHandler::SendGmsCoreNotificationsDisabledDeviceNames() {
  if (!IsJavascriptAllowed())
    return;

  base::ListValue device_names_value;
  for (const auto& device_name : device_names_without_notifications_)
    device_names_value.Append(device_name->Clone());

  FireWebUIListener(kSendGmsCoreNotificationsDisabledDeviceNames,
                    device_names_value);
}

gfx::NativeWindow InternetHandler::GetNativeWindow() const {
  return web_ui()->GetWebContents()->GetTopLevelNativeWindow();
}

void InternetHandler::SetGmsCoreNotificationsStateTrackerForTesting(
    chromeos::tether::GmsCoreNotificationsStateTracker*
        gms_core_notifications_state_tracker) {
  if (gms_core_notifications_state_tracker_)
    gms_core_notifications_state_tracker_->RemoveObserver(this);

  gms_core_notifications_state_tracker_ = gms_core_notifications_state_tracker;
  gms_core_notifications_state_tracker_->AddObserver(this);
}

}  // namespace settings

}  // namespace chromeos
