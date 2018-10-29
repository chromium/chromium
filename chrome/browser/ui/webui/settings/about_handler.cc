// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/about_handler.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/message_formatter.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/user_agent.h"
#include "ui/base/l10n/l10n_util.h"
#include "v8/include/v8-version-string.h"

#if defined(OS_CHROMEOS)
#include "base/i18n/time_formatting.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/tpm_firmware_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/image_source.h"
#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"
#include "chrome/browser/ui/webui/help/version_updater_chromeos.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/system/statistics_provider.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/devicetype_utils.h"
#endif

using base::ListValue;
using content::BrowserThread;

namespace {

#if defined(OS_CHROMEOS)

// Directory containing the regulatory labels for supported regions.
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

bool ShouldShowSafetyInfo() {
  const std::vector<std::string> board =
      base::SplitString(base::SysInfo::GetLsbReleaseBoard(), "-",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return board[0] == "nocturne";
}

// Returns message that informs user that for update it's better to
// connect to a network of one of the allowed types.
base::string16 GetAllowedConnectionTypesMessage() {
  const chromeos::NetworkState* network = chromeos::NetworkHandler::Get()
                                              ->network_state_handler()
                                              ->DefaultNetwork();
  const bool mobile_data =
      network && network->IsConnectedState() && network->IsUsingMobileData();

  if (help_utils_chromeos::IsUpdateOverCellularAllowed(
          true /* interactive */)) {
    return mobile_data
               ? l10n_util::GetStringUTF16(
                     IDS_UPGRADE_NETWORK_LIST_CELLULAR_ALLOWED_NOT_AUTOMATIC)
               : l10n_util::GetStringUTF16(
                     IDS_UPGRADE_NETWORK_LIST_CELLULAR_ALLOWED);
  } else {
    return l10n_util::GetStringUTF16(
        IDS_UPGRADE_NETWORK_LIST_CELLULAR_DISALLOWED);
  }
}

// Returns true if the device is enterprise managed, false otherwise.
bool IsEnterpriseManaged() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
}

// Returns true if current user can change channel, false otherwise.
bool CanChangeChannel(Profile* profile) {
  // On a managed machine we delegate this setting to the users of the same
  // domain only if the policy value is "domain".
  if (IsEnterpriseManaged()) {
    bool value = false;
    chromeos::CrosSettings::Get()->GetBoolean(
        chromeos::kReleaseChannelDelegated, &value);
    if (!value)
      return false;

    // Get the currently logged-in user and strip the domain part only.
    std::string domain = "";
    const user_manager::User* user =
        profile ? chromeos::ProfileHelper::Get()->GetUserByProfile(profile)
                : nullptr;
    std::string email =
        user ? user->GetAccountId().GetUserEmail() : std::string();
    size_t at_pos = email.find('@');
    if (at_pos != std::string::npos && at_pos + 1 < email.length())
      domain = email.substr(email.find('@') + 1);
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    return domain == connector->GetEnterpriseEnrollmentDomain();
  }

  // On non-managed machines, only the local owner can change the channel.
  chromeos::OwnerSettingsServiceChromeOS* service =
      chromeos::OwnerSettingsServiceChromeOSFactory::GetInstance()
          ->GetForBrowserContext(profile);
  return service && service->IsOwner();
}

// Returns the path of the regulatory labels directory for a given region, if
// found. Must be called from the blocking pool.
base::FilePath GetRegulatoryLabelDirForRegion(const std::string& region) {
  // Generate the path under the asset dir or URL host to the regulatory files
  // for the region, e.g., "regulatory_labels/us/".
  const base::FilePath region_path =
      base::FilePath(kRegulatoryLabelsDirectory).AppendASCII(region);

  // Check for file existence starting in /usr/share/chromeos-assets/, e.g.,
  // "/usr/share/chromeos-assets/regulatory_labels/us/label.png".
  const base::FilePath asset_dir(chrome::kChromeOSAssetPath);
  if (base::PathExists(asset_dir.Append(region_path)
                           .AppendASCII(kRegulatoryLabelImageFilename))) {
    return region_path;
  }

  return base::FilePath();
}

// Finds the directory for the regulatory label, using the VPD region code.
// Also tries "us" as a fallback region. Must be called from the blocking pool.
base::FilePath FindRegulatoryLabelDir() {
  std::string region;
  base::FilePath region_path;
  // Use the VPD region code to find the label dir.
  if (chromeos::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          "region", &region) &&
      !region.empty()) {
    region_path = GetRegulatoryLabelDirForRegion(region);
  }

