// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_client.h"

#include "ash/public/cpp/locale_update_controller.h"
#include "ash/public/cpp/system_tray.h"
#include "ash/public/cpp/update_types.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/set_time_dialog.h"
#include "chrome/browser/chromeos/system/system_clock.h"
#include "chrome/browser/chromeos/web_applications/default_web_app_ids.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/webui/chromeos/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_config_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/tether_constants.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/mojom/net.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/connection_holder.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/api/vpn_provider/vpn_service.h"
#include "extensions/browser/api/vpn_provider/vpn_service_factory.h"
#include "net/base/escape.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/events/event_constants.h"

using chromeos::DBusThreadManager;
using chromeos::UpdateEngineClient;
using session_manager::SessionManager;
using session_manager::SessionState;

namespace {

SystemTrayClient* g_system_tray_client_instance = nullptr;

void ShowSettingsSubPageForActiveUser(const std::string& sub_page) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetActiveUserProfile(), sub_page);
}

// Returns the severity of a pending Chrome / Chrome OS update.
ash::UpdateSeverity GetUpdateSeverity(UpgradeDetector* detector) {
  switch (detector->upgrade_notification_stage()) {
    case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
      return ash::UpdateSeverity::kNone;
    case UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW:
      return ash::UpdateSeverity::kVeryLow;
    case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
      return ash::UpdateSeverity::kLow;
    case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
      return ash::UpdateSeverity::kElevated;
    case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
      return ash::UpdateSeverity::kHigh;
    case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
      break;
  }
  DCHECK_EQ(detector->upgrade_notification_stage(),
            UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL);
  return ash::UpdateSeverity::kCritical;
}

const chromeos::NetworkState* GetNetworkState(const std::string& network_id) {
  if (network_id.empty())
    return nullptr;
  return chromeos::NetworkHandler::Get()
      ->network_state_handler()
      ->GetNetworkStateFromGuid(network_id);
}

bool IsArcVpn(const std::string& network_id) {
  const chromeos::NetworkState* network_state = GetNetworkState(network_id);
  return network_state && network_state->type() == shill::kTypeVPN &&
         network_state->GetVpnProviderType() == shill::kProviderArcVpn;
}

}  // namespace

SystemTrayClient::SystemTrayClient()
    : system_tray_(ash::SystemTray::Get()),
      update_notification_style_(ash::NotificationStyle::kDefault) {
  // If this observes clock setting changes before ash comes up the IPCs will
  // be queued on |system_tray_|.
  g_browser_process->platform_part()->GetSystemClock()->AddObserver(this);

  // If an upgrade is available at startup then tell ash about it.
  if (UpgradeDetector::GetInstance()->notify_upgrade())
    HandleUpdateAvailable();

  // If the device is enterprise managed then send ash the enterprise domain.
  policy::BrowserPolicyConnectorChromeOS* policy_connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      policy_connector->GetDeviceCloudPolicyManager();
  if (policy_manager)
    policy_manager->core()->store()->AddObserver(this);
  UpdateEnterpriseDisplayDomain();

  system_tray_->SetClient(this);

  DCHECK(!g_system_tray_client_instance);
  g_system_tray_client_instance = this;
  UpgradeDetector::GetInstance()->AddObserver(this);
}

SystemTrayClient::~SystemTrayClient() {
  DCHECK_EQ(this, g_system_tray_client_instance);
  g_system_tray_client_instance = nullptr;

  system_tray_->SetClient(nullptr);

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  if (policy_manager)
    policy_manager->core()->store()->RemoveObserver(this);

  g_browser_process->platform_part()->GetSystemClock()->RemoveObserver(this);
  UpgradeDetector::GetInstance()->RemoveObserver(this);
}

// static
SystemTrayClient* SystemTrayClient::Get() {
  return g_system_tray_client_instance;
}

void SystemTrayClient::SetFlashUpdateAvailable() {
  flash_update_available_ = true;
  HandleUpdateAvailable();
}

void SystemTrayClient::SetUpdateNotificationState(
    ash::NotificationStyle style,
    const base::string16& notification_title,
    const base::string16& notification_body) {
  update_notification_style_ = style;
  update_notification_title_ = notification_title;
  update_notification_body_ = notification_body;
  HandleUpdateAvailable();
}

void SystemTrayClient::SetPrimaryTrayEnabled(bool enabled) {
  system_tray_->SetPrimaryTrayEnabled(enabled);
}

