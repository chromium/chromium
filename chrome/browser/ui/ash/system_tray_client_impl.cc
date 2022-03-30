// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_client_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/locale_update_controller.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/system_tray.h"
#include "ash/public/cpp/update_types.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/set_time_dialog.h"
#include "chrome/browser/ash/system/system_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_ui.h"
#include "chrome/browser/ui/webui/chromeos/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_config_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/setting.mojom.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/url_constants.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/network_onc_utils.h"
#include "chromeos/network/tether_constants.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/api/vpn_provider/vpn_service.h"
#include "extensions/browser/api/vpn_provider/vpn_service_factory.h"
#include "net/base/escape.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

using chromeos::DBusThreadManager;
using chromeos::UpdateEngineClient;
using session_manager::SessionManager;
using session_manager::SessionState;

namespace {

SystemTrayClientImpl* g_system_tray_client_instance = nullptr;

// The prefix a calendar event URL *must* have in order to be launched by the
// calendar web app.
const char* kOfficialCalendarUrlPrefix =
    "https://calendar.google.com/calendar/";

void ShowSettingsSubPageForActiveUser(const std::string& sub_page) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetActiveUserProfile(), sub_page);
}

// Returns the severity of a pending update.
ash::UpdateSeverity GetUpdateSeverity(ash::UpdateType update_type,
                                      UpgradeDetector* detector) {
  // Lacros is always "low", which is the same severity OS updates start with.
  if (update_type == ash::UpdateType::kLacros)
    return ash::UpdateSeverity::kLow;

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

const chromeos::NetworkState* GetNetworkState(const std::string& network_id) {
  if (network_id.empty())
    return nullptr;
  return chromeos::NetworkHandler::Get()
      ->network_state_handler()
      ->GetNetworkStateFromGuid(network_id);
}

bool ShouldOpenCellularSetupPsimFlowOnClick(const std::string& network_id) {
  // |kActivationStateNotActivated| is only set in physical SIM networks,
  // checking a networks activation state is |kActivationStateNotActivated|
  // ensures the current network is a phyical SIM network.

  const chromeos::NetworkState* network_state = GetNetworkState(network_id);
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
  ash::NewWindowDelegate* primary_delegate =
      ash::NewWindowDelegate::GetPrimary();
  if (!primary_delegate) {
    LOG(ERROR) << __FUNCTION__ << " failed to get primary window delegate";
    return;
  }

  primary_delegate->OpenUrl(event_url,
                            ash::NewWindowDelegate::OpenUrlFrom::kUnspecified);
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
  SystemTrayClientImpl* const owner_;
  Profile* profile_ = nullptr;

  base::ScopedObservation<
      user_manager::UserManager,
      user_manager::UserManager::UserSessionStateObserver,
      &user_manager::UserManager::AddSessionStateObserver,
      &user_manager::UserManager::RemoveSessionStateObserver>
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
  void OnSessionStateChanged() override { UpdateProfile(); }

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
  chromeos::system::SystemClock* clock =
      g_browser_process->platform_part()->GetSystemClock();
  clock->AddObserver(this);
  system_tray_->SetUse24HourClock(clock->ShouldUse24HourClock());

  // If an upgrade is available at startup then tell ash about it.
  if (UpgradeDetector::GetInstance()->notify_upgrade())
    HandleUpdateAvailable(ash::UpdateType::kSystem);

  // If the device is enterprise managed then send ash the enterprise domain.
  policy::BrowserPolicyConnectorAsh* policy_connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::DeviceCloudPolicyManagerAsh* policy_manager =
      policy_connector->GetDeviceCloudPolicyManager();
  if (policy_manager)
    policy_manager->core()->store()->AddObserver(this);
  UpdateEnterpriseDomainInfo();

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
  HandleUpdateAvailable(ash::UpdateType::kSystem);
}

void SystemTrayClientImpl::ResetUpdateState() {
  relaunch_notification_state_ = {};
  system_tray_->ResetUpdateState();
}

void SystemTrayClientImpl::SetLacrosUpdateAvailable() {
  HandleUpdateAvailable(ash::UpdateType::kLacros);
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

////////////////////////////////////////////////////////////////////////////////
// ash::SystemTrayClient:

void SystemTrayClientImpl::ShowSettings(int64_t display_id) {
  // TODO(jamescook): Use different metric for OS settings.
  base::RecordAction(base::UserMetricsAction("ShowOptions"));
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetActiveUserProfile(), display_id);
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
    absl::optional<base::StringPiece> device_address) {
  if (chromeos::BluetoothPairingDialog::ShowDialog(device_address)) {
    base::RecordAction(
        base::UserMetricsAction("StatusArea_Bluetooth_Connect_Unknown"));
  }
}

void SystemTrayClientImpl::ShowDateSettings() {
  base::RecordAction(base::UserMetricsAction("ShowDateOptions"));
  // Everybody can change the time zone (even though it is a device setting).
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kDateAndTimeSectionPath);
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
  if (ash::features::IsPersonalizationHubEnabled()) {
    ash::NewWindowDelegate* primary_delegate =
        ash::NewWindowDelegate::GetPrimary();
    primary_delegate->OpenPersonalizationHub();
    return;
  }
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kDarkModeSubpagePath);
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