  // Try the fallback region code if no directory was found.
  if (region_path.empty() && region != kDefaultRegionCode)
    region_path = GetRegulatoryLabelDirForRegion(kDefaultRegionCode);

  return region_path;
}

// Reads the file containing the regulatory label text, if found, relative to
// the asset directory. Must be called from the blocking pool.
std::string ReadRegulatoryLabelText(const base::FilePath& label_dir_path) {
  base::FilePath text_path(chrome::kChromeOSAssetPath);
  text_path = text_path.Append(label_dir_path);
  text_path = text_path.AppendASCII(kRegulatoryLabelTextFilename);

  std::string contents;
  if (base::ReadFileToString(text_path, &contents))
    return contents;
  return std::string();
}

std::unique_ptr<base::DictionaryValue> GetVersionInfo() {
  std::unique_ptr<base::DictionaryValue> version_info(
      new base::DictionaryValue);

  version_info->SetString("osVersion",
                          chromeos::version_loader::GetVersion(
                              chromeos::version_loader::VERSION_FULL));
  version_info->SetString("arcVersion",
                          chromeos::version_loader::GetARCVersion());
  version_info->SetString("osFirmware",
                          chromeos::version_loader::GetFirmware());

  return version_info;
}

#endif  // defined(OS_CHROMEOS)

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
    case VersionUpdater::DISABLED:
      status_str = "disabled";
      break;
    case VersionUpdater::DISABLED_BY_ADMIN:
      status_str = "disabled_by_admin";
      break;
    case VersionUpdater::NEED_PERMISSION_TO_UPDATE:
      status_str = "need_permission_to_update";
      break;
  }

  return status_str;
}

}  // namespace

namespace settings {

AboutHandler::AboutHandler()
    : apply_changes_from_upgrade_observer_(false), weak_factory_(this) {
  UpgradeDetector::GetInstance()->AddObserver(this);
}

AboutHandler::~AboutHandler() {
  UpgradeDetector::GetInstance()->RemoveObserver(this);
}

AboutHandler* AboutHandler::Create(content::WebUIDataSource* html_source,
                                   Profile* profile) {
  html_source->AddString(
      "aboutBrowserVersion",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ABOUT_PAGE_BROWSER_VERSION,
          base::UTF8ToUTF16(version_info::GetVersionNumber()),
          l10n_util::GetStringUTF16(version_info::IsOfficialBuild()
                                        ? IDS_VERSION_UI_OFFICIAL
                                        : IDS_VERSION_UI_UNOFFICIAL),
          base::UTF8ToUTF16(chrome::GetChannelName()),
          l10n_util::GetStringUTF16(sizeof(void*) == 8
                                        ? IDS_VERSION_UI_64BIT
                                        : IDS_VERSION_UI_32BIT)));

  html_source->AddString(
      "aboutProductCopyright",
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COPYRIGHT),
          base::Time::Now()));

  base::string16 license = l10n_util::GetStringFUTF16(
      IDS_VERSION_UI_LICENSE, base::ASCIIToUTF16(chrome::kChromiumProjectURL),
      base::ASCIIToUTF16(chrome::kChromeUICreditsURL));
  html_source->AddString("aboutProductLicense", license);

  html_source->AddBoolean("aboutObsoleteNowOrSoon",
                          ObsoleteSystem::IsObsoleteNowOrSoon());
  html_source->AddBoolean("aboutObsoleteEndOfTheLine",
                          ObsoleteSystem::IsObsoleteNowOrSoon() &&
                              ObsoleteSystem::IsEndOfTheLine());
  html_source->AddString("aboutObsoleteSystem",
                         ObsoleteSystem::LocalizedObsoleteString());
  html_source->AddString("aboutObsoleteSystemURL",
                         ObsoleteSystem::GetLinkURL());

