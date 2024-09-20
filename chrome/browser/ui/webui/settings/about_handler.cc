// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/about_handler.h"

#include <stddef.h>

#include <limits>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "v8/include/v8-version-string.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/eol/eol_incentive_util.h"
#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"
#include "chrome/browser/ash/image_source/image_source.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/tpm/tpm_firmware_update.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_dialog.h"
#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"
#include "chrome/browser/ui/webui/help/version_updater_chromeos.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/fwupd/firmware_update_manager.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/version/version_loader.h"
#include "components/user_manager/user_manager.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/chromeos/devicetype_utils.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)

using ash::eol_incentive_util::EolIncentiveType;

// The directory containing the regulatory labels for supported
// models/regions, relative to chromeos-assets directory
const char kRegulatoryLabelsDirectory[] = "regulatory_labels";

// File names of the image file and the file containing alt text for the label.
const char kRegulatoryLabelImageFilename[] = "label.png";
const char kRegulatoryLabelTextFilename[] = "label.txt";

// Default region code to use if there's no label for the VPD region code.
const char kDefaultRegionCode[] = "us";

struct RegulatoryLabel {
  const std::string label_text;
  const std::string image_url;
};

// Returns message that informs user that for update it's better to
// connect to a network of one of the allowed types.
std::u16string GetAllowedConnectionTypesMessage() {
  if (!help_utils_chromeos::IsUpdateOverCellularAllowed(
          /*interactive=*/true)) {
    return l10n_util::GetStringUTF16(
        IDS_UPGRADE_NETWORK_LIST_CELLULAR_DISALLOWED);
  }

  const bool metered = ash::NetworkHandler::Get()
                           ->network_state_handler()
                           ->default_network_is_metered();
  return l10n_util::GetStringUTF16(
      metered ? IDS_UPGRADE_NETWORK_LIST_CELLULAR_ALLOWED_NOT_AUTOMATIC
              : IDS_UPGRADE_NETWORK_LIST_CELLULAR_ALLOWED);
}

// Returns true if current user can change channel, false otherwise.
bool CanChangeChannel(Profile* profile) {
  if (policy::ManagementServiceFactory::GetForPlatform()->IsManaged()) {
    bool value = false;
    // On a managed machine we delegate this setting to the affiliated users
    // only if the policy value is true.
    ash::CrosSettings::Get()->GetBoolean(ash::kReleaseChannelDelegated, &value);
    if (!value)
      return false;

    // Get the currently logged-in user and check if it is affiliated.
    const user_manager::User* user =
        profile ? ash::ProfileHelper::Get()->GetUserByProfile(profile)
                : nullptr;
    return user && user->IsAffiliated();
  }

  // On non-managed machines, only the local owner can change the channel.
  ash::OwnerSettingsServiceAsh* service =
      ash::OwnerSettingsServiceAshFactory::GetInstance()->GetForBrowserContext(
          profile);
  return service && service->IsOwner();
}

// Returns the relative path under the chromeos-assets dir
// to the directory of regulatory labels for a given region, if found
// (e.g. "regulatory_labels/us"). Must be called from the blocking pool.
base::FilePath GetRegulatoryLabelDirForRegion(std::string_view region) {
  base::FilePath region_path(kRegulatoryLabelsDirectory);
  const std::string model_subdir =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kRegulatoryLabelDir);
  if (!model_subdir.empty()) {
    region_path = region_path.AppendASCII(model_subdir);
  }
  region_path = region_path.AppendASCII(region);

  // Check if the label image file exists in the full path, e.g.,
  // "/usr/share/chromeos-assets/regulatory_labels/us/label.png".
  const base::FilePath image_path =
      base::FilePath(chrome::kChromeOSAssetPath)
          .Append(region_path)
          .AppendASCII(kRegulatoryLabelImageFilename);
  return base::PathExists(image_path) ? region_path : base::FilePath();
}

