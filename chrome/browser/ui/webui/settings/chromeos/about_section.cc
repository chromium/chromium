// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/about_section.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/message_formatter.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/browser/ui/webui/management_ui.h"
#include "chrome/browser/ui/webui/settings/about_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/version_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetAboutSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ABOUT_CHROME_OS_DETAILED_BUILD,
       mojom::kDetailedBuildInfoSubpagePath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kDetailedBuildInfo}},
      {IDS_SETTINGS_ABOUT_OS,
       mojom::kAboutChromeOsDetailsSubpagePath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAboutChromeOsDetails}},
      {IDS_OS_SETTINGS_TAG_OS_VERSION,
       mojom::kAboutChromeOsDetailsSubpagePath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAboutChromeOsDetails}},
      {IDS_OS_SETTINGS_TAG_ABOUT_CHROME_OS_CHANNEL,
       mojom::kDetailedBuildInfoSubpagePath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kChangeChromeChannel}},
      {IDS_OS_SETTINGS_TAG_ABOUT_CHROME_OS_COPY_DETAILED_BUILD,
       mojom::kDetailedBuildInfoSubpagePath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCopyDetailedBuildInfo}},
      {IDS_OS_SETTINGS_TAG_ABOUT_OS_UPDATE,
       mojom::kAboutChromeOsDetailsSubpagePath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCheckForOsUpdate}},
      {IDS_OS_SETTINGS_TAG_ABOUT_HELP,
       mojom::kAboutChromeOsDetailsSubpagePath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kGetHelpWithChromeOs}},
      {IDS_OS_SETTINGS_TAG_ABOUT_RELEASE_NOTES,
       mojom::kAboutChromeOsDetailsSubpagePath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSeeWhatsNew},
       {IDS_OS_SETTINGS_TAG_ABOUT_RELEASE_NOTES_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const std::vector<SearchConcept>& GetAboutTermsOfServiceSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ABOUT_TERMS_OF_SERVICE,
       mojom::kAboutChromeOsDetailsSubpagePath,
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
       mojom::kAboutChromeOsDetailsSubpagePath,
       mojom::SearchResultIcon::kChrome,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kReportAnIssue},
       {IDS_OS_SETTINGS_TAG_ABOUT_REPORT_ISSUE_ALT1,
        SearchConcept::kAltTagEnd}},
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

}  // namespace

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
AboutSection::AboutSection(Profile* profile,
                           SearchTagRegistry* search_tag_registry,
                           PrefService* pref_service)
    : AboutSection(profile, search_tag_registry) {
  pref_service_ = pref_service;

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetAboutTermsOfServiceSearchConcepts());

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kUserFeedbackAllowed,
      base::BindRepeating(&AboutSection::UpdateReportIssueSearchTags,
                          base::Unretained(this)));
  UpdateReportIssueSearchTags();
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

AboutSection::AboutSection(Profile* profile,
                           SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetAboutSearchConcepts());
}

AboutSection::~AboutSection() = default;

void AboutSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  // Top level About page strings.
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"aboutProductLogoAlt", IDS_SHORT_PRODUCT_LOGO_ALT_TEXT},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"aboutReportAnIssue", IDS_SETTINGS_ABOUT_PAGE_REPORT_AN_ISSUE},
#endif
    {"aboutRelaunch", IDS_SETTINGS_ABOUT_PAGE_RELAUNCH},
    {"aboutUpgradeCheckStarted", IDS_SETTINGS_ABOUT_UPGRADE_CHECK_STARTED},
    {"aboutUpgradeRelaunch", IDS_SETTINGS_UPGRADE_SUCCESSFUL_RELAUNCH},
    {"aboutUpgradeUpdating", IDS_SETTINGS_UPGRADE_UPDATING},
    {"aboutUpgradeUpdatingPercent", IDS_SETTINGS_UPGRADE_UPDATING_PERCENT},
    {"aboutGetHelpUsingChrome", IDS_SETTINGS_GET_HELP_USING_CHROME},
    {"aboutPageTitle", IDS_SETTINGS_ABOUT_PROGRAM},
    {"aboutProductTitle", IDS_PRODUCT_NAME},

    {"aboutEndOfLifeTitle", IDS_SETTINGS_ABOUT_PAGE_END_OF_LIFE_TITLE},
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

    // About page, channel switcher dialog.
    {"aboutChangeChannel", IDS_SETTINGS_ABOUT_PAGE_CHANGE_CHANNEL},
    {"aboutChangeChannelAndPowerwash",
     IDS_SETTINGS_ABOUT_PAGE_CHANGE_CHANNEL_AND_POWERWASH},
    {"aboutDelayedWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_DELAYED_WARNING_MESSAGE},
    {"aboutDelayedWarningTitle", IDS_SETTINGS_ABOUT_PAGE_DELAYED_WARNING_TITLE},
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
    {"aboutChannelDialogStable", IDS_SETTINGS_ABOUT_PAGE_DIALOG_CHANNEL_STABLE},

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
    {"aboutChannelLongTermStable",
     IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_STABLE_TT},
    {"aboutCheckForUpdates", IDS_SETTINGS_ABOUT_PAGE_CHECK_FOR_UPDATES},
    {"aboutCurrentlyOnChannel", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL},
    {"aboutDetailedBuildInfo", IDS_SETTINGS_ABOUT_PAGE_DETAILED_BUILD_INFO},
    {version_ui::kApplicationLabel, IDS_PRODUCT_NAME},
    {version_ui::kPlatform, IDS_PLATFORM_LABEL},
    {version_ui::kFirmwareVersion, IDS_VERSION_UI_FIRMWARE_VERSION},
    {version_ui::kARC, IDS_ARC_LABEL},
    {"aboutBuildDetailsCopyTooltipLabel",
     IDS_OS_SETTINGS_ABOUT_PAGE_BUILD_DETAILS_COPY_TOOLTIP_LABEL},
    {"aboutIsArcStatusTitle", IDS_OS_SETTINGS_ABOUT_ARC_STATUS_TITLE},
    {"aboutIsDeveloperModeTitle", IDS_OS_SETTINGS_ABOUT_DEVELOPER_MODE},
    {"isEnterpriseManagedTitle",
     IDS_OS_SETTINGS_ABOUT_PAGE_ENTERPRISE_ENNROLLED_TITLE},
    {"aboutOsPageTitle", IDS_SETTINGS_ABOUT_OS},
    {"aboutGetHelpUsingChromeOs", IDS_SETTINGS_GET_HELP_USING_CHROME_OS},
    {"aboutOsProductTitle", IDS_PRODUCT_OS_NAME},
    {"aboutReleaseNotesOffline", IDS_SETTINGS_ABOUT_PAGE_RELEASE_NOTES},
    {"aboutShowReleaseNotes", IDS_SETTINGS_ABOUT_PAGE_SHOW_RELEASE_NOTES},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString("aboutTPMFirmwareUpdateLearnMoreURL",
                         chrome::kTPMFirmwareUpdateLearnMoreURL);
  html_source->AddString(
      "aboutUpgradeUpToDate",
      ui::SubstituteChromeOSDeviceType(IDS_SETTINGS_UPGRADE_UP_TO_DATE));
  html_source->AddString("managementPage",
                         ManagementUI::GetManagementPageSubtitle(profile()));

  if (user_manager::UserManager::IsInitialized()) {
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    if (!webui::IsEnterpriseManaged() && !user_manager->IsCurrentUserOwner()) {
      html_source->AddString("ownerEmail",
                             user_manager->GetOwnerAccountId().GetUserEmail());
    }
  }

  html_source->AddString(
      "aboutBrowserVersion",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ABOUT_PAGE_BROWSER_VERSION,
          base::UTF8ToUTF16(version_info::GetVersionNumber()),
          l10n_util::GetStringUTF16(version_info::IsOfficialBuild()
                                        ? IDS_VERSION_UI_OFFICIAL
                                        : IDS_VERSION_UI_UNOFFICIAL),
          base::UTF8ToUTF16(chrome::GetChannelName()),
          l10n_util::GetStringUTF16(VersionUI::VersionProcessorVariation())));
  html_source->AddString(
      "aboutProductCopyright",
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COPYRIGHT),
          base::Time::Now()));

  base::string16 license = l10n_util::GetStringFUTF16(
      IDS_VERSION_UI_LICENSE, base::ASCIIToUTF16(chrome::kChromiumProjectURL),
      base::ASCIIToUTF16(chrome::kChromeUICreditsURL));
  html_source->AddString("aboutProductLicense", license);

  base::string16 os_license = l10n_util::GetStringFUTF16(
      IDS_ABOUT_CROS_VERSION_LICENSE,
      base::ASCIIToUTF16(chrome::kChromeUIOSCreditsURL));
  html_source->AddString("aboutProductOsLicense", os_license);
  base::string16 os_with_linux_license = l10n_util::GetStringFUTF16(
      IDS_ABOUT_CROS_WITH_LINUX_VERSION_LICENSE,
      base::ASCIIToUTF16(chrome::kChromeUIOSCreditsURL),
      base::ASCIIToUTF16(chrome::kChromeUICrostiniCreditsURL));
  html_source->AddString("aboutProductOsWithLinuxLicense",
                         os_with_linux_license);
  html_source->AddBoolean("aboutEnterpriseManaged",
                          webui::IsEnterpriseManaged());
  html_source->AddBoolean("aboutIsArcEnabled",
                          arc::IsArcPlayStoreEnabledForProfile(profile()));
  html_source->AddBoolean("aboutIsDeveloperMode",
                          base::CommandLine::ForCurrentProcess()->HasSwitch(
                              chromeos::switches::kSystemDevMode));

  html_source->AddString("endOfLifeMessage",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_ABOUT_PAGE_LAST_UPDATE_MESSAGE,
                             ui::GetChromeOSDeviceName(),
                             base::ASCIIToUTF16(chrome::kEolNotificationURL)));

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
}

void AboutSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<::settings::AboutHandler>(profile()));
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

std::string AboutSection::GetSectionPath() const {
  return mojom::kAboutChromeOsSectionPath;
}

bool AboutSection::LogMetric(mojom::Setting setting, base::Value& value) const {
  // Unimplemented.
  return false;
}

void AboutSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  // About Chrome OS.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_ABOUT_OS, mojom::Subpage::kAboutChromeOsDetails,
      mojom::SearchResultIcon::kChrome, mojom::SearchResultDefaultRank::kMedium,
      mojom::kAboutChromeOsDetailsSubpagePath);
  static constexpr mojom::Setting kAboutChromeOsDetailsSettings[] = {
      mojom::Setting::kCheckForOsUpdate, mojom::Setting::kSeeWhatsNew,
      mojom::Setting::kGetHelpWithChromeOs, mojom::Setting::kReportAnIssue,
      mojom::Setting::kTermsOfService};
  RegisterNestedSettingBulk(mojom::Subpage::kAboutChromeOsDetails,
                            kAboutChromeOsDetailsSettings, generator);

  // Detailed build info.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_ABOUT_PAGE_DETAILED_BUILD_INFO,
      mojom::Subpage::kDetailedBuildInfo, mojom::Subpage::kAboutChromeOsDetails,
      mojom::SearchResultIcon::kChrome, mojom::SearchResultDefaultRank::kMedium,
      mojom::kDetailedBuildInfoSubpagePath);
  static constexpr mojom::Setting kDetailedBuildInfoSettings[] = {
      mojom::Setting::kChangeChromeChannel,
      mojom::Setting::kCopyDetailedBuildInfo};
  RegisterNestedSettingBulk(mojom::Subpage::kDetailedBuildInfo,
                            kDetailedBuildInfoSettings, generator);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void AboutSection::UpdateReportIssueSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (pref_service_->GetBoolean(prefs::kUserFeedbackAllowed))
    updater.AddSearchTags(GetAboutReportIssueSearchConcepts());
  else
    updater.RemoveSearchTags(GetAboutReportIssueSearchConcepts());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace settings
}  // namespace chromeos
