// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/about/about_section.h"

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/message_formatter.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/ash/settings/pages/about/device_name_handler.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/management/management_ui.h"
#include "chrome/browser/ui/webui/settings/about_handler.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kAboutChromeOsSectionPath;
using ::chromeos::settings::mojom::kDetailedBuildInfoSubpagePath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetAboutSearchConcepts() {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ABOUT_CHROME_OS_DETAILED_BUILD,
       mojom::kDetailedBuildInfoSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kDetailedBuild
                        : mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kDetailedBuildInfo}},
      {IDS_SETTINGS_ABOUT_OS,
       mojom::kAboutChromeOsSectionPath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kAboutChromeOs}},
      {IDS_OS_SETTINGS_TAG_OS_VERSION,
       mojom::kAboutChromeOsSectionPath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kAboutChromeOs}},
      {IDS_OS_SETTINGS_TAG_ABOUT_CHROME_OS_CHANNEL,
       mojom::kDetailedBuildInfoSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kDetailedBuild
                        : mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kChangeChromeChannel}},
      {IDS_OS_SETTINGS_TAG_ABOUT_CHROME_OS_COPY_DETAILED_BUILD,
       mojom::kDetailedBuildInfoSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kDetailedBuild
                        : mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCopyDetailedBuildInfo}},
      {IDS_OS_SETTINGS_TAG_ABOUT_OS_UPDATE,
       mojom::kAboutChromeOsSectionPath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kCheckForUpdate
                        : mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCheckForOsUpdate}},
      {IDS_OS_SETTINGS_TAG_ABOUT_HELP,
       mojom::kAboutChromeOsSectionPath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kHelp
                        : mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kGetHelpWithChromeOs}},
      {IDS_OS_SETTINGS_TAG_ABOUT_RELEASE_NOTES,
       mojom::kAboutChromeOsSectionPath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kReleaseNotes
                        : mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSeeWhatsNew},
       {IDS_OS_SETTINGS_TAG_ABOUT_RELEASE_NOTES_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDiagnosticsAppSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ABOUT_DIAGNOSTICS,
       mojom::kAboutChromeOsSectionPath,
       ash::features::IsOsSettingsRevampWayfindingEnabled()
           ? mojom::SearchResultIcon::kDiagnostics
           : mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDiagnostics},
       {IDS_OS_SETTINGS_TAG_ABOUT_DIAGNOSTICS_ALT1,
        IDS_OS_SETTINGS_TAG_ABOUT_DIAGNOSTICS_ALT2,
        IDS_OS_SETTINGS_TAG_ABOUT_DIAGNOSTICS_ALT3,
        IDS_OS_SETTINGS_TAG_ABOUT_DIAGNOSTICS_ALT4, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetFirmwareUpdatesAppSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ABOUT_FIRMWARE_UPDATES,
       mojom::kAboutChromeOsSectionPath,
       ash::features::IsOsSettingsRevampWayfindingEnabled()
           ? mojom::SearchResultIcon::kFirmwareUpdates
           : mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kFirmwareUpdates}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDeviceNameSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ABOUT_DEVICE_NAME,
       mojom::kDetailedBuildInfoSubpagePath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kChangeDeviceName},
       {IDS_OS_SETTINGS_TAG_ABOUT_DEVICE_NAME_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const std::vector<SearchConcept>& GetAboutTermsOfServiceSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ABOUT_TERMS_OF_SERVICE,
       mojom::kAboutChromeOsSectionPath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTermsOfService}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAboutReportIssueSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ABOUT_REPORT_ISSUE,
       mojom::kAboutChromeOsSectionPath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kReportAnIssue},
       {IDS_OS_SETTINGS_TAG_ABOUT_REPORT_ISSUE_ALT1,
        IDS_SETTINGS_ABOUT_PAGE_SEND_FEEDBACK, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Returns the link to the safety info for the device (if it exists).
std::string GetSafetyInfoLink() {
  const std::vector<std::string> board =
      base::SplitString(base::SysInfo::GetLsbReleaseBoard(), "-",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (board[0] == "nocturne") {
    return chrome::kChromeUISafetyPixelSlateURL;
  }
  if (board[0] == "eve" || board[0] == "atlas") {
    return chrome::kChromeUISafetyPixelbookURL;
  }

  return std::string();
}

std::string GetDeviceManager() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  DCHECK(connector);
  return connector->GetEnterpriseDomainManager();
}

}  // namespace