#if defined(GOOGLE_CHROME_BUILD)
  base::string16 tos = l10n_util::GetStringFUTF16(
      IDS_ABOUT_TERMS_OF_SERVICE, base::UTF8ToUTF16(chrome::kChromeUITermsURL));
  html_source->AddString("aboutProductTos", tos);
#endif

#if defined(OS_CHROMEOS)
  html_source->AddBoolean("shouldShowSafetyInfo", ShouldShowSafetyInfo());
#if defined(GOOGLE_CHROME_BUILD)
  html_source->AddString(
      "aboutProductSafety",
      l10n_util::GetStringUTF16(IDS_ABOUT_SAFETY_INFORMATION));
  html_source->AddString("aboutProductSafetyURL",
                         base::UTF8ToUTF16(chrome::kChromeUISafetyURL));
#endif

  base::string16 os_license = l10n_util::GetStringFUTF16(
      IDS_ABOUT_CROS_VERSION_LICENSE,
      base::ASCIIToUTF16(chrome::kChromeUIOSCreditsURL));
  html_source->AddString("aboutProductOsLicense", os_license);
  base::string16 os_with_linux_license = l10n_util::GetStringFUTF16(
      IDS_ABOUT_CROS_WITH_LINUX_VERSION_LICENSE,
      base::ASCIIToUTF16(chrome::kChromeUIOSCreditsURL),
      base::ASCIIToUTF16(chrome::kChromeUILinuxCreditsURL));
  html_source->AddString("aboutProductOsWithLinuxLicense",
                         os_with_linux_license);
  html_source->AddBoolean("aboutEnterpriseManaged", IsEnterpriseManaged());

  base::Time build_time = base::SysInfo::GetLsbReleaseTime();
  base::string16 build_date = base::TimeFormatFriendlyDate(build_time);
  html_source->AddString("aboutBuildDate", build_date);

  base::CommandLine::StringType command_line =
      base::CommandLine::ForCurrentProcess()->GetCommandLineString();
  html_source->AddString("aboutCommandLine", command_line);

  html_source->AddString("aboutUserAgent", GetUserAgent());
  html_source->AddString("aboutJsEngineVersion", V8_VERSION_STRING);
  html_source->AddString("aboutBlinkVersion", content::GetWebKitVersion());
  html_source->AddString("endOfLifeMessage",
                         l10n_util::GetStringUTF16(IDS_EOL_NOTIFICATION_EOL));
  html_source->AddString("endOfLifeLearnMoreURL",
                         base::ASCIIToUTF16(chrome::kEolNotificationURL));
#endif

  return new AboutHandler();
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
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "setChannel", base::BindRepeating(&AboutHandler::HandleSetChannel,
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
      "refreshTPMFirmwareUpdateStatus",
      base::BindRepeating(&AboutHandler::HandleRefreshTPMFirmwareUpdateStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getHasEndOfLife",
      base::BindRepeating(&AboutHandler::HandleGetHasEndOfLife,
                          base::Unretained(this)));
#endif
#if defined(OS_MACOSX)
  web_ui()->RegisterMessageCallback(
      "promoteUpdater", base::BindRepeating(&AboutHandler::PromoteUpdater,
                                            base::Unretained(this)));
#endif

#if defined(OS_CHROMEOS)
  // Handler for the product label image, which will be shown if available.
  content::URLDataSource::Add(Profile::FromWebUI(web_ui()),
                              std::make_unique<chromeos::ImageSource>());
#endif
}

