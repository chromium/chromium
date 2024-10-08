// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"

#include <cstdio>
#include <memory>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/personalization_entry_point.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/locale_update_controller.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/system_tray.h"
#include "ash/public/cpp/update_types.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "ash/webui/settings/public/constants/setting.mojom.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/eol/eol_incentive_util.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system/system_clock.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_metrics.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/extensions/vpn_provider/vpn_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_dialog.h"
#include "chrome/browser/ui/webui/ash/bluetooth/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/webui/ash/internet/internet_config_dialog.h"
#include "chrome/browser/ui/webui/ash/internet/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/browser/ui/webui/ash/set_time/set_time_dialog.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_util.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/ash/components/network/tether_constants.h"
#include "chromeos/ash/components/phonehub/util/histogram_util.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

using session_manager::SessionManager;
using session_manager::SessionState;

namespace {

SystemTrayClientImpl* g_system_tray_client_instance = nullptr;

// The prefix a calendar event URL *must* have in order to be launched by the
// calendar web app.
constexpr char kOfficialCalendarUrlPrefix[] =
    "https://calendar.google.com/calendar/";

void ShowSettingsSubPageForActiveUser(const std::string& sub_page) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetActiveUserProfile(), sub_page);
}

// Returns the severity of a pending update.
ash::UpdateSeverity GetUpdateSeverity(UpgradeDetector* detector) {
  // OS updates use UpgradeDetector's severity mapping.
  switch (detector->upgrade_notification_stage()) {
    case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
      return ash::UpdateSeverity::kNone;
    case UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW:
      return ash::UpdateSeverity::kVeryLow;
    case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
      return ash::UpdateSeverity::kLow;
    case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
      return ash::UpdateSeverity::kElevated;
    case UpgradeDetector::UPGRADE_ANNOYANCE_GRACE:
      return ash::UpdateSeverity::kGrace;
    case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
      return ash::UpdateSeverity::kHigh;
    case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
      return ash::UpdateSeverity::kCritical;
  }
}

const ash::NetworkState* GetNetworkState(const std::string& network_id) {
  if (network_id.empty())
    return nullptr;
  return ash::NetworkHandler::Get()
      ->network_state_handler()
      ->GetNetworkStateFromGuid(network_id);
}

bool ShouldOpenCellularSetupPsimFlowOnClick(const std::string& network_id) {
  // |kActivationStateNotActivated| is only set in physical SIM networks,
  // checking a networks activation state is |kActivationStateNotActivated|
  // ensures the current network is a phyical SIM network.

  const ash::NetworkState* network_state = GetNetworkState(network_id);
  return network_state && network_state->type() == shill::kTypeCellular &&
         network_state->activation_state() ==
             shill::kActivationStateNotActivated &&
         network_state->eid().empty();
}

apps::AppServiceProxyAsh* GetActiveUserAppServiceProxyAsh() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  apps::AppServiceProxyAsh* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  return proxy;
}

apps::AppRegistryCache* GetActiveUserAppRegistryCache() {
  apps::AppServiceProxyAsh* proxy = GetActiveUserAppServiceProxyAsh();
  if (!proxy)
    return nullptr;

  return &proxy->AppRegistryCache();
}

bool IsAppInstalled(std::string app_id) {
  apps::AppRegistryCache* reg_cache = GetActiveUserAppRegistryCache();
  if (!reg_cache) {
    LOG(ERROR) << __FUNCTION__
               << " Failed to get active user AppRegistryCache ";
    return false;
  }

  bool found_app_id = false;
  reg_cache->ForEachApp([&found_app_id, app_id](const apps::AppUpdate& update) {
    if (update.AppId() == app_id) {
      found_app_id = true;
      return;
    }
  });

  return found_app_id;
}

void OpenInBrowser(const GURL& event_url) {
  ShowSingletonTabOverwritingNTP(ProfileManager::GetActiveUserProfile(),
                                 event_url,
                                 NavigateParams::IGNORE_AND_NAVIGATE);
}

ash::ManagementDeviceMode GetManagementDeviceMode(
    policy::BrowserPolicyConnectorAsh* connector) {
  if (!connector->IsDeviceEnterpriseManaged())
    return ash::ManagementDeviceMode::kNone;

  if (connector->IsKioskEnrolled())
    return ash::ManagementDeviceMode::kKioskSku;

  switch (connector->GetEnterpriseMarketSegment()) {
    case policy::MarketSegment::UNKNOWN:
      return ash::ManagementDeviceMode::kOther;
    case policy::MarketSegment::ENTERPRISE:
      return ash::ManagementDeviceMode::kChromeEnterprise;
    case policy::MarketSegment::EDUCATION:
      return ash::ManagementDeviceMode::kChromeEducation;
  }

  NOTREACHED_IN_MIGRATION();
  return ash::ManagementDeviceMode::kOther;
}

}  // namespace

