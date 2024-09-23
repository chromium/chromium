// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/internet/internet_handler.h"

#include <memory>
#include <vector>

#include "ash/components/arc/mojom/net.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/tether/tether_service.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/events/event_constants.h"

namespace ash::settings {

namespace {

const char kAddThirdPartyVpnMessage[] = "addThirdPartyVpn";
const char kConfigureThirdPartyVpnMessage[] = "configureThirdPartyVpn";
const char kShowCarrierAccountDetail[] = "showCarrierAccountDetail";
const char kShowCellularSetupUI[] = "showCellularSetupUi";
const char kShowPortalSignin[] = "showPortalSignin";
const char kRequestGmsCoreNotificationsDisabledDeviceNames[] =
    "requestGmsCoreNotificationsDisabledDeviceNames";
const char kSendGmsCoreNotificationsDisabledDeviceNames[] =
    "sendGmsCoreNotificationsDisabledDeviceNames";

Profile* GetProfileForPrimaryUser() {
  return ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetPrimaryUser());
}

bool IsVpnConfigAllowed() {
  PrefService* prefs = GetProfileForPrimaryUser()->GetPrefs();
  DCHECK(prefs);
  return prefs->GetBoolean(prefs::kVpnConfigAllowed);
}

}  // namespace

InternetHandler::InternetHandler(Profile* profile) : profile_(profile) {
  DCHECK(profile_);

  auto* tether_service = tether::TetherService::Get(profile);
  gms_core_notifications_state_tracker_ =
      tether_service ? tether_service->GetGmsCoreNotificationsStateTracker()
                     : nullptr;
  if (gms_core_notifications_state_tracker_) {
    gms_core_notifications_state_tracker_->AddObserver(this);
  }
}

InternetHandler::~InternetHandler() {
  if (gms_core_notifications_state_tracker_) {
    gms_core_notifications_state_tracker_->RemoveObserver(this);
  }
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
      kShowCarrierAccountDetail,
      base::BindRepeating(&InternetHandler::ShowCarrierAccountDetail,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kShowCellularSetupUI,
      base::BindRepeating(&InternetHandler::ShowCellularSetupUI,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kShowPortalSignin, base::BindRepeating(&InternetHandler::ShowPortalSignin,
                                             base::Unretained(this)));
}

void InternetHandler::OnJavascriptAllowed() {}

void InternetHandler::OnJavascriptDisallowed() {}

void InternetHandler::OnGmsCoreNotificationStateChanged() {
  SetGmsCoreNotificationsDisabledDeviceNames();
}

void InternetHandler::AddThirdPartyVpn(const base::Value::List& args) {
  if (args.size() < 1 || !args[0].is_string()) {
    NOTREACHED_IN_MIGRATION()
        << "Invalid args for: " << kAddThirdPartyVpnMessage;
    return;
  }
  const std::string& app_id = args[0].GetString();
  if (app_id.empty()) {
    NET_LOG(ERROR) << "Empty app id for " << kAddThirdPartyVpnMessage;
    return;
  }
  if (profile_ != GetProfileForPrimaryUser() || profile_->IsChild()) {
    NET_LOG(ERROR)
        << "Only the primary user and non-child accounts can add VPNs";
    return;
  }
  if (!IsVpnConfigAllowed()) {
    NET_LOG(ERROR) << "Cannot add VPN; prohibited by policy";
    return;
  }

  // Request to launch Arc VPN provider.
  const auto* arc_app_list_prefs = ArcAppListPrefs::Get(profile_);
  if (arc_app_list_prefs && arc_app_list_prefs->GetApp(app_id)) {
    DCHECK(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
        profile_));
    apps::AppServiceProxyFactory::GetForProfile(profile_)->Launch(
        app_id, ui::EF_NONE, apps::LaunchSource::kFromParentalControls);
    return;
  }

  // Request that the third-party VPN provider identified by |provider_id|
  // show its "add network" dialog.
  chromeos::VpnServiceFactory::GetForBrowserContext(GetProfileForPrimaryUser())
      ->SendShowAddDialogToExtension(app_id);
}