void AboutHandler::OnJavascriptAllowed() {
  apply_changes_from_upgrade_observer_ = true;
  version_updater_.reset(VersionUpdater::Create(web_ui()->GetWebContents()));
  policy_registrar_.reset(new policy::PolicyChangeRegistrar(
      g_browser_process->policy_service(),
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string())));
  policy_registrar_->Observe(
      policy::key::kDeviceAutoUpdateDisabled,
      base::Bind(&AboutHandler::OnDeviceAutoUpdatePolicyChanged,
                 base::Unretained(this)));
}

void AboutHandler::OnJavascriptDisallowed() {
  apply_changes_from_upgrade_observer_ = false;
  version_updater_.reset();
  policy_registrar_.reset();
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
  if (previous_policy)
    CHECK(previous_policy->GetAsBoolean(&previous_auto_update_disabled));

  bool current_auto_update_disabled = false;
  if (current_policy)
    CHECK(current_policy->GetAsBoolean(&current_auto_update_disabled));

  if (current_auto_update_disabled != previous_auto_update_disabled) {
    // Refresh the update status to refresh the status of the UI.
    RefreshUpdateStatus();
  }
}

void AboutHandler::HandlePageReady(const base::ListValue* args) {
  AllowJavascript();
}

void AboutHandler::HandleRefreshUpdateStatus(const base::ListValue* args) {
  RefreshUpdateStatus();
}

void AboutHandler::RefreshUpdateStatus() {
// On Chrome OS, do not check for an update automatically.
#if defined(OS_CHROMEOS)
  static_cast<VersionUpdaterCros*>(version_updater_.get())
      ->GetUpdateStatus(
          base::Bind(&AboutHandler::SetUpdateStatus, base::Unretained(this)));
#else
  RequestUpdate();
#endif
}

#if defined(OS_MACOSX)
void AboutHandler::PromoteUpdater(const base::ListValue* args) {
  version_updater_->PromoteUpdater();
}
#endif

void AboutHandler::HandleOpenFeedbackDialog(const base::ListValue* args) {
  DCHECK(args->empty());
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  chrome::OpenFeedbackDialog(browser,
                             chrome::kFeedbackSourceMdSettingsAboutPage);
}

void AboutHandler::HandleOpenHelpPage(const base::ListValue* args) {
  DCHECK(args->empty());
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  chrome::ShowHelp(browser, chrome::HELP_SOURCE_WEBUI);
}

#if defined(OS_CHROMEOS)

void AboutHandler::HandleSetChannel(const base::ListValue* args) {
  DCHECK(args->GetSize() == 2);

  if (!CanChangeChannel(Profile::FromWebUI(web_ui()))) {
    LOG(WARNING) << "Non-owner tried to change release track.";
    return;
  }

  base::string16 channel;
  bool is_powerwash_allowed;
  if (!args->GetString(0, &channel) ||
      !args->GetBoolean(1, &is_powerwash_allowed)) {
    LOG(ERROR) << "Can't parse SetChannel() args";
    return;
  }

  version_updater_->SetChannel(base::UTF16ToUTF8(channel),
                               is_powerwash_allowed);
  if (user_manager::UserManager::Get()->IsCurrentUserOwner()) {
    // Check for update after switching release channel.
    version_updater_->CheckForUpdate(
        base::Bind(&AboutHandler::SetUpdateStatus, base::Unretained(this)),
        VersionUpdater::PromoteCallback());
  }
}

void AboutHandler::HandleGetVersionInfo(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&GetVersionInfo),
      base::Bind(&AboutHandler::OnGetVersionInfoReady,
                 weak_factory_.GetWeakPtr(), callback_id));
}

void AboutHandler::OnGetVersionInfoReady(
    std::string callback_id,
    std::unique_ptr<base::DictionaryValue> version_info) {
  ResolveJavascriptCallback(base::Value(callback_id), *version_info);
}