void SystemTrayClientImpl::ShowAccessibilityHelp() {
  ash::AccessibilityManager::ShowAccessibilityHelp();
}

void SystemTrayClientImpl::ShowAccessibilitySettings() {
  base::RecordAction(base::UserMetricsAction("ShowAccessibilitySettings"));
  ShowSettingsSubPageForActiveUser(
      chromeos::settings::mojom::kManageAccessibilitySubpagePath);
}

void SystemTrayClientImpl::ShowGestureEducationHelp() {
  base::RecordAction(base::UserMetricsAction("ShowGestureEducationHelp"));
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return;

  web_app::SystemAppLaunchParams params;
  params.url = GURL(chrome::kChromeOSGestureEducationHelpURL);
  params.launch_source = apps::mojom::LaunchSource::kFromOtherApp;
  web_app::LaunchSystemWebAppAsync(profile, web_app::SystemAppType::HELP,
                                   params);
}

void SystemTrayClientImpl::ShowPaletteHelp() {
  if (crosapi::browser_util::IsLacrosPrimaryBrowser()) {
    crosapi::BrowserManager::Get()->SwitchToTab(
        GURL(chrome::kChromePaletteHelpURL));
    return;
  }

  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  ShowSingletonTab(displayer.browser(), GURL(chrome::kChromePaletteHelpURL));
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
  if (crosapi::browser_util::IsLacrosPrimaryBrowser()) {
    crosapi::BrowserManager::Get()->SwitchToTab(
        GURL(chrome::kChromeUIManagementURL));
    return;
  }

  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::ShowEnterpriseManagementPageInTabbedBrowser(displayer.browser());
}

void SystemTrayClientImpl::ShowNetworkConfigure(const std::string& network_id) {
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

void SystemTrayClientImpl::ShowNetworkCreate(const std::string& type) {
  if (type == ::onc::network_type::kCellular) {
    ShowSettingsCellularSetup(/*show_psim_flow=*/false);
    return;
  }
  chromeos::InternetConfigDialog::ShowDialogForNetworkType(type);
}

void SystemTrayClientImpl::ShowSettingsCellularSetup(bool show_psim_flow) {
  // TODO(crbug.com/1093185) Add metrics action recorder
  std::string page = chromeos::settings::mojom::kCellularNetworksSubpagePath;
  page += "&showCellularSetup=true";
  if (show_psim_flow)
    page += "&showPsimFlow=true";
  ShowSettingsSubPageForActiveUser(page);
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
      app_id, ui::EF_NONE, apps::mojom::LaunchSource::kFromParentalControls);
}

void SystemTrayClientImpl::ShowSettingsSimUnlock() {
  // TODO(https://crbug.com/1093185) Add metrics action recorder.
  SessionManager* const session_manager = SessionManager::Get();
  DCHECK(session_manager->IsSessionStarted());
  DCHECK(!session_manager->IsInSecondaryLoginScreen());
  std::string page = chromeos::settings::mojom::kCellularNetworksSubpagePath;
  page += "&showSimLockDialog=true";
  ShowSettingsSubPageForActiveUser(page);
}

void SystemTrayClientImpl::ShowNetworkSettings(const std::string& network_id) {
  ShowNetworkSettingsHelper(network_id, false /* show_configure */);
}