void SystemTrayClient::SetPrimaryTrayVisible(bool visible) {
  system_tray_->SetPrimaryTrayVisible(visible);
}

void SystemTrayClient::SetPerformanceTracingIconVisible(bool visible) {
  system_tray_->SetPerformanceTracingIconVisible(visible);
}

void SystemTrayClient::SetLocaleList(
    std::vector<ash::LocaleInfo> locale_list,
    const std::string& current_locale_iso_code) {
  system_tray_->SetLocaleList(std::move(locale_list), current_locale_iso_code);
}

////////////////////////////////////////////////////////////////////////////////
// ash::mojom::SystemTrayClient:

void SystemTrayClient::ShowSettings() {
  // TODO(jamescook): Use different metric for OS settings.
  base::RecordAction(base::UserMetricsAction("ShowOptions"));
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetActiveUserProfile());
}

void SystemTrayClient::ShowBluetoothSettings() {
  base::RecordAction(base::UserMetricsAction("ShowBluetoothSettingsPage"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kBluetoothDevicesSubpagePath);
}

void SystemTrayClient::ShowBluetoothPairingDialog(
    const std::string& address,
    const base::string16& name_for_display,
    bool paired,
    bool connected) {
  if (chromeos::BluetoothPairingDialog::ShowDialog(address, name_for_display,
                                                   paired, connected)) {
    base::RecordAction(
        base::UserMetricsAction("StatusArea_Bluetooth_Connect_Unknown"));
  }
}

void SystemTrayClient::ShowDateSettings() {
  base::RecordAction(base::UserMetricsAction("ShowDateOptions"));
  // Everybody can change the time zone (even though it is a device setting).
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kDateAndTimeSectionPath);
}

void SystemTrayClient::ShowSetTimeDialog() {
  chromeos::SetTimeDialog::ShowDialog();
}

void SystemTrayClient::ShowDisplaySettings() {
  base::RecordAction(base::UserMetricsAction("ShowDisplayOptions"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kDisplaySubpagePath);
}

void SystemTrayClient::ShowPowerSettings() {
  base::RecordAction(base::UserMetricsAction("Tray_ShowPowerOptions"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kPowerSubpagePath);
}

void SystemTrayClient::ShowChromeSlow() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetPrimaryUserProfile());
  chrome::ShowSlow(displayer.browser());
}

void SystemTrayClient::ShowIMESettings() {
  base::RecordAction(base::UserMetricsAction("OpenLanguageOptionsDialog"));
  const std::string path =
      base::FeatureList::IsEnabled(
          ::chromeos::features::kLanguageSettingsUpdate)
          ? chromeos::settings::mojom::kInputSubpagePath
          : chromeos::settings::mojom::kLanguagesAndInputDetailsSubpagePath;
  ShowSettingsSubPageForActiveUser(path);
}

void SystemTrayClient::ShowConnectedDevicesSettings() {
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kMultiDeviceFeaturesSubpagePath);
}

void SystemTrayClient::ShowAboutChromeOS() {
  // We always want to check for updates when showing the about page from the
  // Ash UI.
  ShowSettingsSubPageForActiveUser(
      std::string(chromeos::settings::mojom::kAboutChromeOsSectionPath) +
      "?checkForUpdate=true");
}

void SystemTrayClient::ShowHelp() {
  chrome::ShowHelpForProfile(ProfileManager::GetActiveUserProfile(),
                             chrome::HELP_SOURCE_MENU);
}

void SystemTrayClient::ShowAccessibilityHelp() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chromeos::AccessibilityManager::ShowAccessibilityHelp(displayer.browser());
}

void SystemTrayClient::ShowAccessibilitySettings() {
  base::RecordAction(base::UserMetricsAction("ShowAccessibilitySettings"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kManageAccessibilitySubpagePath);
}

void SystemTrayClient::ShowGestureEducationHelp() {
  base::RecordAction(base::UserMetricsAction("ShowGestureEducationHelp"));
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return;

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfileRedirectInIncognito(profile);
  proxy->LaunchAppWithUrl(
      chromeos::default_web_apps::kHelpAppId, ui::EventFlags::EF_NONE,
      GURL(chrome::kChromeOSGestureEducationHelpURL),
      apps::mojom::LaunchSource::kFromOtherApp, display::kDefaultDisplayId);
}

void SystemTrayClient::ShowPaletteHelp() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  ShowSingletonTab(displayer.browser(), GURL(chrome::kChromePaletteHelpURL));
}