// Finds the relative path under the chromeos-assets dir to the region
// subdirectory of regulatory labels, using the VPD region code. Also
// tries "us" as a fallback region. Must be called from the blocking pool.
base::FilePath FindRegulatoryLabelDir() {
  base::FilePath region_path;
  // Use the VPD region code to find the label dir.
  const std::optional<std::string_view> region =
      ash::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          ash::system::kRegionKey);
  if (region && !region->empty()) {
    region_path = GetRegulatoryLabelDirForRegion(region.value());
  }

  // Try the fallback region code if no directory was found.
  if (region_path.empty() && region != kDefaultRegionCode)
    region_path = GetRegulatoryLabelDirForRegion(kDefaultRegionCode);

  return region_path;
}

// Reads the file containing the regulatory label text, if found
// in the given relative path under the chromeos-assets dir.
// Must be called from the blocking pool.
std::string ReadRegulatoryLabelText(const base::FilePath& label_dir_path) {
  const base::FilePath text_path =
      base::FilePath(chrome::kChromeOSAssetPath)
          .Append(label_dir_path)
          .AppendASCII(kRegulatoryLabelTextFilename);

  std::string contents;
  if (base::ReadFileToString(text_path, &contents))
    return contents;
  return std::string();
}

base::Value::Dict GetVersionInfo() {
  base::Value::Dict version_info;
  std::optional<std::string> version = chromeos::version_loader::GetVersion(
      chromeos::version_loader::VERSION_FULL);
  version_info.Set("osVersion", version.value_or("0.0.0.0"));
  version_info.Set("arcVersion", chromeos::version_loader::GetArcVersion());
  version_info.Set("osFirmware", chromeos::version_loader::GetFirmware());
  return version_info;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::string UpdateStatusToString(VersionUpdater::Status status) {
  std::string status_str;
  switch (status) {
    case VersionUpdater::CHECKING:
      status_str = "checking";
      break;
    case VersionUpdater::UPDATING:
      status_str = "updating";
      break;
    case VersionUpdater::NEARLY_UPDATED:
      status_str = "nearly_updated";
      break;
    case VersionUpdater::UPDATED:
      status_str = "updated";
      break;
    case VersionUpdater::FAILED:
    case VersionUpdater::FAILED_OFFLINE:
    case VersionUpdater::FAILED_CONNECTION_TYPE_DISALLOWED:
      status_str = "failed";
      break;
    case VersionUpdater::FAILED_HTTP:
      status_str = "failed_http";
      break;
    case VersionUpdater::FAILED_DOWNLOAD:
      status_str = "failed_download";
      break;
    case VersionUpdater::DISABLED:
      status_str = "disabled";
      break;
    case VersionUpdater::DISABLED_BY_ADMIN:
      status_str = "disabled_by_admin";
      break;
    case VersionUpdater::UPDATE_TO_ROLLBACK_VERSION_DISALLOWED:
      status_str = "update_to_rollback_version_disallowed";
      break;
    case VersionUpdater::NEED_PERMISSION_TO_UPDATE:
      status_str = "need_permission_to_update";
      break;
    case VersionUpdater::DEFERRED:
      status_str = "deferred";
      break;
  }

  return status_str;
}

}  // namespace