void AboutHandler::HandleGetRegulatoryInfo(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&FindRegulatoryLabelDir),
      base::Bind(&AboutHandler::OnRegulatoryLabelDirFound,
                 weak_factory_.GetWeakPtr(), callback_id));
}

void AboutHandler::HandleGetChannelInfo(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  version_updater_->GetChannel(
      true /* get current channel */,
      base::Bind(&AboutHandler::OnGetCurrentChannel, weak_factory_.GetWeakPtr(),
                 callback_id));
}

void AboutHandler::OnGetCurrentChannel(std::string callback_id,
                                       const std::string& current_channel) {
  version_updater_->GetChannel(
      false /* get target channel */,
      base::Bind(&AboutHandler::OnGetTargetChannel, weak_factory_.GetWeakPtr(),
                 callback_id, current_channel));
}

void AboutHandler::OnGetTargetChannel(std::string callback_id,
                                      const std::string& current_channel,
                                      const std::string& target_channel) {
  std::unique_ptr<base::DictionaryValue> channel_info(
      new base::DictionaryValue);
  channel_info->SetString("currentChannel", current_channel);
  channel_info->SetString("targetChannel", target_channel);
  channel_info->SetBoolean("canChangeChannel",
                           CanChangeChannel(Profile::FromWebUI(web_ui())));

  ResolveJavascriptCallback(base::Value(callback_id), *channel_info);
}

void AboutHandler::HandleRequestUpdate(const base::ListValue* args) {
  RequestUpdate();
}

void AboutHandler::HandleRequestUpdateOverCellular(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());

  std::string update_version;
  std::string update_size_string;
  int64_t update_size;

  CHECK(args->GetString(0, &update_version));
  CHECK(args->GetString(1, &update_size_string));
  CHECK(base::StringToInt64(update_size_string, &update_size));

  RequestUpdateOverCellular(update_version, update_size);
}

void AboutHandler::RequestUpdateOverCellular(const std::string& update_version,
                                             int64_t update_size) {
  version_updater_->SetUpdateOverCellularOneTimePermission(
      base::Bind(&AboutHandler::SetUpdateStatus, base::Unretained(this)),
      update_version, update_size);
}