void SystemTrayClient::ShowPaletteSettings() {
  base::RecordAction(base::UserMetricsAction("ShowPaletteOptions"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kStylusSubpagePath);
}

void SystemTrayClient::ShowPublicAccountInfo() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::ShowPolicy(displayer.browser());
}

void SystemTrayClient::ShowEnterpriseInfo() {
  // At the login screen, lock screen, etc. show enterprise help in a window.
  if (SessionManager::Get()->IsUserSessionBlocked()) {
    scoped_refptr<chromeos::HelpAppLauncher> help_app(
        new chromeos::HelpAppLauncher(nullptr /* parent_window */));
    help_app->ShowHelpTopic(chromeos::HelpAppLauncher::HELP_ENTERPRISE);
    return;
  }

  // Otherwise show enterprise management info page.
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::ShowEnterpriseManagementPageInTabbedBrowser(displayer.browser());
}

void SystemTrayClient::ShowNetworkConfigure(const std::string& network_id) {
  // UI is not available at the lock screen.
  if (SessionManager::Get()->IsScreenLocked())
    return;

  DCHECK(chromeos::NetworkHandler::IsInitialized());
  const chromeos::NetworkState* network_state = GetNetworkState(network_id);
  if (!network_state) {
    LOG(ERROR) << "Network not found: " << network_id;
    return;
  }
  if (network_state->type() == chromeos::kTypeTether &&
      !network_state->tether_has_connected_to_host()) {
    ShowNetworkSettingsHelper(network_id, true /* show_configure */);
    return;
  }

  chromeos::InternetConfigDialog::ShowDialogForNetworkId(network_id);
}

void SystemTrayClient::ShowNetworkCreate(const std::string& type) {
  if (type == ::onc::network_type::kCellular) {
    const chromeos::NetworkState* cellular =
        chromeos::NetworkHandler::Get()
            ->network_state_handler()
            ->FirstNetworkByType(
                chromeos::onc::NetworkTypePatternFromOncType(type));
    std::string network_id = cellular ? cellular->guid() : "";
    ShowNetworkSettingsHelper(network_id, false /* show_configure */);
    return;
  }
  chromeos::InternetConfigDialog::ShowDialogForNetworkType(type);
}

void SystemTrayClient::ShowThirdPartyVpnCreate(
    const std::string& extension_id) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  if (!profile)
    return;

  // Request that the third-party VPN provider show its "add network" dialog.
  chromeos::VpnServiceFactory::GetForBrowserContext(profile)
      ->SendShowAddDialogToExtension(extension_id);
}

void SystemTrayClient::ShowArcVpnCreate(const std::string& app_id) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  if (!profile)
    return;

  arc::LaunchApp(profile, app_id, ui::EF_NONE,
                 arc::UserInteractionType::APP_STARTED_FROM_SETTINGS);
}

void SystemTrayClient::ShowNetworkSettings(const std::string& network_id) {
  ShowNetworkSettingsHelper(network_id, false /* show_configure */);
}

void SystemTrayClient::ShowNetworkSettingsHelper(const std::string& network_id,
                                                 bool show_configure) {
  SessionManager* const session_manager = SessionManager::Get();
  if (session_manager->IsInSecondaryLoginScreen())
    return;
  if (!session_manager->IsSessionStarted()) {
    chromeos::InternetDetailDialog::ShowDialog(network_id);
    return;
  }

  if (IsArcVpn(network_id)) {
    // Special case: clicking on a connected ARCVPN will ask Android to
    // show the settings dialog.
    auto* net_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc::ArcServiceManager::Get()->arc_bridge_service()->net(),
        ConfigureAndroidVpn);
    if (!net_instance) {
      LOG(ERROR) << "User requested VPN configuration but API is unavailable";
      return;
    }
    net_instance->ConfigureAndroidVpn();
    return;
  }

  std::string page = chromeos::settings::mojom::kNetworkSectionPath;
  const chromeos::NetworkState* network_state = GetNetworkState(network_id);
  if (!network_id.empty() && network_state) {
    // TODO(khorimoto): Use a more general path name here. This path is named
    // kWifi*, but it's actually a generic page.
    page = chromeos::settings::mojom::kWifiDetailsSubpagePath;
    page += "?guid=";
    page += net::EscapeUrlEncodedData(network_id, true);
    page += "&name=";
    page += net::EscapeUrlEncodedData(network_state->name(), true);
    page += "&type=";
    page += net::EscapeUrlEncodedData(
        chromeos::network_util::TranslateShillTypeToONC(network_state->type()),
        true);
    if (show_configure)
      page += "&showConfigure=true";
  }
  base::RecordAction(base::UserMetricsAction("OpenInternetOptionsDialog"));
  ShowSettingsSubPageForActiveUser(page);
}