class SystemTrayClientImpl::EnterpriseAccountObserver
    : public user_manager::UserManager::UserSessionStateObserver,
      public policy::CloudPolicyStore::Observer,
      public session_manager::SessionManagerObserver {
 public:
  explicit EnterpriseAccountObserver(SystemTrayClientImpl* owner)
      : owner_(owner) {
    user_manager::UserManager* manager = user_manager::UserManager::Get();
    session_state_observation_.Observe(manager);
    session_observation_.Observe(session_manager::SessionManager::Get());
    UpdateProfile();
  }
  EnterpriseAccountObserver(const EnterpriseAccountObserver&) = delete;
  EnterpriseAccountObserver& operator=(const EnterpriseAccountObserver&) =
      delete;
  ~EnterpriseAccountObserver() override = default;

 private:
  const raw_ptr<SystemTrayClientImpl> owner_;
  raw_ptr<Profile> profile_ = nullptr;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::UserSessionStateObserver>
      session_state_observation_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};
  base::ScopedObservation<policy::CloudPolicyStore,
                          policy::CloudPolicyStore::Observer>
      policy_observation_{this};

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override {
    UpdateProfile();
  }

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override {
    TRACE_EVENT0("ui",
                 "SystemTrayClientImpl::EnterpriseAccountObserver::"
                 "OnSessionStateChanged");
    UpdateProfile();
  }

  // policy::CloudPolicyStore::Observer
  void OnStoreLoaded(policy::CloudPolicyStore* store) override {
    owner_->UpdateEnterpriseAccountDomainInfo(profile_);
  }
  void OnStoreError(policy::CloudPolicyStore* store) override {
    owner_->UpdateEnterpriseAccountDomainInfo(profile_);
  }

  void UpdateProfile() {
    user_manager::User* user =
        user_manager::UserManager::Get()->GetActiveUser();
    Profile* profile =
        user ? ash::ProfileHelper::Get()->GetProfileByUser(user) : nullptr;
    if (profile == profile_)
      return;

    policy_observation_.Reset();

    profile_ = profile;
    if (profile_) {
      policy::UserCloudPolicyManagerAsh* manager =
          profile_->GetUserCloudPolicyManagerAsh();
      if (manager)
        policy_observation_.Observe(manager->core()->store());
    }
    owner_->UpdateEnterpriseAccountDomainInfo(profile_);
  }
};

SystemTrayClientImpl::SystemTrayClientImpl()
    : system_tray_(ash::SystemTray::Get()),
      enterprise_account_observer_(
          std::make_unique<EnterpriseAccountObserver>(this)) {
  // If this observes clock setting changes before ash comes up the IPCs will
  // be queued on |system_tray_|.
  ash::system::SystemClock* clock =
      g_browser_process->platform_part()->GetSystemClock();
  clock->AddObserver(this);
  system_tray_->SetUse24HourClock(clock->ShouldUse24HourClock());

  // If an upgrade is available at startup then tell ash about it.
  if (UpgradeDetector::GetInstance()->notify_upgrade())
    HandleUpdateAvailable();

  // If the device is enterprise managed then send ash the enterprise domain.
  policy::BrowserPolicyConnectorAsh* policy_connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::DeviceCloudPolicyManagerAsh* policy_manager =
      policy_connector->GetDeviceCloudPolicyManager();
  if (policy_manager)
    policy_manager->core()->store()->AddObserver(this);
  UpdateDeviceEnterpriseInfo();

  system_tray_->SetClient(this);

  DCHECK(!g_system_tray_client_instance);
  g_system_tray_client_instance = this;
  UpgradeDetector::GetInstance()->AddObserver(this);
}

SystemTrayClientImpl::~SystemTrayClientImpl() {
  DCHECK_EQ(this, g_system_tray_client_instance);
  g_system_tray_client_instance = nullptr;

  // This can happen when mocking this class in tests.
  if (!system_tray_)
    return;

  system_tray_->SetClient(nullptr);

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::DeviceCloudPolicyManagerAsh* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  if (policy_manager)
    policy_manager->core()->store()->RemoveObserver(this);

  g_browser_process->platform_part()->GetSystemClock()->RemoveObserver(this);
  UpgradeDetector::GetInstance()->RemoveObserver(this);
}