void AboutHandler::HandleRefreshTPMFirmwareUpdateStatus(
    const base::ListValue* args) {
  chromeos::tpm_firmware_update::GetAvailableUpdateModes(
      base::Bind(&AboutHandler::RefreshTPMFirmwareUpdateStatus,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta());
}

void AboutHandler::RefreshTPMFirmwareUpdateStatus(
    const std::set<chromeos::tpm_firmware_update::Mode>& modes) {
  std::unique_ptr<base::DictionaryValue> event(new base::DictionaryValue);
  event->SetBoolean("updateAvailable", !modes.empty());
  FireWebUIListener("tpm-firmware-update-status-changed", *event);
}

void AboutHandler::HandleGetHasEndOfLife(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  version_updater_->GetEolStatus(
      base::BindOnce(&AboutHandler::OnGetEndOfLifeStatus,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void AboutHandler::OnGetEndOfLifeStatus(std::string callback_id,
                                        update_engine::EndOfLifeStatus status) {
  // Check for EndOfLifeStatus::kEol only because
  // EndOfLifeStatus::kSecurityOnly state is no longer supported.
  ResolveJavascriptCallback(
      base::Value(callback_id),
      base::Value(status == update_engine::EndOfLifeStatus::kEol));
}

#endif  // defined(OS_CHROMEOS)

void AboutHandler::RequestUpdate() {
  version_updater_->CheckForUpdate(
      base::Bind(&AboutHandler::SetUpdateStatus, base::Unretained(this)),
#if defined(OS_MACOSX)
      base::Bind(&AboutHandler::SetPromotionState, base::Unretained(this)));
#else
      VersionUpdater::PromoteCallback());
#endif  // OS_MACOSX
}

void AboutHandler::SetUpdateStatus(VersionUpdater::Status status,
                                   int progress,
                                   bool rollback,
                                   const std::string& version,
                                   int64_t size,
                                   const base::string16& message) {
  // Only UPDATING state should have progress set.
  DCHECK(status == VersionUpdater::UPDATING || progress == 0);

  std::unique_ptr<base::DictionaryValue> event(new base::DictionaryValue);
  event->SetString("status", UpdateStatusToString(status));
  event->SetString("message", message);
  event->SetInteger("progress", progress);
  event->SetBoolean("rollback", rollback);
  event->SetString("version", version);
  // DictionaryValue does not support int64_t, so convert to string.
  event->SetString("size", base::Int64ToString(size));
#if defined(OS_CHROMEOS)
  if (status == VersionUpdater::FAILED_OFFLINE ||
      status == VersionUpdater::FAILED_CONNECTION_TYPE_DISALLOWED) {
    base::string16 types_msg = GetAllowedConnectionTypesMessage();
    if (!types_msg.empty())
      event->SetString("connectionTypes", types_msg);
    else
      event->Set("connectionTypes", std::make_unique<base::Value>());
  } else {
    event->Set("connectionTypes", std::make_unique<base::Value>());
  }
#endif  // defined(OS_CHROMEOS)

  FireWebUIListener("update-status-changed", *event);
}

#if defined(OS_MACOSX)
void AboutHandler::SetPromotionState(VersionUpdater::PromotionState state) {
  // Worth noting: PROMOTE_DISABLED indicates that promotion is possible,
  // there's just something else going on right now (e.g. checking for update).
  bool hidden = state == VersionUpdater::PROMOTE_HIDDEN;
  bool disabled = state == VersionUpdater::PROMOTE_HIDDEN ||
                  state == VersionUpdater::PROMOTE_DISABLED ||
                  state == VersionUpdater::PROMOTED;
  bool actionable = state == VersionUpdater::PROMOTE_DISABLED ||
                    state == VersionUpdater::PROMOTE_ENABLED;

  base::string16 text = base::string16();
  if (actionable)
    text = l10n_util::GetStringUTF16(IDS_ABOUT_CHROME_AUTOUPDATE_ALL);
  else if (state == VersionUpdater::PROMOTED)
    text = l10n_util::GetStringUTF16(IDS_ABOUT_CHROME_AUTOUPDATE_ALL_IS_ON);

  base::DictionaryValue promo_state;
  promo_state.SetBoolean("hidden", hidden);
  promo_state.SetBoolean("disabled", disabled);
  promo_state.SetBoolean("actionable", actionable);
  if (!text.empty())
    promo_state.SetString("text", text);

  FireWebUIListener("promotion-state-changed", promo_state);
}
#endif  // defined(OS_MACOSX)

#if defined(OS_CHROMEOS)
void AboutHandler::OnRegulatoryLabelDirFound(
    std::string callback_id,
    const base::FilePath& label_dir_path) {
  if (label_dir_path.empty()) {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&ReadRegulatoryLabelText, label_dir_path),
      base::Bind(&AboutHandler::OnRegulatoryLabelTextRead,
                 weak_factory_.GetWeakPtr(), callback_id, label_dir_path));
}

void AboutHandler::OnRegulatoryLabelTextRead(
    std::string callback_id,
    const base::FilePath& label_dir_path,
    const std::string& text) {
  std::unique_ptr<base::DictionaryValue> regulatory_info(
      new base::DictionaryValue);
  // Remove unnecessary whitespace.
  regulatory_info->SetString("text", base::CollapseWhitespaceASCII(text, true));

  std::string image_path =
      label_dir_path.AppendASCII(kRegulatoryLabelImageFilename).MaybeAsASCII();
  std::string url =
      std::string("chrome://") + chrome::kChromeOSAssetHost + "/" + image_path;
  regulatory_info->SetString("url", url);

  ResolveJavascriptCallback(base::Value(callback_id), *regulatory_info);
}
#endif  // defined(OS_CHROMEOS)

}  // namespace settings