void SystemTrayClient::ShowMultiDeviceSetup() {
  chromeos::multidevice_setup::MultiDeviceSetupDialog::Show();
}

void SystemTrayClient::RequestRestartForUpdate() {
  // Flash updates on Chrome OS require device reboot.
  const browser_shutdown::RebootPolicy reboot_policy =
      flash_update_available_ ? browser_shutdown::RebootPolicy::kForceReboot
                              : browser_shutdown::RebootPolicy::kOptionalReboot;

  browser_shutdown::NotifyAndTerminate(true /* fast_path */, reboot_policy);
}

void SystemTrayClient::SetLocaleAndExit(const std::string& locale_iso_code) {
  ProfileManager::GetActiveUserProfile()->ChangeAppLocale(
      locale_iso_code, Profile::APP_LOCALE_CHANGED_VIA_SYSTEM_TRAY);
  chrome::AttemptUserExit();
}

void SystemTrayClient::HandleUpdateAvailable() {
  // Show an update icon for Chrome updates and Flash component updates.
  UpgradeDetector* detector = UpgradeDetector::GetInstance();
  bool update_available = detector->notify_upgrade() || flash_update_available_;
  DCHECK(update_available);
  if (!update_available)
    return;

  // Get the Chrome update severity.
  ash::UpdateSeverity severity = GetUpdateSeverity(detector);

  // Flash updates are low severity unless the Chrome severity is higher.
  if (flash_update_available_)
    severity = std::max(severity, ash::UpdateSeverity::kLow);

  // Show a string specific to updating flash player if there is no system
  // update.
  ash::UpdateType update_type = detector->notify_upgrade()
                                    ? ash::UpdateType::kSystem
                                    : ash::UpdateType::kFlash;

  system_tray_->ShowUpdateIcon(severity, detector->is_factory_reset_required(),
                               detector->is_rollback(), update_type);

  // Only overwrite title and body for system updates, not for flash updates.
  if (update_type == ash::UpdateType::kSystem) {
    system_tray_->SetUpdateNotificationState(update_notification_style_,
                                             update_notification_title_,
                                             update_notification_body_);
  }
}

////////////////////////////////////////////////////////////////////////////////
// chromeos::system::SystemClockObserver:

void SystemTrayClient::OnSystemClockChanged(
    chromeos::system::SystemClock* clock) {
  system_tray_->SetUse24HourClock(clock->ShouldUse24HourClock());
}

////////////////////////////////////////////////////////////////////////////////
// UpgradeDetector::UpgradeObserver:
void SystemTrayClient::OnUpdateOverCellularAvailable() {
  // Requests that ash show the update over cellular available icon.
  system_tray_->SetUpdateOverCellularAvailableIconVisible(true);
}

void SystemTrayClient::OnUpdateOverCellularOneTimePermissionGranted() {
  // Requests that ash hide the update over cellular available icon.
  system_tray_->SetUpdateOverCellularAvailableIconVisible(false);
}

void SystemTrayClient::OnUpgradeRecommended() {
  HandleUpdateAvailable();
}

////////////////////////////////////////////////////////////////////////////////
// policy::CloudPolicyStore::Observer
void SystemTrayClient::OnStoreLoaded(policy::CloudPolicyStore* store) {
  UpdateEnterpriseDisplayDomain();
}

void SystemTrayClient::OnStoreError(policy::CloudPolicyStore* store) {
  UpdateEnterpriseDisplayDomain();
}

void SystemTrayClient::UpdateEnterpriseDisplayDomain() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  const std::string enterprise_display_domain =
      connector->GetEnterpriseDisplayDomain();
  const bool active_directory_managed = connector->IsActiveDirectoryManaged();
  if (enterprise_display_domain == last_enterprise_display_domain_ &&
      active_directory_managed == last_active_directory_managed_) {
    return;
  }
  // Send to ash, which will add an item to the system tray.
  system_tray_->SetEnterpriseDisplayDomain(enterprise_display_domain,
                                           active_directory_managed);
  last_enterprise_display_domain_ = enterprise_display_domain;
  last_active_directory_managed_ = active_directory_managed;
}