void SystemTrayClientImpl::ShowNetworkSettingsHelper(
    const std::string& network_id,
    bool show_configure) {
  SessionManager* const session_manager = SessionManager::Get();
  if (session_manager->IsInSecondaryLoginScreen())
    return;
  if (!session_manager->IsSessionStarted()) {
    chromeos::InternetDetailDialog::ShowDialog(network_id);
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
  chromeos::multidevice_setup::MultiDeviceSetupDialog::Show();
}

void SystemTrayClientImpl::ShowFirmwareUpdate() {
  chrome::ShowFirmwareUpdatesApp(ProfileManager::GetActiveUserProfile());
}

void SystemTrayClientImpl::RequestRestartForUpdate() {
  browser_shutdown::NotifyAndTerminate(/*fast_path=*/true);
}

void SystemTrayClientImpl::SetLocaleAndExit(
    const std::string& locale_iso_code) {
  ProfileManager::GetActiveUserProfile()->ChangeAppLocale(
      locale_iso_code, Profile::APP_LOCALE_CHANGED_VIA_SYSTEM_TRAY);
  chrome::AttemptUserExit();
}

void SystemTrayClientImpl::ShowAccessCodeCastingDialog() {
  AccessCodeCastDialog::ShowForDesktopMirroring();
}

void SystemTrayClientImpl::ShowCalendarEvent(
    const absl::optional<GURL>& event_url,
    bool& opened_pwa,
    GURL& final_event_url) {
  // Default is that we didn't open the calendar PWA.
  opened_pwa = false;

  // By default, open the calendar to no particular event, i.e. today's date.
  GURL official_url(kOfficialCalendarUrlPrefix);

  // Needed in order for us to pass the "in app scope" guards in
  // WebAppLaunchProcess::Run().  See http://b/214428922
  if (event_url.has_value()) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr("https");
    replacements.SetHostStr("calendar.google.com");
    official_url = event_url->ReplaceComponents(replacements);
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
  proxy->LaunchAppWithUrl(
      web_app::kGoogleCalendarAppId,
      apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                          WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true),
      official_url, apps::mojom::LaunchSource::kFromShelf);
  opened_pwa = true;
}

SystemTrayClientImpl::SystemTrayClientImpl(SystemTrayClientImpl* mock_instance)
    : system_tray_(nullptr) {
  DCHECK(!g_system_tray_client_instance);
  g_system_tray_client_instance = mock_instance;
}

void SystemTrayClientImpl::HandleUpdateAvailable(ash::UpdateType update_type) {
  UpgradeDetector* detector = UpgradeDetector::GetInstance();
  if (detector->upgrade_notification_stage() ==
      UpgradeDetector::UPGRADE_ANNOYANCE_NONE) {
    // Close any existing notifications.
    ResetUpdateState();
    return;
  }

  if (update_type == ash::UpdateType::kSystem && !detector->notify_upgrade()) {
    LOG(ERROR) << "Tried to show update notification when no update available";
    return;
  }

  // Show the system tray icon.
  ash::UpdateSeverity severity = GetUpdateSeverity(update_type, detector);
  system_tray_->ShowUpdateIcon(severity, detector->is_factory_reset_required(),
                               detector->is_rollback(), update_type);

  // Only overwrite title and body for system updates.
  if (update_type == ash::UpdateType::kSystem)
    system_tray_->SetRelaunchNotificationState(relaunch_notification_state_);
}

////////////////////////////////////////////////////////////////////////////////
// chromeos::system::SystemClockObserver:

void SystemTrayClientImpl::OnSystemClockChanged(
    chromeos::system::SystemClock* clock) {
  system_tray_->SetUse24HourClock(clock->ShouldUse24HourClock());
}

////////////////////////////////////////////////////////////////////////////////
// UpgradeDetector::UpgradeObserver:
void SystemTrayClientImpl::OnUpdateOverCellularAvailable() {
  // Requests that ash show the update over cellular available icon.
  system_tray_->SetUpdateOverCellularAvailableIconVisible(true);
}

void SystemTrayClientImpl::OnUpdateOverCellularOneTimePermissionGranted() {
  // Requests that ash hide the update over cellular available icon.
  system_tray_->SetUpdateOverCellularAvailableIconVisible(false);
}

void SystemTrayClientImpl::OnUpgradeRecommended() {
  HandleUpdateAvailable(ash::UpdateType::kSystem);
}

////////////////////////////////////////////////////////////////////////////////
// policy::CloudPolicyStore::Observer
void SystemTrayClientImpl::OnStoreLoaded(policy::CloudPolicyStore* store) {
  UpdateEnterpriseDomainInfo();
}

void SystemTrayClientImpl::OnStoreError(policy::CloudPolicyStore* store) {
  UpdateEnterpriseDomainInfo();
}

void SystemTrayClientImpl::UpdateEnterpriseDomainInfo() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  const std::string enterprise_domain_manager =
      connector->GetEnterpriseDomainManager();
  const bool active_directory_managed = connector->IsActiveDirectoryManaged();
  if (enterprise_domain_manager == last_enterprise_domain_manager_ &&
      active_directory_managed == last_active_directory_managed_) {
    return;
  }
  // Send to ash, which will add an item to the system tray.
  system_tray_->SetEnterpriseDomainInfo(enterprise_domain_manager,
                                        active_directory_managed);
  last_enterprise_domain_manager_ = enterprise_domain_manager;
  last_active_directory_managed_ = active_directory_managed;
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