// static
SystemTrayClientImpl* SystemTrayClientImpl::Get() {
  return g_system_tray_client_instance;
}

void SystemTrayClientImpl::SetRelaunchNotificationState(
    const ash::RelaunchNotificationState& relaunch_notification_state) {
  relaunch_notification_state_ = relaunch_notification_state;
  HandleUpdateAvailable();
}

void SystemTrayClientImpl::ResetUpdateState() {
  relaunch_notification_state_ = {};
  system_tray_->ResetUpdateState();
}

void SystemTrayClientImpl::SetPrimaryTrayEnabled(bool enabled) {
  system_tray_->SetPrimaryTrayEnabled(enabled);
}

void SystemTrayClientImpl::SetPrimaryTrayVisible(bool visible) {
  system_tray_->SetPrimaryTrayVisible(visible);
}

void SystemTrayClientImpl::SetPerformanceTracingIconVisible(bool visible) {
  system_tray_->SetPerformanceTracingIconVisible(visible);
}

void SystemTrayClientImpl::SetLocaleList(
    std::vector<ash::LocaleInfo> locale_list,
    const std::string& current_locale_iso_code) {
  system_tray_->SetLocaleList(std::move(locale_list), current_locale_iso_code);
}

void SystemTrayClientImpl::SetShowEolNotice(bool show,
                                            bool eol_passed_recently) {
  eol_incentive_recently_passed_ = eol_passed_recently;
  system_tray_->SetShowEolNotice(show);
}
////////////////////////////////////////////////////////////////////////////////
// ash::SystemTrayClient:

void SystemTrayClientImpl::ShowSettings(int64_t display_id) {
  // TODO(jamescook): Use different metric for OS settings.
  base::RecordAction(base::UserMetricsAction("ShowOptions"));
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetActiveUserProfile(), display_id);
}

void SystemTrayClientImpl::ShowAccountSettings() {
  // The "Accounts" section is called "People" for historical reasons.
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kPeopleSectionPath);
}

void SystemTrayClientImpl::ShowBluetoothSettings() {
  base::RecordAction(base::UserMetricsAction("ShowBluetoothSettingsPage"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kBluetoothDevicesSubpagePath);
}

void SystemTrayClientImpl::ShowBluetoothSettings(const std::string& device_id) {
  base::RecordAction(base::UserMetricsAction("ShowBluetoothSettingsPage"));
  ShowSettingsSubPageForActiveUser(base::StrCat(
      {chromeos::settings::mojom::kBluetoothDeviceDetailSubpagePath,
       "?id=", device_id}));
}

void SystemTrayClientImpl::ShowBluetoothPairingDialog(
    std::optional<std::string_view> device_address) {
  if (ash::BluetoothPairingDialog::ShowDialog(device_address)) {
    base::RecordAction(
        base::UserMetricsAction("StatusArea_Bluetooth_Connect_Unknown"));
  }
}

void SystemTrayClientImpl::ShowDateSettings() {
  base::RecordAction(base::UserMetricsAction("ShowDateOptions"));
  // Everybody can change the time zone (even though it is a device setting).
  ShowSettingsSubPageForActiveUser(
      ash::features::IsOsSettingsRevampWayfindingEnabled()
          ? chromeos::settings::mojom::kSystemPreferencesSectionPath
          : chromeos::settings::mojom::kDateAndTimeSectionPath);
}

void SystemTrayClientImpl::ShowSetTimeDialog() {
  ash::SetTimeDialog::ShowDialog();
}

void SystemTrayClientImpl::ShowDisplaySettings() {
  base::RecordAction(base::UserMetricsAction("ShowDisplayOptions"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kDisplaySubpagePath);
}

void SystemTrayClientImpl::ShowDarkModeSettings() {
  // Record entry point metric to Personalization through Dark Mode Quick
  // Settings/System Tray.
  ash::personalization_app::LogPersonalizationEntryPoint(
      ash::PersonalizationEntryPoint::kSystemTray);
  ash::NewWindowDelegate::GetPrimary()->OpenPersonalizationHub();
}

void SystemTrayClientImpl::ShowStorageSettings() {
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kStorageSubpagePath);
}

void SystemTrayClientImpl::ShowPowerSettings() {
  base::RecordAction(base::UserMetricsAction("Tray_ShowPowerOptions"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kPowerSubpagePath);
}

void SystemTrayClientImpl::ShowPrivacyAndSecuritySettings() {
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kPrivacyAndSecuritySectionPath);
}

void SystemTrayClientImpl::ShowPrivacyHubSettings() {
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kPrivacyHubSubpagePath);
}