AboutSection::AboutSection(Profile* profile,
                           SearchTagRegistry* search_tag_registry,
                           PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      pref_service_(pref_service),
      crostini_subsection_(
          ash::features::IsOsSettingsRevampWayfindingEnabled()
              ? std::make_optional<CrostiniSection>(profile,
                                                    search_tag_registry,
                                                    pref_service)
              : std::nullopt) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetAboutSearchConcepts());

  updater.AddSearchTags(GetDiagnosticsAppSearchConcepts());

  updater.AddSearchTags(GetFirmwareUpdatesAppSearchConcepts());

  if (base::FeatureList::IsEnabled(features::kEnableHostnameSetting)) {
    updater.AddSearchTags(GetDeviceNameSearchConcepts());
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  updater.AddSearchTags(GetAboutTermsOfServiceSearchConcepts());

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kUserFeedbackAllowed,
      base::BindRepeating(&AboutSection::UpdateReportIssueSearchTags,
                          base::Unretained(this)));
  UpdateReportIssueSearchTags();

  pref_change_registrar_.Add(prefs::kConsumerAutoUpdateToggle,
                             base::DoNothingAs<void()>());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

AboutSection::~AboutSection() = default;

void AboutSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  // Top level About page strings.
  webui::LocalizedString kLocalizedStrings[] = {
      {"aboutProductLogoAlt", IDS_SHORT_PRODUCT_LOGO_ALT_TEXT},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      {"aboutReportAnIssue", IDS_SETTINGS_ABOUT_PAGE_REPORT_AN_ISSUE},
      {"aboutSendFeedback", IDS_SETTINGS_ABOUT_PAGE_SEND_FEEDBACK},
      {"aboutSendFeedbackDescription",
       IDS_OS_SETTINGS_REVAMP_SEND_FEEDBACK_DESCRIPTION},
#endif
      {"aboutDiagnostics", IDS_SETTINGS_ABOUT_PAGE_DIAGNOSTICS},
      {"aboutDiagnosticseDescription",
       IDS_OS_SETTINGS_REVAMP_DIAGNOSTICS_DESCRIPTION},
      {"aboutFirmwareUpdates", IDS_SETTINGS_ABOUT_PAGE_FIRMWARE_UPDATES},
      {"aboutFirmwareUpToDateDescription",
       IDS_OS_SETTINGS_REVAMP_FIRMWARE_UP_TO_DATE_DESCRIPTION},
      {"aboutFirmwareUpdateAvailableDescription",
       IDS_OS_SETTINGS_REVAMP_FIRMWARE_UPDATE_AVAILABLE_DESCRIPTION},
      {"aboutRelaunch", IDS_SETTINGS_ABOUT_PAGE_RELAUNCH},
      {"aboutUpgradeCheckStarted", IDS_SETTINGS_ABOUT_UPGRADE_CHECK_STARTED},
      {"aboutUpgradeNotUpToDate", IDS_SETTINGS_UPGRADE_NOT_UP_TO_DATE},
      {"aboutUpgradeRelaunch", IDS_SETTINGS_UPGRADE_SUCCESSFUL_RELAUNCH},
      {"aboutUpgradeUpdating", IDS_SETTINGS_UPGRADE_UPDATING},
      {"aboutUpgradeUpdatingPercent", IDS_SETTINGS_UPGRADE_UPDATING_PERCENT},
      {"aboutGetHelpUsingChrome", IDS_SETTINGS_GET_HELP_USING_CHROME},
      {"aboutPageTitle", IDS_SETTINGS_ABOUT_PROGRAM},
      {"aboutProductTitle", IDS_PRODUCT_NAME},

      {"aboutEndOfLifeTitle", IDS_SETTINGS_ABOUT_PAGE_END_OF_LIFE_TITLE},
      {"aboutDeviceName", IDS_SETTINGS_ABOUT_PAGE_DEVICE_NAME},
      {"aboutRelaunchAndAutoUpdate",
       IDS_SETTINGS_ABOUT_PAGE_RELAUNCH_AND_AUTO_UPDATE},
      {"aboutRelaunchAndPowerwash",
       IDS_SETTINGS_ABOUT_PAGE_RELAUNCH_AND_POWERWASH},
      {"aboutRollbackInProgress", IDS_SETTINGS_UPGRADE_ROLLBACK_IN_PROGRESS},
      {"aboutRollbackSuccess", IDS_SETTINGS_UPGRADE_ROLLBACK_SUCCESS},
      {"aboutUpgradeUpdatingChannelSwitch",
       IDS_SETTINGS_UPGRADE_UPDATING_CHANNEL_SWITCH},
      {"aboutUpgradeSuccessChannelSwitch",
       IDS_SETTINGS_UPGRADE_SUCCESSFUL_CHANNEL_SWITCH},
      {"aboutTPMFirmwareUpdateTitle",
       IDS_SETTINGS_ABOUT_TPM_FIRMWARE_UPDATE_TITLE},
      {"aboutTPMFirmwareUpdateDescription",
       IDS_SETTINGS_ABOUT_TPM_FIRMWARE_UPDATE_DESCRIPTION},
      {"aboutDeferredUpdate",
       IDS_SETTINGS_ABOUT_PAGE_DEFERRED_UPDATE_DESCRIPTION},
      {"aboutExtendedUpdatesButton",
       IDS_SETTINGS_ABOUT_PAGE_EXTENDED_UPDATES_BUTTON},

      // About page, channel switcher dialog.
      {"aboutChangeChannel", IDS_SETTINGS_ABOUT_PAGE_CHANGE_CHANNEL},
      {"aboutChangeChannelAndPowerwash",
       IDS_SETTINGS_ABOUT_PAGE_CHANGE_CHANNEL_AND_POWERWASH},
      {"aboutDelayedWarningMessage",
       IDS_SETTINGS_ABOUT_PAGE_DELAYED_WARNING_MESSAGE},
      {"aboutDelayedWarningTitle",
       IDS_SETTINGS_ABOUT_PAGE_DELAYED_WARNING_TITLE},
      {"aboutPowerwashWarningMessage",
       IDS_SETTINGS_ABOUT_PAGE_POWERWASH_WARNING_MESSAGE},
      {"aboutPowerwashWarningTitle",
       IDS_SETTINGS_ABOUT_PAGE_POWERWASH_WARNING_TITLE},
      {"aboutUnstableWarningMessage",
       IDS_SETTINGS_ABOUT_PAGE_UNSTABLE_WARNING_MESSAGE},
      {"aboutUnstableWarningTitle",
       IDS_SETTINGS_ABOUT_PAGE_UNSTABLE_WARNING_TITLE},
      {"aboutChannelDialogBeta", IDS_SETTINGS_ABOUT_PAGE_DIALOG_CHANNEL_BETA},
      {"aboutChannelDialogDev", IDS_SETTINGS_ABOUT_PAGE_DIALOG_CHANNEL_DEV},
      {"aboutChannelDialogStable",
       IDS_SETTINGS_ABOUT_PAGE_DIALOG_CHANNEL_STABLE},

      // About page, edit device name dialog.
      {"aboutEditDeviceName", IDS_SETTINGS_ABOUT_PAGE_EDIT_DEVICE_NAME},
      {"aboutDeviceNameInfo", IDS_SETTINGS_ABOUT_PAGE_DEVICE_NAME_INFO},
      {"aboutDeviceNameConstraints",
       IDS_SETTINGS_ABOUT_PAGE_DEVICE_NAME_CONSTRAINTS},
      {"aboutDeviceNameConstraintsA11yDescription",
       IDS_SETTINGS_ABOUT_PAGE_DEVICE_NAME_CONSTRAINTS_A11Y_DESCRIPTION},
      {"aboutDeviceNameInputCharacterCount",
       IDS_SETTINGS_ABOUT_PAGE_DEVICE_NAME_INPUT_COUNT},
      {"aboutDeviceNameInputA11yLabel",
       IDS_SETTINGS_ABOUT_PAGE_DEVICE_NAME_INPUT_A11Y_LABEL},
      {"aboutDeviceNameDoneBtnA11yLabel",
       IDS_SETTINGS_ABOUT_PAGE_DEVICE_NAME_DONE_BTN_A11Y_LABEL},
      {"aboutDeviceNameEditBtnA11yLabel",
       IDS_SETTINGS_ABOUT_PAGE_DEVICE_NAME_EDIT_BTN_A11Y_LABEL},
      {"aboutDeviceNameEditBtnA11yDescription",
       IDS_SETTINGS_ABOUT_PAGE_DEVICE_NAME_EDIT_BTN_A11Y_DESCRIPTION},

      // About page, update warning dialog.
      {"aboutUpdateWarningMessage",
       IDS_SETTINGS_ABOUT_PAGE_UPDATE_WARNING_MESSAGE},
      {"aboutUpdateWarningTitle", IDS_SETTINGS_ABOUT_PAGE_UPDATE_WARNING_TITLE},

      // Detailed build information
      {"aboutBuildDetailsTitle", IDS_OS_SETTINGS_ABOUT_PAGE_BUILD_DETAILS},
      {"aboutChannelBeta", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_BETA},
      {"aboutChannelCanary", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_CANARY},
      {"aboutChannelDev", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_DEV},
      {"aboutChannelLabel", IDS_SETTINGS_ABOUT_PAGE_CHANNEL},
      {"aboutChannelStable", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_STABLE},
      {"aboutChannelLongTermSupportCandidate",
       IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_LTC},
      {"aboutChannelLongTermSupport",
       IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_LTS},
      {"aboutCheckForUpdates", IDS_SETTINGS_ABOUT_PAGE_CHECK_FOR_UPDATES},
      {"aboutCurrentlyOnChannelInfo",
       IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_INFO},
      {"aboutDetailedBuildInfo", IDS_SETTINGS_ABOUT_PAGE_DETAILED_BUILD_INFO},
      {version_ui::kApplicationLabel, IDS_PRODUCT_NAME},
      {version_ui::kPlatform, IDS_PLATFORM_LABEL},
      {version_ui::kFirmwareVersion, IDS_VERSION_UI_FIRMWARE_VERSION},
      {version_ui::kARC, IDS_ARC_LABEL},
      {"aboutBuildDetailsCopyTooltipLabel",
       IDS_OS_SETTINGS_ABOUT_PAGE_BUILD_DETAILS_COPY_TOOLTIP_LABEL},
      {"aboutBuildDetailsCopiedToClipboardA11yLabel",
       IDS_OS_SETTINGS_ABOUT_PAGE_BUILD_DETAILS_COPIED_TO_CLIPBOARD_A11Y_LABEL},
      {"aboutIsArcStatusTitle", IDS_OS_SETTINGS_ABOUT_ARC_STATUS_TITLE},
      {"aboutIsDeveloperModeTitle", IDS_OS_SETTINGS_ABOUT_DEVELOPER_MODE},
      {"isEnterpriseManagedTitle",
       IDS_OS_SETTINGS_ABOUT_PAGE_ENTERPRISE_ENNROLLED_TITLE},
      {"aboutOsPageTitle", IDS_SETTINGS_ABOUT_OS},
      {"aboutChromeOsMenuItemDescription",
       IDS_OS_SETTINGS_ABOUT_CHROMEOS_MENU_ITEM_DESCRIPTION},
      {"aboutGetHelpUsingChromeOs",
       kIsRevampEnabled ? IDS_OS_SETTINGS_REVAMP_GET_HELP_USING_CHROME_OS
                        : IDS_SETTINGS_GET_HELP_USING_CHROME_OS},
      {"aboutGetHelpDescription",
       IDS_OS_SETTINGS_REVAMP_GET_HELP_USING_CHROME_OS_DESCRIPTION},
      {"aboutOsProductTitle", IDS_PRODUCT_OS_NAME},
      {"aboutReleaseNotesOffline", IDS_SETTINGS_ABOUT_PAGE_RELEASE_NOTES},
      {"aboutShowReleaseNotes", IDS_SETTINGS_ABOUT_PAGE_SHOW_RELEASE_NOTES},
      {"aboutShowReleaseNotesDescription",
       IDS_OS_SETTINGS_REVAMP_ABOUT_PAGE_SHOW_RELEASE_NOTES_DESCRIPTION},
      {"aboutManagedEndOfLifeSubtitle",
       IDS_SETTINGS_ABOUT_PAGE_MANAGED_END_OF_LIFE_SUBTITLE},
      {"aboutUpgradeTryAgain", IDS_SETTINGS_UPGRADE_TRY_AGAIN},
      {"aboutUpgradeDownloadError", IDS_SETTINGS_UPGRADE_DOWNLOAD_ERROR},
      {"aboutUpgradeAdministrator", IDS_SETTINGS_UPGRADE_ADMINISTRATOR_ERROR},
      {"aboutUpdateToRollbackVersionDisallowed",
       IDS_SETTINGS_UPDATE_TO_ROLLBACK_VERSION_DISALLOWED},

      // About page auto update toggle.
      {"aboutConsumerAutoUpdateToggleTitle",
       IDS_SETTINGS_ABOUT_PAGE_CONSUMER_AUTO_UPDATE_TOGGLE_TITLE},
      {"aboutConsumerAutoUpdateToggleDescription",
       IDS_SETTINGS_ABOUT_PAGE_CONSUMER_AUTO_UPDATE_TOGGLE_DESCRIPTION},
      {"aboutConsumerAutoUpdateToggleDialogTitle",
       IDS_SETTINGS_ABOUT_PAGE_CONSUMER_AUTO_UPDATE_TOGGLE_DIALOG_TITLE},
      {"aboutConsumerAutoUpdateToggleDialogDescription",
       IDS_SETTINGS_ABOUT_PAGE_CONSUMER_AUTO_UPDATE_TOGGLE_DIALOG_DESCRIPTION},
      {"aboutConsumerAutoUpdateToggleTurnOffButton",
       IDS_SETTINGS_ABOUT_PAGE_CONSUMER_AUTO_UPDATE_TOGGLE_TURN_OFF_BUTTON},
      {"aboutConsumerAutoUpdateToggleKeepUpdatesButton",
       IDS_SETTINGS_ABOUT_PAGE_CONSUMER_AUTO_UPDATE_TOGGLE_KEEP_UPDATES_BUTTON},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("aboutTPMFirmwareUpdateLearnMoreURL",
                         chrome::kTPMFirmwareUpdateLearnMoreURL);
  html_source->AddString(
      "aboutUpgradeUpToDate",
      ui::SubstituteChromeOSDeviceType(IDS_SETTINGS_UPGRADE_UP_TO_DATE));
  html_source->AddString("managementPage",
                         ManagementUI::GetManagementPageSubtitle(profile()));

  html_source->AddString("deviceManager", GetDeviceManager());

  if (user_manager::UserManager::IsInitialized()) {
    bool is_enterprise_managed =
        policy::ManagementServiceFactory::GetForPlatform()->IsManaged();
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    bool is_current_owner = user_manager->IsCurrentUserOwner();

    if (!is_enterprise_managed && !is_current_owner) {
      html_source->AddString("ownerEmail",
                             user_manager->GetOwnerAccountId().GetUserEmail());
    }

    html_source->AddBoolean("isConsumerAutoUpdateTogglingAllowed",
                            is_current_owner);
    html_source->AddBoolean("showAutoUpdateToggle",
                            ShouldShowAUToggle(user_manager->GetActiveUser()));
  }

  html_source->AddString("aboutBrowserVersion",
                         VersionUI::GetAnnotatedVersionStringForUi());
  html_source->AddString(
      "aboutProductCopyright",
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COPYRIGHT),
          base::Time::Now()));

  html_source->AddString(
      "aboutProductLicenseChromium",
      l10n_util::GetStringFUTF16(IDS_VERSION_UI_LICENSE_CHROMIUM,
                                 chrome::kChromiumProjectURL));
  html_source->AddString(
      "aboutProductLicenseOther",
      l10n_util::GetStringUTF16(IDS_VERSION_UI_LICENSE_OTHER));

  std::u16string os_license = l10n_util::GetStringFUTF16(
      IDS_ABOUT_CROS_VERSION_LICENSE, chrome::kChromeUIOSCreditsURL16);
  html_source->AddString("aboutProductOsLicense", os_license);
  std::u16string os_with_linux_license = l10n_util::GetStringFUTF16(
      IDS_ABOUT_CROS_WITH_LINUX_VERSION_LICENSE,
      chrome::kChromeUIOSCreditsURL16, chrome::kChromeUICrostiniCreditsURL16);
  html_source->AddString("aboutProductOsWithLinuxLicense",
                         os_with_linux_license);
  html_source->AddBoolean(
      "aboutEnterpriseManaged",
      policy::ManagementServiceFactory::GetForPlatform()->IsManaged());
  html_source->AddBoolean("aboutIsArcEnabled",
                          arc::IsArcPlayStoreEnabledForProfile(profile()));
  html_source->AddBoolean("aboutIsDeveloperMode",
                          base::CommandLine::ForCurrentProcess()->HasSwitch(
                              chromeos::switches::kSystemDevMode));
  html_source->AddBoolean("isHostnameSettingEnabled",
                          features::IsHostnameSettingEnabled());

  html_source->AddString(
      "endOfLifeMessage",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_ABOUT_PAGE_LAST_UPDATE_MESSAGE,
                                 ui::GetChromeOSDeviceName(),
                                 chrome::kEolNotificationURL));

  html_source->AddString("eolIncentiveOfferTitle",
                         l10n_util::GetStringUTF16(
                             IDS_SETTINGS_END_OF_LIFE_INCENTIVE_OFFER_TITLE));
  html_source->AddString("eolIncentiveOfferMessage",
                         l10n_util::GetStringUTF16(
                             IDS_SETTINGS_END_OF_LIFE_INCENTIVE_OFFER_MESSAGE));
  html_source->AddString("eolIncentiveButtonOfferText",
                         l10n_util::GetStringUTF16(
                             IDS_SETTINGS_END_OF_LIFE_INCENTIVE_OFFER_BUTTON));
  html_source->AddString(
      "eolIncentiveNoOfferTitle",
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_END_OF_LIFE_INCENTIVE_NO_OFFER_TITLE));
  html_source->AddString(
      "eolIncentiveNoOfferMessage",
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_END_OF_LIFE_INCENTIVE_NO_OFFER_MESSAGE));

  html_source->AddString(
      "extendedUpdatesMainMessage",
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_ABOUT_PAGE_EXTENDED_UPDATES_MAIN_MESSAGE));
  html_source->AddString(
      "extendedUpdatesSecondaryMessage",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ABOUT_PAGE_EXTENDED_UPDATES_SECONDARY_MESSAGE,
          chrome::kDeviceExtendedUpdatesLearnMoreURL));

  std::string safetyInfoLink = GetSafetyInfoLink();
  html_source->AddBoolean("shouldShowSafetyInfo", !safetyInfoLink.empty());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  html_source->AddString("aboutTermsURL", chrome::kChromeUITermsURL);
  html_source->AddLocalizedString("aboutProductTos",
                                  IDS_ABOUT_TERMS_OF_SERVICE);
  html_source->AddString(
      "aboutProductSafety",
      l10n_util::GetStringUTF16(IDS_ABOUT_SAFETY_INFORMATION));
  html_source->AddString("aboutProductSafetyURL",
                         base::UTF8ToUTF16(safetyInfoLink));