namespace settings {

AboutHandler::AboutHandler(Profile* profile)
    : profile_(profile), clock_(base::DefaultClock::GetInstance()) {
  UpgradeDetector::GetInstance()->AddObserver(this);
}

AboutHandler::~AboutHandler() {
  UpgradeDetector::GetInstance()->RemoveObserver(this);
}

void AboutHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "aboutPageReady", base::BindRepeating(&AboutHandler::HandlePageReady,
                                            base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "refreshUpdateStatus",
      base::BindRepeating(&AboutHandler::HandleRefreshUpdateStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openFeedbackDialog",
      base::BindRepeating(&AboutHandler::HandleOpenFeedbackDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openHelpPage", base::BindRepeating(&AboutHandler::HandleOpenHelpPage,
                                          base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      "openDiagnostics",
      base::BindRepeating(&AboutHandler::HandleOpenDiagnostics,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "openFirmwareUpdatesPage",
      base::BindRepeating(&AboutHandler::HandleOpenFirmwareUpdates,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getFirmwareUpdateCount",
      base::BindRepeating(&AboutHandler::HandleGetFirmwareUpdateCount,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "openOsHelpPage", base::BindRepeating(&AboutHandler::HandleOpenOsHelpPage,
                                            base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setChannel", base::BindRepeating(&AboutHandler::HandleSetChannel,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "applyDeferredUpdate",
      base::BindRepeating(&AboutHandler::HandleApplyDeferredUpdate,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestUpdate", base::BindRepeating(&AboutHandler::HandleRequestUpdate,
                                           base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestUpdateOverCellular",
      base::BindRepeating(&AboutHandler::HandleRequestUpdateOverCellular,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getVersionInfo", base::BindRepeating(&AboutHandler::HandleGetVersionInfo,
                                            base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getRegulatoryInfo",
      base::BindRepeating(&AboutHandler::HandleGetRegulatoryInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getChannelInfo", base::BindRepeating(&AboutHandler::HandleGetChannelInfo,
                                            base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "canChangeChannel",
      base::BindRepeating(&AboutHandler::HandleCanChangeChannel,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "refreshTPMFirmwareUpdateStatus",
      base::BindRepeating(&AboutHandler::HandleRefreshTPMFirmwareUpdateStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getEndOfLifeInfo",
      base::BindRepeating(&AboutHandler::HandleGetEndOfLifeInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openEndOfLifeIncentive",
      base::BindRepeating(&AboutHandler::HandleOpenEndOfLifeIncentive,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "launchReleaseNotes",
      base::BindRepeating(&AboutHandler::HandleLaunchReleaseNotes,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "checkInternetConnection",
      base::BindRepeating(&AboutHandler::HandleCheckInternetConnection,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "isManagedAutoUpdateEnabled",
      base::BindRepeating(&AboutHandler::HandleIsManagedAutoUpdateEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "isConsumerAutoUpdateEnabled",
      base::BindRepeating(&AboutHandler::HandleIsConsumerAutoUpdateEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setConsumerAutoUpdate",
      base::BindRepeating(&AboutHandler::HandleSetConsumerAutoUpdate,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openProductLicenseOther",
      base::BindRepeating(&AboutHandler::HandleOpenProductLicenseOther,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "isExtendedUpdatesOptInEligible",
      base::BindRepeating(&AboutHandler::HandleIsExtendedUpdatesOptInEligible,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openExtendedUpdatesDialog",
      base::BindRepeating(&AboutHandler::HandleOpenExtendedUpdatesDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "recordExtendedUpdatesShown",
      base::BindRepeating(&AboutHandler::HandleRecordExtendedUpdatesShown,
                          base::Unretained(this)));
#endif
#if BUILDFLAG(IS_MAC)
  web_ui()->RegisterMessageCallback(
      "promoteUpdater", base::BindRepeating(&AboutHandler::PromoteUpdater,
                                            base::Unretained(this)));
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Handler for the product label image, which will be shown if available.
  content::URLDataSource::Add(profile_, std::make_unique<ash::ImageSource>());
#endif
  content::URLDataSource::Add(profile_,
                              std::make_unique<ThemeSource>(profile_));
}

void AboutHandler::OnJavascriptAllowed() {
  apply_changes_from_upgrade_observer_ = true;
  version_updater_ = VersionUpdater::Create(web_ui()->GetWebContents());
  policy_registrar_ = std::make_unique<policy::PolicyChangeRegistrar>(
      g_browser_process->policy_service(),
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
// TODO(b/330932781): Investigate and fix mismatched BUILDFLAG and comment.
#if BUILDFLAG(IS_CHROMEOS)
  policy_registrar_->Observe(
      policy::key::kDeviceAutoUpdateDisabled,
      base::BindRepeating(&AboutHandler::OnDeviceAutoUpdatePolicyChanged,
                          weak_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::CrosSettings::IsInitialized()) {
    extended_updates_setting_change_subscription_ =
        ash::CrosSettings::Get()->AddSettingsObserver(
            ash::kDeviceExtendedAutoUpdateEnabled,
            base::BindRepeating(&AboutHandler::OnExtendedUpdatesSettingChanged,
                                weak_factory_.GetWeakPtr()));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void AboutHandler::OnJavascriptDisallowed() {
  apply_changes_from_upgrade_observer_ = false;
  version_updater_.reset();
  policy_registrar_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

void AboutHandler::OnUpgradeRecommended() {
  if (apply_changes_from_upgrade_observer_) {
    // A version update is installed and ready to go. Refresh the UI so the
    // correct state will be shown.
    RequestUpdate();
  }
}

void AboutHandler::OnDeviceAutoUpdatePolicyChanged(
    const base::Value* previous_policy,
    const base::Value* current_policy) {
  bool previous_auto_update_disabled = false;
  if (previous_policy) {
    CHECK(previous_policy->is_bool());
    previous_auto_update_disabled = previous_policy->GetBool();
  }

  bool current_auto_update_disabled = false;
  if (current_policy) {
    CHECK(current_policy->is_bool());
    current_auto_update_disabled = current_policy->GetBool();
  }

  if (current_auto_update_disabled != previous_auto_update_disabled) {
    // Refresh the update status to refresh the status of the UI.
    RefreshUpdateStatus();
  }
}

void AboutHandler::HandlePageReady(const base::Value::List& args) {
  AllowJavascript();
}

void AboutHandler::HandleRefreshUpdateStatus(const base::Value::List& args) {
  RefreshUpdateStatus();
}

void AboutHandler::RefreshUpdateStatus() {
// On Chrome OS, do not check for an update automatically.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  static_cast<VersionUpdaterCros*>(version_updater_.get())
      ->GetUpdateStatus(base::BindRepeating(&AboutHandler::SetUpdateStatus,
                                            weak_factory_.GetWeakPtr()));
#else
  RequestUpdate();
#endif
}

#if BUILDFLAG(IS_MAC)
void AboutHandler::PromoteUpdater(const base::Value::List& args) {
  version_updater_->PromoteUpdater();
}
#endif

void AboutHandler::HandleOpenFeedbackDialog(const base::Value::List& args) {
  DCHECK(args.empty());
  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  chrome::OpenFeedbackDialog(browser,
                             feedback::kFeedbackSourceMdSettingsAboutPage);
}

void AboutHandler::HandleOpenHelpPage(const base::Value::List& args) {
  DCHECK(args.empty());
  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  chrome::ShowHelp(browser, chrome::HELP_SOURCE_WEBUI);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AboutHandler::HandleOpenDiagnostics(const base::Value::List& args) {
  DCHECK(args.empty());
  chrome::ShowDiagnosticsApp(profile_);
}

void AboutHandler::HandleOpenFirmwareUpdates(const base::Value::List& args) {
  DCHECK(args.empty());
  chrome::ShowFirmwareUpdatesApp(profile_);
}

void AboutHandler::HandleCheckInternetConnection(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();

  ash::NetworkStateHandler* network_state_handler =
      ash::NetworkHandler::Get()->network_state_handler();
  const ash::NetworkState* network = network_state_handler->DefaultNetwork();
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(network && network->IsOnline()));
}

void AboutHandler::HandleLaunchReleaseNotes(const base::Value::List& args) {
  DCHECK(args.empty());
  // We can always show the release notes since the Help app caches it, or can
  // show an appropriate error state (e.g. No internet connection).
  base::RecordAction(base::UserMetricsAction("ReleaseNotes.LaunchedAboutPage"));
  chrome::LaunchReleaseNotes(profile_, apps::LaunchSource::kFromOtherApp);
}

void AboutHandler::HandleOpenOsHelpPage(const base::Value::List& args) {
  DCHECK(args.empty());
  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  chrome::ShowHelp(browser, chrome::HELP_SOURCE_WEBUI_CHROME_OS);
}

void AboutHandler::HandleSetChannel(const base::Value::List& args) {
  DCHECK_EQ(2U, args.size());

  if (!CanChangeChannel(profile_)) {
    LOG(WARNING) << "Non-owner tried to change release track.";
    return;
  }

  if (!args[0].is_string() || !args[1].is_bool()) {
    LOG(ERROR) << "Can't parse SetChannel() args";
    return;
  }
  const std::string& channel = args[0].GetString();
  const bool& is_powerwash_allowed = args[1].GetBool();

  version_updater_->SetChannel(channel, is_powerwash_allowed);
  if (user_manager::UserManager::Get()->IsCurrentUserOwner()) {
    // Check for update after switching release channel.
    version_updater_->CheckForUpdate(
        base::BindRepeating(&AboutHandler::SetUpdateStatus,
                            weak_factory_.GetWeakPtr()),
        VersionUpdater::PromoteCallback());
  }
}

void AboutHandler::HandleGetVersionInfo(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetVersionInfo),
      base::BindOnce(&AboutHandler::OnGetVersionInfoReady,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void AboutHandler::OnGetVersionInfoReady(std::string callback_id,
                                         base::Value::Dict version_info) {
  ResolveJavascriptCallback(base::Value(callback_id), version_info);
}

void AboutHandler::HandleGetFirmwareUpdateCount(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();
  size_t update_count = 0u;
  if (!ash::FirmwareUpdateManager::IsInitialized()) {
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(static_cast<int>(update_count)));
    return;
  }

  auto* firmware_update_manager = ash::FirmwareUpdateManager::Get();
  update_count = firmware_update_manager->GetUpdateCount();
  DCHECK_LT(update_count, std::numeric_limits<size_t>::max());
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(static_cast<int>(update_count)));
}

void AboutHandler::HandleGetRegulatoryInfo(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&FindRegulatoryLabelDir),
      base::BindOnce(&AboutHandler::OnRegulatoryLabelDirFound,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void AboutHandler::HandleGetChannelInfo(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();
  version_updater_->GetChannel(
      true /* get current channel */,
      base::BindOnce(&AboutHandler::OnGetCurrentChannel,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void AboutHandler::HandleCanChangeChannel(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(CanChangeChannel(profile_)));
}

void AboutHandler::OnGetCurrentChannel(std::string callback_id,
                                       const std::string& current_channel) {
  version_updater_->GetChannel(
      false /* get target channel */,
      base::BindOnce(&AboutHandler::OnGetTargetChannel,
                     weak_factory_.GetWeakPtr(), callback_id, current_channel));
}

void AboutHandler::OnGetTargetChannel(std::string callback_id,
                                      const std::string& current_channel,
                                      const std::string& target_channel) {
  base::Value::Dict channel_info;
  channel_info.Set("currentChannel", current_channel);
  channel_info.Set("targetChannel", target_channel);

  // For the LTS pilot simply check whether the device policy is set and ignore
  // its value.
  std::string value;
  bool is_lts =
      ash::CrosSettings::Get()->GetString(ash::kReleaseLtsTag, &value);
  channel_info.Set("isLts", is_lts);

  ResolveJavascriptCallback(base::Value(callback_id), channel_info);
}

void AboutHandler::HandleApplyDeferredUpdate(const base::Value::List& args) {
  version_updater_->ApplyDeferredUpdate();
}

void AboutHandler::HandleRequestUpdate(const base::Value::List& args) {
  RequestUpdate();
}

void AboutHandler::HandleRequestUpdateOverCellular(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());

  const std::string& update_version = args[0].GetString();
  const std::string& update_size_string = args[1].GetString();
  int64_t update_size;
  CHECK(base::StringToInt64(update_size_string, &update_size));

  RequestUpdateOverCellular(update_version, update_size);
}

void AboutHandler::RequestUpdateOverCellular(const std::string& update_version,
                                             int64_t update_size) {
  version_updater_->SetUpdateOverCellularOneTimePermission(
      base::BindRepeating(&AboutHandler::SetUpdateStatus,
                          weak_factory_.GetWeakPtr()),
      update_version, update_size);
}

void AboutHandler::HandleRefreshTPMFirmwareUpdateStatus(
    const base::Value::List& args) {
  ash::tpm_firmware_update::GetAvailableUpdateModes(
      base::BindOnce(&AboutHandler::RefreshTPMFirmwareUpdateStatus,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta());
}

void AboutHandler::RefreshTPMFirmwareUpdateStatus(
    const std::set<ash::tpm_firmware_update::Mode>& modes) {
  base::Value::Dict event;
  event.Set("updateAvailable", !modes.empty());
  FireWebUIListener("tpm-firmware-update-status-changed", event);
}

void AboutHandler::HandleGetEndOfLifeInfo(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();
  version_updater_->GetEolInfo(base::BindOnce(&AboutHandler::OnGetEndOfLifeInfo,
                                              weak_factory_.GetWeakPtr(),
                                              callback_id));
}

void AboutHandler::OnGetEndOfLifeInfo(
    std::string callback_id,
    ash::UpdateEngineClient::EolInfo eol_info) {
  // Response values.
  bool eol_passed = false;
  std::u16string eol_message;
  bool show_eol_incentive = false;
  bool show_offer_text = false;
  bool extended_date_passed = false;
  bool extended_opt_in_required = false;

  if (!eol_info.extended_date.is_null()) {
    extended_date_passed = eol_info.extended_date <= clock_->Now();
    extended_opt_in_required = eol_info.extended_opt_in_required;
  }

  if (!eol_info.eol_date.is_null()) {
    eol_passed = eol_info.eol_date <= clock_->Now();
    if (extended_opt_in_required &&
        !ash::ExtendedUpdatesController::Get()->IsOptedIn()) {
      eol_message = GetEndOfLifeMessage(eol_info.extended_date);
    } else {
      eol_message = GetEndOfLifeMessage(eol_info.eol_date);
    }
    const EolIncentiveType eolIncentiveType =
        ash::eol_incentive_util::ShouldShowEolIncentive(
            profile_, eol_info.eol_date, clock_->Now());
    show_eol_incentive =
        (eolIncentiveType == EolIncentiveType::kEolPassedRecently ||
         eolIncentiveType == EolIncentiveType::kEolPassed) &&
        eol_passed &&
        base::FeatureList::IsEnabled(ash::features::kEolIncentiveSettings);
    show_offer_text =
        (ash::features::kEolIncentiveParam.Get() !=
             ash::features::EolIncentiveParam::kNoOffer &&
         eolIncentiveType == EolIncentiveType::kEolPassedRecently);
    eol_incentive_shows_offer_ = show_offer_text;
  }

  base::Value::Dict response;
  response.Set("hasEndOfLife", eol_passed);
  response.Set("aboutPageEndOfLifeMessage", eol_message);
  response.Set("shouldShowEndOfLifeIncentive", show_eol_incentive);
  response.Set("shouldShowOfferText", show_offer_text);
  response.Set("isExtendedUpdatesDatePassed", extended_date_passed);
  response.Set("isExtendedUpdatesOptInRequired", extended_opt_in_required);

  ResolveJavascriptCallback(base::Value(callback_id), response);
}

std::u16string AboutHandler::GetEndOfLifeMessage(base::Time eol_date) const {
  bool eol_passed = eol_date <= clock_->Now();
  int eol_string_id = eol_passed
                          ? IDS_SETTINGS_ABOUT_PAGE_END_OF_LIFE_MESSAGE_PAST
                          : IDS_SETTINGS_ABOUT_PAGE_END_OF_LIFE_MESSAGE_FUTURE;
  const char16_t* eol_url =
      eol_passed ? chrome::kEolNotificationURL : chrome::kAutoUpdatePolicyURL;
  return l10n_util::GetStringFUTF16(eol_string_id,
                                    base::TimeFormatMonthAndYearForTimeZone(
                                        eol_date, icu::TimeZone::getGMT()),
                                    eol_url);
}

void AboutHandler::HandleOpenEndOfLifeIncentive(const base::Value::List& args) {
  DCHECK(args.empty());
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(eol_incentive_shows_offer_
               ? chrome::kEolIncentiveNotificationOfferURL
               : chrome::kEolIncentiveNotificationNoOfferURL),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

void AboutHandler::HandleIsManagedAutoUpdateEnabled(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(
      base::Value(callback_id),
      base::Value(version_updater_->IsManagedAutoUpdateEnabled()));
}

void AboutHandler::HandleIsConsumerAutoUpdateEnabled(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();
  const std::string& feature = update_engine::kFeatureConsumerAutoUpdate;
  version_updater_->IsFeatureEnabled(
      feature,
      base::BindOnce(&AboutHandler::OnIsConsumerAutoUpdateEnabled,
                     weak_factory_.GetWeakPtr(), callback_id, feature));
}

void AboutHandler::OnIsConsumerAutoUpdateEnabled(std::string callback_id,
                                                 std::string feature,
                                                 std::optional<bool> enabled) {
  if (!enabled.has_value()) {
    LOG(ERROR) << "Failed to get feature value for " << feature
               << " defaulting to enabled";
    enabled = true;
  }
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(enabled.value()));
}

void AboutHandler::HandleSetConsumerAutoUpdate(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  if (!args[0].is_bool()) {
    LOG(ERROR) << "Can't parse SetConsumerAutoUpdate() args";
    return;
  }
  bool enable = args[0].GetBool();
  const std::string& feature = update_engine::kFeatureConsumerAutoUpdate;
  version_updater_->ToggleFeature(feature, enable);
}

void AboutHandler::HandleOpenProductLicenseOther(
    const base::Value::List& args) {
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(chrome::kChromeUICreditsURL),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kSwitchToTab);
}

void AboutHandler::HandleIsExtendedUpdatesOptInEligible(
    const base::Value::List& args) {
  CHECK_EQ(4U, args.size());
  ash::ExtendedUpdatesController::Params params{
      .eol_passed = args[1].GetBool(),
      .extended_date_passed = args[2].GetBool(),
      .opt_in_required = args[3].GetBool(),
  };
  bool eligible =
      ash::ExtendedUpdatesController::Get()->IsOptInEligible(profile_, params);
  ResolveJavascriptCallback(args[0], base::Value(eligible));
}

void AboutHandler::HandleOpenExtendedUpdatesDialog(
    const base::Value::List& args) {
  CHECK(args.empty());
  ash::ExtendedUpdatesController::
      RecordEntryPointEventForSettingsSetUpButtonClicked();
  ash::extended_updates::ExtendedUpdatesDialog::Show();
}

void AboutHandler::HandleRecordExtendedUpdatesShown(
    const base::Value::List& args) {
  CHECK(args.empty());
  ash::ExtendedUpdatesController::
      RecordEntryPointEventForSettingsSetUpButtonShown();
}

void AboutHandler::OnExtendedUpdatesSettingChanged() {
  FireWebUIListener("extended-updates-setting-changed");
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void AboutHandler::RequestUpdate() {
  version_updater_->CheckForUpdate(
      base::BindRepeating(&AboutHandler::SetUpdateStatus,
                          weak_factory_.GetWeakPtr()),
#if BUILDFLAG(IS_MAC)
      base::BindRepeating(&AboutHandler::SetPromotionState,
                          weak_factory_.GetWeakPtr()));
#else
      VersionUpdater::PromoteCallback());
#endif  // BUILDFLAG(IS_MAC)
}

void AboutHandler::SetUpdateStatus(VersionUpdater::Status status,
                                   int progress,
                                   bool rollback,
                                   bool powerwash,
                                   const std::string& version,
                                   int64_t size,
                                   const std::u16string& message) {
  // Only UPDATING state should have progress set.
  DCHECK(status == VersionUpdater::UPDATING || progress == 0);

  base::Value::Dict event;
  event.Set("status", UpdateStatusToString(status));
  event.Set("message", message);
  event.Set("progress", progress);
  event.Set("rollback", rollback);
  event.Set("powerwash", powerwash);
  event.Set("version", version);
  // `base::Value::Dict` does not support int64_t, so convert to string.
  event.Set("size", base::NumberToString(size));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::u16string types_msg;
  if (status == VersionUpdater::FAILED_OFFLINE ||
      status == VersionUpdater::FAILED_CONNECTION_TYPE_DISALLOWED) {
    types_msg = GetAllowedConnectionTypesMessage();
  }
  base::Value types_value;
  if (!types_msg.empty()) {
    types_value = base::Value(std::move(types_msg));
  }
  event.Set("connectionTypes", std::move(types_value));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  FireWebUIListener("update-status-changed", event);
}

#if BUILDFLAG(IS_MAC)
void AboutHandler::SetPromotionState(VersionUpdater::PromotionState state) {
  // Worth noting: PROMOTE_DISABLED indicates that promotion is possible,
  // there's just something else going on right now (e.g. checking for update).
  bool hidden = state == VersionUpdater::PROMOTE_HIDDEN;
  bool disabled = state == VersionUpdater::PROMOTE_HIDDEN ||
                  state == VersionUpdater::PROMOTE_DISABLED ||
                  state == VersionUpdater::PROMOTED;
  bool actionable = state == VersionUpdater::PROMOTE_DISABLED ||
                    state == VersionUpdater::PROMOTE_ENABLED;

  std::u16string text;
  if (actionable)
    text = l10n_util::GetStringUTF16(IDS_ABOUT_CHROME_AUTOUPDATE_ALL);
  else if (state == VersionUpdater::PROMOTED)
    text = l10n_util::GetStringUTF16(IDS_ABOUT_CHROME_AUTOUPDATE_ALL_IS_ON);

  base::Value::Dict promo_state;
  promo_state.Set("hidden", hidden);
  promo_state.Set("disabled", disabled);
  promo_state.Set("actionable", actionable);
  if (!text.empty())
    promo_state.Set("text", text);

  FireWebUIListener("promotion-state-changed", promo_state);
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AboutHandler::OnRegulatoryLabelDirFound(
    std::string callback_id,
    const base::FilePath& label_dir_path) {
  if (label_dir_path.empty()) {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadRegulatoryLabelText, label_dir_path),
      base::BindOnce(&AboutHandler::OnRegulatoryLabelTextRead,
                     weak_factory_.GetWeakPtr(), callback_id, label_dir_path));
}

void AboutHandler::OnRegulatoryLabelTextRead(
    std::string callback_id,
    const base::FilePath& label_dir_path,
    const std::string& text) {
  base::Value::Dict regulatory_info;
  // Remove unnecessary whitespace.
  regulatory_info.Set("text", base::CollapseWhitespaceASCII(text, true));

  std::string image_path =
      label_dir_path.AppendASCII(kRegulatoryLabelImageFilename).MaybeAsASCII();
  std::string url =
      base::StrCat({"chrome://", chrome::kChromeOSAssetHost, "/", image_path});
  regulatory_info.Set("url", url);

  ResolveJavascriptCallback(base::Value(callback_id), regulatory_info);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace settings