void SystemTrayClientImpl::ShowSpeakOnMuteDetectionSettings() {
  ShowSettingsSubPageForActiveUser(
      std::string(chromeos::settings::mojom::kPrivacyHubSubpagePath) +
      "?settingId=" +
      base::NumberToString(static_cast<int32_t>(
          chromeos::settings::mojom::Setting::kSpeakOnMuteDetectionOnOff)));
}

void SystemTrayClientImpl::ShowSmartPrivacySettings() {
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kSmartPrivacySubpagePath);
}

void SystemTrayClientImpl::ShowChromeSlow() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetPrimaryUserProfile());
  chrome::ShowSlow(displayer.browser());
}

void SystemTrayClientImpl::ShowIMESettings() {
  base::RecordAction(base::UserMetricsAction("OpenLanguageOptionsDialog"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kInputSubpagePath);
}

void SystemTrayClientImpl::ShowConnectedDevicesSettings() {
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kMultiDeviceFeaturesSubpagePath);
}

void SystemTrayClientImpl::ShowTetherNetworkSettings() {
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kMobileDataNetworksSubpagePath);
}

void SystemTrayClientImpl::ShowWifiSyncSettings() {
  ShowSettingsSubPageForActiveUser(
      std::string(chromeos::settings::mojom::kMultiDeviceFeaturesSubpagePath) +
      "?settingId=" +
      base::NumberToString(static_cast<int32_t>(
          chromeos::settings::mojom::Setting::kWifiSyncOnOff)));
}

void SystemTrayClientImpl::ShowAboutChromeOS() {
  // We always want to check for updates when showing the about page from the
  // Ash UI.
  ShowSettingsSubPageForActiveUser(
      std::string(chromeos::settings::mojom::kAboutChromeOsSectionPath) +
      "?checkForUpdate=true");
}

void SystemTrayClientImpl::ShowAboutChromeOSDetails() {
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kDetailedBuildInfoSubpagePath);
}

void SystemTrayClientImpl::ShowAccessibilityHelp() {
  ash::AccessibilityManager::ShowAccessibilityHelp();
}

void SystemTrayClientImpl::ShowAccessibilitySettings() {
  base::RecordAction(base::UserMetricsAction("ShowAccessibilitySettings"));
  // TODO(crbug.com/1358729): We show the old Manage Accessibility page in kiosk
  // mode, so users can't get to other OS Settings (such as Wi-Fi, Date / Time).
  // We plan to remove this after we add a standalone OS Accessibility page for
  // kiosk mode, which blocks access to other OS settings.
  ShowSettingsSubPageForActiveUser(
      user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()
          ? chromeos::settings::mojom::kManageAccessibilitySubpagePath
          : chromeos::settings::mojom::kAccessibilitySectionPath);
}

void SystemTrayClientImpl::ShowColorCorrectionSettings() {
  if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    // TODO(b/259370808): Color correction settings subpage not available in
    // Kiosk.
    return;
  }
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kDisplayAndMagnificationSubpagePath);
}

void SystemTrayClientImpl::ShowGestureEducationHelp() {
  base::RecordAction(base::UserMetricsAction("ShowGestureEducationHelp"));
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return;

  ash::SystemAppLaunchParams params;
  params.url = GURL(chrome::kChromeOSGestureEducationHelpURL);
  params.launch_source = apps::LaunchSource::kFromOtherApp;
  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::HELP, params);
}

void SystemTrayClientImpl::ShowPaletteHelp() {
  ShowSingletonTab(ProfileManager::GetActiveUserProfile(),
                   GURL(chrome::kChromePaletteHelpURL));
}