#endif

  // Crostini subsection exists only when OsSettingsRevampWayfinding is enabled.
  if (crostini_subsection_) {
    crostini_subsection_->AddLoadTimeData(html_source);
  }
}

void AboutSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<::settings::AboutHandler>(profile()));
  if (features::IsHostnameSettingEnabled()) {
    web_ui->AddMessageHandler(std::make_unique<DeviceNameHandler>());
  }

  // Crostini subsection exists only when OsSettingsRevampWayfinding is enabled.
  if (crostini_subsection_) {
    crostini_subsection_->AddHandlers(web_ui);
  }
}

int AboutSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_ABOUT_OS;
}

mojom::Section AboutSection::GetSection() const {
  return mojom::Section::kAboutChromeOs;
}

mojom::SearchResultIcon AboutSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kChrome;
}

const char* AboutSection::GetSectionPath() const {
  return mojom::kAboutChromeOsSectionPath;
}

bool AboutSection::LogMetric(mojom::Setting setting, base::Value& value) const {
  // Unimplemented.
  return false;
}

void AboutSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  // Top-level About section
  generator->RegisterTopLevelSetting(mojom::Setting::kCheckForOsUpdate);
  generator->RegisterTopLevelSetting(mojom::Setting::kSeeWhatsNew);
  generator->RegisterTopLevelSetting(mojom::Setting::kGetHelpWithChromeOs);
  generator->RegisterTopLevelSetting(mojom::Setting::kReportAnIssue);
  generator->RegisterTopLevelSetting(mojom::Setting::kTermsOfService);
  generator->RegisterTopLevelSetting(mojom::Setting::kDiagnostics);
  generator->RegisterTopLevelSetting(mojom::Setting::kFirmwareUpdates);

  // Detailed build info.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_ABOUT_PAGE_DETAILED_BUILD_INFO,
      mojom::Subpage::kDetailedBuildInfo, mojom::SearchResultIcon::kChrome,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kDetailedBuildInfoSubpagePath);
  static constexpr mojom::Setting kDetailedBuildInfoSettings[] = {
      mojom::Setting::kChangeChromeChannel, mojom::Setting::kChangeDeviceName,
      mojom::Setting::kCopyDetailedBuildInfo};
  RegisterNestedSettingBulk(mojom::Subpage::kDetailedBuildInfo,
                            kDetailedBuildInfoSettings, generator);

  if (crostini_subsection_) {
    crostini_subsection_->RegisterHierarchy(generator);
  }
}

bool AboutSection::ShouldShowAUToggle(user_manager::User* active_user) {
  if (!active_user) {
    return false;
  }

  AccountId account_id = active_user->GetAccountId();
  if (account_id.GetAccountType() != AccountType::GOOGLE) {
    return false;
  }

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile());
  if (!identity_manager) {
    return false;
  }

  const AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByGaiaId(account_id.GetGaiaId());
  // If the user falls under New Deal..
  if (account_info.capabilities.can_toggle_auto_updates() ==
      signin::Tribool::kTrue) {
    // Show toggle based on user's capabilities.
    return features::IsConsumerAutoUpdateToggleAllowed();
  }

  return false;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void AboutSection::UpdateReportIssueSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (pref_service_->GetBoolean(prefs::kUserFeedbackAllowed)) {
    updater.AddSearchTags(GetAboutReportIssueSearchConcepts());
  } else {
    updater.RemoveSearchTags(GetAboutReportIssueSearchConcepts());
  }
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace ash::settings