void InternetHandler::ConfigureThirdPartyVpn(const base::Value::List& args) {
  if (args.size() < 1 || !args[0].is_string()) {
    NOTREACHED_IN_MIGRATION()
        << "Invalid args for: " << kConfigureThirdPartyVpnMessage;
    return;
  }
  const std::string& guid = args[0].GetString();
  if (profile_ != GetProfileForPrimaryUser()) {
    NET_LOG(ERROR) << "Only the primary user can configure VPNs";
    return;
  }
  if (!IsVpnConfigAllowed()) {
    NET_LOG(ERROR) << "Cannot configure VPN; prohibited by policy";
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
    NET_LOG(ERROR) << "ConfigureThirdPartyVpn: Network is not a VPN: "
                   << NetworkId(network);
    return;
  }

  if (network->GetVpnProviderType() == shill::kProviderThirdPartyVpn) {
    // Request that the third-party VPN provider used by the |network| show a
    // configuration dialog for it.
    chromeos::VpnServiceFactory::GetForBrowserContext(profile_)
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
                 << network->GetVpnProviderType()
                 << " For: " << NetworkId(network);
}

void InternetHandler::RequestGmsCoreNotificationsDisabledDeviceNames(
    const base::Value::List& args) {
  AllowJavascript();
  SetGmsCoreNotificationsDisabledDeviceNames();
}

void InternetHandler::ShowCarrierAccountDetail(const base::Value::List& args) {
  if (args.size() < 1 || !args[0].is_string()) {
    NOTREACHED_IN_MIGRATION()
        << "Invalid args for: " << kShowCarrierAccountDetail;
    return;
  }
  const std::string& guid = args[0].GetString();
  NetworkConnect::Get()->ShowCarrierAccountDetail(guid);
}

void InternetHandler::ShowPortalSignin(const base::Value::List& args) {
  if (args.size() < 1 || !args[0].is_string()) {
    NOTREACHED_IN_MIGRATION() << "Invalid args for: " << kShowPortalSignin;
    return;
  }
  const std::string& guid = args[0].GetString();
  NetworkConnect::Get()->ShowPortalSignin(guid,
                                          NetworkConnect::Source::kSettings);
}

void InternetHandler::ShowCellularSetupUI(const base::Value::List& args) {
  if (args.size() < 1 || !args[0].is_string()) {
    NOTREACHED_IN_MIGRATION()
        << "Invalid args for: " << kConfigureThirdPartyVpnMessage;
    return;
  }
  const std::string& guid = args[0].GetString();
  NetworkConnect::Get()->ShowMobileSetup(guid);
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
    device_names_without_notifications_.Append(base::Value(device_name));
  }
  SendGmsCoreNotificationsDisabledDeviceNames();
}

void InternetHandler::SendGmsCoreNotificationsDisabledDeviceNames() {
  if (!IsJavascriptAllowed()) {
    return;
  }

  base::Value::List device_names_value;
  for (const auto& device_name : device_names_without_notifications_) {
    device_names_value.Append(device_name.Clone());
  }

  FireWebUIListener(kSendGmsCoreNotificationsDisabledDeviceNames,
                    device_names_value);
}

gfx::NativeWindow InternetHandler::GetNativeWindow() {
  return web_ui()->GetWebContents()->GetTopLevelNativeWindow();
}

void InternetHandler::SetGmsCoreNotificationsStateTrackerForTesting(
    tether::GmsCoreNotificationsStateTracker*
        gms_core_notifications_state_tracker) {
  if (gms_core_notifications_state_tracker_) {
    gms_core_notifications_state_tracker_->RemoveObserver(this);
  }

  gms_core_notifications_state_tracker_ = gms_core_notifications_state_tracker;
  gms_core_notifications_state_tracker_->AddObserver(this);
}

}  // namespace ash::settings