void SystemTrayClientImpl::ShowPaletteSettings() {
  base::RecordAction(base::UserMetricsAction("ShowPaletteOptions"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kStylusSubpagePath);
}

void SystemTrayClientImpl::ShowEnterpriseInfo() {
  // At the login screen, lock screen, etc. show enterprise help in a window.
  if (SessionManager::Get()->IsUserSessionBlocked()) {
    base::MakeRefCounted<ash::HelpAppLauncher>(/*parent_window=*/nullptr)
        ->ShowHelpTopic(ash::HelpAppLauncher::HELP_ENTERPRISE);
    return;
  }

  // Otherwise show enterprise management info page.
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::ShowEnterpriseManagementPageInTabbedBrowser(displayer.browser());
}

void SystemTrayClientImpl::ShowNetworkConfigure(const std::string& network_id) {
  // UI is not available at the lock screen.
  if (SessionManager::Get()->IsScreenLocked())
    return;

  DCHECK(ash::NetworkHandler::IsInitialized());
  const ash::NetworkState* network_state = GetNetworkState(network_id);
  if (!network_state) {
    LOG(ERROR) << "Network not found: " << network_id;
    return;
  }
  if (network_state->type() == ash::kTypeTether &&
      !network_state->tether_has_connected_to_host()) {
    ShowNetworkSettingsHelper(network_id, true /* show_configure */);
    return;
  }

  ash::InternetConfigDialog::ShowDialogForNetworkId(network_id);
}

void SystemTrayClientImpl::ShowNetworkCreate(const std::string& type) {
  if (type == ::onc::network_type::kCellular) {
    ShowSettingsCellularSetup(/*show_psim_flow=*/false);
    return;
  }
  ash::InternetConfigDialog::ShowDialogForNetworkType(type);
}

void SystemTrayClientImpl::ShowSettingsCellularSetup(bool show_psim_flow) {
  // TODO(crbug.com/40134918) Add metrics action recorder
  std::string page = chromeos::settings::mojom::kCellularNetworksSubpagePath;
  page += "&showCellularSetup=true";
  if (show_psim_flow)
    page += "&showPsimFlow=true";
  ShowSettingsSubPageForActiveUser(page);
}

void SystemTrayClientImpl::ShowMobileDataSubpage() {
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kCellularNetworksSubpagePath);
}

void SystemTrayClientImpl::ShowThirdPartyVpnCreate(
    const std::string& extension_id) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  if (!profile)
    return;

  // Request that the third-party VPN provider show its "add network" dialog.
  chromeos::VpnServiceFactory::GetForBrowserContext(profile)
      ->SendShowAddDialogToExtension(extension_id);
}

void SystemTrayClientImpl::ShowArcVpnCreate(const std::string& app_id) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  if (!profile ||
      !apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return;
  }

  apps::AppServiceProxyFactory::GetForProfile(profile)->Launch(
      app_id, ui::EF_NONE, apps::LaunchSource::kFromParentalControls);
}

void SystemTrayClientImpl::ShowSettingsSimUnlock() {
  // TODO(crbug.com/40134918) Add metrics action recorder.
  SessionManager* const session_manager = SessionManager::Get();
  DCHECK(session_manager->IsSessionStarted());
  DCHECK(!session_manager->IsInSecondaryLoginScreen());
  std::string page = chromeos::settings::mojom::kCellularNetworksSubpagePath;
  page += "&showSimLockDialog=true";
  ShowSettingsSubPageForActiveUser(page);
}

void SystemTrayClientImpl::ShowApnSubpage(const std::string& network_id) {
  CHECK(ash::features::IsApnRevampEnabled());
  std::string page = chromeos::settings::mojom::kApnSubpagePath +
                     std::string("?guid=") +
                     base::EscapeUrlEncodedData(network_id, /*use_plus=*/true);
  ShowSettingsSubPageForActiveUser(page);
}

void SystemTrayClientImpl::ShowNetworkSettings(const std::string& network_id) {
  ShowNetworkSettingsHelper(network_id, false /* show_configure */);
}

void SystemTrayClientImpl::ShowHotspotSubpage() {
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kHotspotSubpagePath);
}

void SystemTrayClientImpl::ShowNetworkSettingsHelper(
    const std::string& network_id,
    bool show_configure) {
  SessionManager* const session_manager = SessionManager::Get();
  if (session_manager->IsInSecondaryLoginScreen())
    return;
  if (!session_manager->IsSessionStarted()) {
    ash::InternetDetailDialog::ShowDialog(network_id);
    return;
  }

  if (ShouldOpenCellularSetupPsimFlowOnClick(network_id)) {
    // Special case: clicking "click to activate" on a network item should open
    // the cellular setup dialogs' pSIM flow if the network is a non-activated
    // cellular network.
    ShowSettingsCellularSetup(/*show_psim_flow=*/true);
    return;
  }

  std::string page = chromeos::settings::mojom::kNetworkSectionPath;
  const ash::NetworkState* network_state = GetNetworkState(network_id);
  if (!network_id.empty() && network_state) {
    // TODO(khorimoto): Use a more general path name here. This path is named
    // kWifi*, but it's actually a generic page.
    page = chromeos::settings::mojom::kWifiDetailsSubpagePath;
    page += "?guid=";
    page += base::EscapeUrlEncodedData(network_id, true);
    page += "&name=";
    page += base::EscapeUrlEncodedData(network_state->name(), true);
    page += "&type=";
    page += base::EscapeUrlEncodedData(
        ash::network_util::TranslateShillTypeToONC(network_state->type()),
        true);
    page += "&settingId=";
    page += base::NumberToString(static_cast<int32_t>(
        chromeos::settings::mojom::Setting::kDisconnectWifiNetwork));
    if (show_configure)
      page += "&showConfigure=true";
  }
  base::RecordAction(base::UserMetricsAction("OpenInternetOptionsDialog"));
  ShowSettingsSubPageForActiveUser(page);
}

void SystemTrayClientImpl::ShowMultiDeviceSetup() {
  ash::multidevice_setup::MultiDeviceSetupDialog::Show();
}

void SystemTrayClientImpl::ShowFirmwareUpdate() {
  chrome::ShowFirmwareUpdatesApp(ProfileManager::GetActiveUserProfile());
}

void SystemTrayClientImpl::SetLocaleAndExit(
    const std::string& locale_iso_code) {
  ProfileManager::GetActiveUserProfile()->ChangeAppLocale(
      locale_iso_code, Profile::APP_LOCALE_CHANGED_VIA_SYSTEM_TRAY);
  chrome::AttemptUserExit();
}

void SystemTrayClientImpl::ShowAccessCodeCastingDialog(
    AccessCodeCastDialogOpenLocation open_location) {
  media_router::AccessCodeCastDialog::ShowForDesktopMirroring(open_location);
}

void SystemTrayClientImpl::ShowCalendarEvent(
    const std::optional<GURL>& event_url,
    const base::Time& date,
    bool& opened_pwa,
    GURL& final_event_url) {
  // Default is that we didn't open the calendar PWA.
  opened_pwa = false;

  // Calendar URL we'll actually open, today's date by default.
  GURL official_url(kOfficialCalendarUrlPrefix);

  // Compose the actual URL to be opened.
  if (event_url.has_value()) {
    // An event URL was passed in, so modify it as needed for us to pass the "in
    // app scope" guards in WebAppLaunchProcess::Run().  See http://b/214428922
    GURL::Replacements replacements;
    replacements.SetSchemeStr("https");
    replacements.SetHostStr("calendar.google.com");
    official_url = event_url->ReplaceComponents(replacements);
  } else {
    // No event URL provided, so fall back on opening calendar with `date`.
    official_url = GURL(kOfficialCalendarUrlPrefix +
                        base::UnlocalizedTimeFormatWithPattern(
                            date, "'r/week/'y/M/d", icu::TimeZone::getGMT()));
  }

  // Return the URL we actually opened.
  final_event_url = official_url;

  // Check calendar web app installation.
  if (!IsAppInstalled(web_app::kGoogleCalendarAppId)) {
    OpenInBrowser(official_url);
    return;
  }

  // Need this in order to launch the web app.
  apps::AppServiceProxyAsh* proxy = GetActiveUserAppServiceProxyAsh();
  if (!proxy) {
    LOG(ERROR) << __FUNCTION__
               << " failed to get active user AppServiceProxyAsh";
    OpenInBrowser(official_url);
    return;
  }

  // Launch web app.
  proxy->LaunchAppWithUrl(web_app::kGoogleCalendarAppId, ui::EF_NONE,
                          official_url, apps::LaunchSource::kFromShelf);
  opened_pwa = true;
}

// TODO(b/269075177): Reuse existing Google Meet PWA instead of opening a new
// one for each call to `LaunchAppWithUrl`.
void SystemTrayClientImpl::ShowVideoConference(
    const GURL& video_conference_url) {
  if (auto* profile = ProfileManager::GetActiveUserProfile()) {
    apps::MaybeLaunchPreferredAppForUrl(
        profile, video_conference_url,
        apps::LaunchSource::kFromSysTrayCalendar);
  }
}

void SystemTrayClientImpl::ShowChannelInfoAdditionalDetails() {
  base::RecordAction(
      base::UserMetricsAction("Tray_ShowChannelInfoAdditionalDetails"));
  ShowSettingsSubPageForActiveUser(
      std::string(chromeos::settings::mojom::kDetailedBuildInfoSubpagePath));
}

void SystemTrayClientImpl::ShowChannelInfoGiveFeedback() {
  ash::NewWindowDelegate::GetInstance()->OpenFeedbackPage(
      ash::NewWindowDelegate::kFeedbackSourceChannelIndicator);
}

void SystemTrayClientImpl::ShowAudioSettings() {
  base::RecordAction(base::UserMetricsAction("ShowAudioSettingsPage"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kAudioSubpagePath);
}

void SystemTrayClientImpl::ShowGraphicsTabletSettings() {
  DCHECK(ash::features::IsPeripheralCustomizationEnabled());
  base::RecordAction(base::UserMetricsAction("ShowGraphicsTabletSettingsPage"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kGraphicsTabletSubpagePath);
}

void SystemTrayClientImpl::ShowMouseSettings() {
  DCHECK(ash::features::IsPeripheralCustomizationEnabled());
  base::RecordAction(base::UserMetricsAction("ShowMouseSettingsPage"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kPerDeviceMouseSubpagePath);
}

void SystemTrayClientImpl::ShowKeyboardSettings() {
  DCHECK(ash::features::IsWelcomeExperienceEnabled());
  base::RecordAction(base::UserMetricsAction("ShowKeyboardSettingsPage"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kPerDeviceKeyboardSubpagePath);
}

void SystemTrayClientImpl::ShowTouchpadSettings() {
  DCHECK(ash::features::IsInputDeviceSettingsSplitEnabled());
  base::RecordAction(base::UserMetricsAction("ShowTouchpadSettingsPage"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kPerDeviceTouchpadSubpagePath);
}

void SystemTrayClientImpl::ShowPointingStickSettings() {
  DCHECK(ash::features::IsWelcomeExperienceEnabled());
  base::RecordAction(base::UserMetricsAction("ShowPointingStickSettingsPage"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kPerDevicePointingStickSubpagePath);
}

void SystemTrayClientImpl::ShowNearbyShareSettings() {
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kNearbyShareSubpagePath);
}

void SystemTrayClientImpl::ShowRemapKeysSubpage(int device_id) {
  DCHECK(ash::features::IsInputDeviceSettingsSplitEnabled());
  base::RecordAction(base::UserMetricsAction("ShowRemapKeysSettingsSubpage"));
  ShowSettingsSubPageForActiveUser(base::StrCat({
      chromeos::settings::mojom::kPerDeviceKeyboardRemapKeysSubpagePath,
      "?keyboardId=",
      base::NumberToString(device_id),
  }));
}

void SystemTrayClientImpl::ShowYouTubeMusicPremiumPage() {
  DCHECK(ash::features::IsFocusModeEnabled());
  DCHECK(ash::features::IsFocusModeYTMEnabled());
  base::RecordAction(base::UserMetricsAction("ShowYouTubeMusicPremiumPage"));

  const GURL official_url(chrome::kYoutubeMusicPremiumURL);

  // Check YouTube Music web app installation.
  if (!IsAppInstalled(web_app::kYoutubeMusicAppId)) {
    OpenInBrowser(official_url);
    return;
  }

  // Need this in order to launch the web app.
  apps::AppServiceProxyAsh* proxy = GetActiveUserAppServiceProxyAsh();
  if (!proxy) {
    LOG(ERROR) << " failed to get active user AppServiceProxyAsh";
    OpenInBrowser(official_url);
    return;
  }

  // Launch web app.
  proxy->LaunchAppWithUrl(
      web_app::kYoutubeMusicAppId, ui::EF_NONE, official_url,
      apps::LaunchSource::kFromFocusMode, /*window_info=*/nullptr,
      base::BindOnce(
          [](const GURL& url, apps::LaunchResult&& result) {
            if (result.state != apps::LaunchResult::State::kSuccess) {
              OpenInBrowser(url);
            }
          },
          official_url));
}

void SystemTrayClientImpl::ShowChromebookPerksYouTubePage() {
  DCHECK(ash::features::IsFocusModeEnabled());
  DCHECK(ash::features::IsFocusModeYTMEnabled());
  OpenInBrowser(GURL(chrome::kChromebookPerksYouTubePage));
}

void SystemTrayClientImpl::ShowEolInfoPage() {
  const bool use_offer_url = ash::features::kEolIncentiveParam.Get() !=
                                 ash::features::EolIncentiveParam::kNoOffer &&
                             eol_incentive_recently_passed_;

  if (eol_incentive_recently_passed_) {
    ash::eol_incentive_util::RecordButtonClicked(
        use_offer_url ? ash::eol_incentive_util::EolIncentiveButtonType::
                            kQuickSettings_Offer_RecentlyPassed
                      : ash::eol_incentive_util::EolIncentiveButtonType::
                            kQuickSettings_NoOffer_RecentlyPassed);
  } else {
    DCHECK(!use_offer_url);
    ash::eol_incentive_util::RecordButtonClicked(
        ash::eol_incentive_util::EolIncentiveButtonType::
            kQuickSettings_NoOffer_Passed);
  }

  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(use_offer_url ? chrome::kEolIncentiveNotificationOfferURL
                         : chrome::kEolIncentiveNotificationNoOfferURL),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

void SystemTrayClientImpl::RecordEolNoticeShown() {
  ash::eol_incentive_util::RecordShowSourceHistogram(
      ash::eol_incentive_util::EolIncentiveShowSource::kQuickSettings);
}

bool SystemTrayClientImpl::IsUserFeedbackEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kForceShowReleaseTrack)) {
    // Force the release track UI to show the feedback button.
    return true;
  }
  PrefService* signin_prefs =
      ProfileManager::GetActiveUserProfile()->GetPrefs();
  DCHECK(signin_prefs);
  return signin_prefs->GetBoolean(prefs::kUserFeedbackAllowed);
}

SystemTrayClientImpl::SystemTrayClientImpl(SystemTrayClientImpl* mock_instance)
    : system_tray_(nullptr) {
  DCHECK(!g_system_tray_client_instance);
  g_system_tray_client_instance = mock_instance;
}

void SystemTrayClientImpl::HandleUpdateAvailable() {
  UpgradeDetector* detector = UpgradeDetector::GetInstance();
  if (detector->upgrade_notification_stage() ==
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE) {
    // Close any existing notifications.
    ResetUpdateState();
    return;
  }

  if (!detector->notify_upgrade()) {
    LOG(ERROR) << "Tried to show update notification when no update available";
    return;
  }

  // Show the system tray icon.
  ash::UpdateSeverity severity = GetUpdateSeverity(detector);
  system_tray_->ShowUpdateIcon(severity, detector->is_factory_reset_required(),
                               detector->is_rollback());

  // Overwrite title and body.
  system_tray_->SetRelaunchNotificationState(relaunch_notification_state_);
}

////////////////////////////////////////////////////////////////////////////////
// chromeos::system::SystemClockObserver:

void SystemTrayClientImpl::OnSystemClockChanged(
    ash::system::SystemClock* clock) {
  system_tray_->SetUse24HourClock(clock->ShouldUse24HourClock());
}

////////////////////////////////////////////////////////////////////////////////
// UpgradeDetector::UpgradeObserver:
void SystemTrayClientImpl::OnUpdateDeferred(bool use_notification) {
  system_tray_->SetUpdateDeferred(
      use_notification ? ash::DeferredUpdateState::kShowNotification
                       : ash::DeferredUpdateState::kShowDialog);
}

void SystemTrayClientImpl::OnUpdateOverCellularAvailable() {
  // Requests that ash show the update over cellular available icon.
  system_tray_->SetUpdateOverCellularAvailableIconVisible(true);
}

void SystemTrayClientImpl::OnUpdateOverCellularOneTimePermissionGranted() {
  // Requests that ash hide the update over cellular available icon.
  system_tray_->SetUpdateOverCellularAvailableIconVisible(false);
}

void SystemTrayClientImpl::OnUpgradeRecommended() {
  HandleUpdateAvailable();
}

////////////////////////////////////////////////////////////////////////////////
// policy::CloudPolicyStore::Observer
void SystemTrayClientImpl::OnStoreLoaded(policy::CloudPolicyStore* store) {
  UpdateDeviceEnterpriseInfo();
}

void SystemTrayClientImpl::OnStoreError(policy::CloudPolicyStore* store) {
  UpdateDeviceEnterpriseInfo();
}

void SystemTrayClientImpl::UpdateDeviceEnterpriseInfo() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  ash::DeviceEnterpriseInfo device_enterprise_info;
  device_enterprise_info.enterprise_domain_manager =
      connector->GetEnterpriseDomainManager();
  device_enterprise_info.management_device_mode =
      GetManagementDeviceMode(connector);
  if (!last_device_enterprise_info_) {
    last_device_enterprise_info_ =
        std::make_unique<ash::DeviceEnterpriseInfo>();
  }

  if (device_enterprise_info == *last_device_enterprise_info_)
    return;

  // Send to ash, which will add an item to the system tray.
  system_tray_->SetDeviceEnterpriseInfo(device_enterprise_info);
  *last_device_enterprise_info_ = device_enterprise_info;
}

void SystemTrayClientImpl::UpdateEnterpriseAccountDomainInfo(Profile* profile) {
  std::string account_manager =
      profile
          ? chrome::GetAccountManagerIdentity(profile).value_or(std::string())
          : std::string();
  if (account_manager == last_enterprise_account_domain_manager_)
    return;

  // Send to ash, which will add an item to the system tray.
  system_tray_->SetEnterpriseAccountDomainInfo(account_manager);
  last_enterprise_account_domain_manager_ = account_manager;
}
