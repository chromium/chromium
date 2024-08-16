// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/crostini/crostini_section.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crostini/crostini_disk.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/pages/crostini/crostini_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/crostini/guest_os_handler.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kAboutChromeOsSectionPath;
using ::chromeos::settings::mojom::kBruschettaDetailsSubpagePath;
using ::chromeos::settings::mojom::kBruschettaManageSharedFoldersSubpagePath;
using ::chromeos::settings::mojom::kBruschettaUsbPreferencesSubpagePath;
using ::chromeos::settings::mojom::kCrostiniBackupAndRestoreSubpagePath;
using ::chromeos::settings::mojom::kCrostiniDetailsSubpagePath;
using ::chromeos::settings::mojom::kCrostiniDevelopAndroidAppsSubpagePath;
using ::chromeos::settings::mojom::kCrostiniExtraContainersSubpagePath;
using ::chromeos::settings::mojom::kCrostiniManageSharedFoldersSubpagePath;
using ::chromeos::settings::mojom::kCrostiniPortForwardingSubpagePath;
using ::chromeos::settings::mojom::kCrostiniSectionPath;
using ::chromeos::settings::mojom::kCrostiniUsbPreferencesSubpagePath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetCrostiniOptedInSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI,
       mojom::kCrostiniDetailsSubpagePath,
       mojom::SearchResultIcon::kDeveloperTags,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCrostiniDetails},
       {IDS_OS_SETTINGS_TAG_CROSTINI_ALT1, IDS_OS_SETTINGS_TAG_CROSTINI_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CROSTINI_USB_PREFERENCES,
       mojom::kCrostiniUsbPreferencesSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCrostiniUsbPreferences},
       {IDS_OS_SETTINGS_TAG_CROSTINI_USB_PREFERENCES_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CROSTINI_REMOVE,
       mojom::kCrostiniDetailsSubpagePath,
       mojom::SearchResultIcon::kDeveloperTags,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kUninstallCrostini},
       {
           IDS_OS_SETTINGS_TAG_CROSTINI_REMOVE_ALT1,
           IDS_OS_SETTINGS_TAG_CROSTINI_REMOVE_ALT2,
           IDS_OS_SETTINGS_TAG_CROSTINI_REMOVE_ALT3,
           IDS_OS_SETTINGS_TAG_CROSTINI_REMOVE_ALT4,
           IDS_OS_SETTINGS_TAG_CROSTINI_REMOVE_ALT5,
       }},
      {IDS_OS_SETTINGS_TAG_CROSTINI_SHARED_FOLDERS,
       mojom::kCrostiniManageSharedFoldersSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCrostiniManageSharedFolders},
       {IDS_OS_SETTINGS_TAG_CROSTINI_SHARED_FOLDERS_ALT1,
        IDS_OS_SETTINGS_TAG_CROSTINI_SHARED_FOLDERS_ALT2,
        IDS_OS_SETTINGS_TAG_CROSTINI_SHARED_FOLDERS_ALT3,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CROSTINI_MIC_ACCESS,
       mojom::kCrostiniDetailsSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCrostiniMicAccess},
       {IDS_OS_SETTINGS_TAG_CROSTINI_MIC_ACCESS_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCrostiniOptedOutSearchConcepts(
    mojom::Section section,
    const char* section_path) {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI,
       section_path,
       mojom::SearchResultIcon::kDeveloperTags,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = section},
       {IDS_OS_SETTINGS_TAG_CROSTINI_ALT1, IDS_OS_SETTINGS_TAG_CROSTINI_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CROSTINI_SETUP,
       section_path,
       mojom::SearchResultIcon::kDeveloperTags,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSetUpCrostini},
       {IDS_OS_SETTINGS_TAG_CROSTINI_SETUP_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCrostiniExportImportSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI_BACKUP_RESTORE,
       mojom::kCrostiniBackupAndRestoreSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCrostiniBackupAndRestore},
       {IDS_OS_SETTINGS_TAG_CROSTINI_BACKUP_RESTORE_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCrostiniAdbSideloadingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI_ANDROID_APPS_ADB,
       mojom::kCrostiniDevelopAndroidAppsSubpagePath,
       mojom::SearchResultIcon::kDeveloperTags,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCrostiniAdbDebugging},
       {IDS_OS_SETTINGS_TAG_CROSTINI_ANDROID_APPS_ADB_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CROSTINI_ANDROID_APPS,
       mojom::kCrostiniDevelopAndroidAppsSubpagePath,
       mojom::SearchResultIcon::kDeveloperTags,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCrostiniDevelopAndroidApps},
       {IDS_OS_SETTINGS_TAG_CROSTINI_ANDROID_APPS_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCrostiniPortForwardingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI_PORT_FORWARDING,
       mojom::kCrostiniPortForwardingSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCrostiniPortForwarding},
       {IDS_OS_SETTINGS_TAG_CROSTINI_PORT_FORWARDING_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCrostiniContainerUpgradeSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI_CONTAINER_UPGRADE,
       mojom::kCrostiniDetailsSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCrostiniContainerUpgrade},
       {IDS_OS_SETTINGS_TAG_CROSTINI_CONTAINER_UPGRADE_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCrostiniDiskResizingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI_DISK_RESIZE,
       mojom::kCrostiniDetailsSubpagePath,
       mojom::SearchResultIcon::kPenguin,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.setting = mojom::Setting::kCrostiniDiskResize},
       {IDS_OS_SETTINGS_TAG_CROSTINI_DISK_RESIZE_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

bool IsProfileManaged(Profile* profile) {
  return profile->GetProfilePolicyConnector()->IsManaged();
}

bool IsDeviceManaged() {
  return policy::ManagementServiceFactory::GetForPlatform()->IsManaged();
}

bool IsAdbSideloadingAllowed() {
  return base::FeatureList::IsEnabled(features::kArcAdbSideloadingFeature);
}

}  // namespace

CrostiniSection::CrostiniSection(Profile* profile,
                                 SearchTagRegistry* search_tag_registry,
                                 PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      pref_service_(pref_service),
      profile_(profile) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      crostini::prefs::kUserCrostiniAllowedByPolicy,
      base::BindRepeating(&CrostiniSection::UpdateSearchTags,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      crostini::prefs::kCrostiniEnabled,
      base::BindRepeating(&CrostiniSection::UpdateSearchTags,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy,
      base::BindRepeating(&CrostiniSection::UpdateSearchTags,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      arc::prefs::kArcEnabled,
      base::BindRepeating(&CrostiniSection::UpdateSearchTags,
                          base::Unretained(this)));
  UpdateSearchTags();
}

CrostiniSection::~CrostiniSection() = default;

bool CrostiniSection::ShouldShowBruschetta(Profile* profile) {
  const bool bru_installable =
      !bruschetta::GetInstallableConfigs(profile).empty();
  const bool bru_installed =
      !guest_os::GetContainers(profile, guest_os::VmType::BRUSCHETTA).empty();
  return bru_installable || bru_installed;
}

void CrostiniSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  webui::LocalizedString kLocalizedStrings[] = {
      {"bruschettaPageLabel", IDS_SETTINGS_BRUSCHETTA_LABEL},
      {"bruschettaEnable", IDS_SETTINGS_TURN_ON},
      {"bruschettaMicDialogShutdownButton",
       IDS_SETTINGS_BRUSCHETTA_MIC_DIALOG_SHUTDOWN_BUTTON},
      {"bruschettaRemoveButton", IDS_SETTINGS_BRUSCHETTA_REMOVE_BUTTON},
      {"crostiniPageTitle", IDS_SETTINGS_CROSTINI_TITLE},
      {"crostiniPageLabel", IDS_SETTINGS_CROSTINI_LABEL},
      {"crostiniEnable", kIsRevampEnabled
                             ? IDS_OS_SETTINGS_REVAMP_CROSTINI_SET_UP
                             : IDS_SETTINGS_TURN_ON},
      {"crostiniSharedPathsInstructionsAdd",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_INSTRUCTIONS_ADD},
      {"crostiniSharedPathsRemoveFailureDialogMessage",
       IDS_SETTINGS_CROSTINI_SHARED_PATHS_REMOVE_FAILURE_DIALOG_MESSAGE},
      {"crostiniExportImportTitle", IDS_SETTINGS_CROSTINI_EXPORT_IMPORT_TITLE},
      {"crostiniExport", IDS_SETTINGS_CROSTINI_EXPORT},
      {"crostiniExportLabel", IDS_SETTINGS_CROSTINI_EXPORT_LABEL},
      {"crostiniImport", IDS_SETTINGS_CROSTINI_IMPORT},
      {"crostiniImportLabel", IDS_SETTINGS_CROSTINI_IMPORT_LABEL},
      {"crostiniImportConfirmationDialogTitle",
       IDS_SETTINGS_CROSTINI_CONFIRM_IMPORT_DIALOG_WINDOW_TITLE},
      {"crostiniImportConfirmationDialogMessage",
       IDS_SETTINGS_CROSTINI_CONFIRM_IMPORT_DIALOG_WINDOW_MESSAGE},
      {"crostiniImportConfirmationDialogConfirmationButton",
       IDS_SETTINGS_CROSTINI_IMPORT},
      {"crostiniRemoveButton", IDS_SETTINGS_CROSTINI_REMOVE_BUTTON},
      {"crostiniSharedUsbDevicesDescription",
       IDS_SETTINGS_CROSTINI_SHARED_USB_DEVICES_DESCRIPTION},
      {"crostiniArcAdbTitle", IDS_SETTINGS_CROSTINI_ARC_ADB_TITLE},
      {"crostiniArcAdbDescription", IDS_SETTINGS_CROSTINI_ARC_ADB_DESCRIPTION},
      {"crostiniArcAdbLabel", IDS_SETTINGS_CROSTINI_ARC_ADB_LABEL},
      {"crostiniArcAdbRestartButton",
       IDS_SETTINGS_CROSTINI_ARC_ADB_RESTART_BUTTON},
      {"crostiniArcAdbConfirmationTitleEnable",
       IDS_SETTINGS_CROSTINI_ARC_ADB_CONFIRMATION_TITLE_ENABLE},
      {"crostiniArcAdbConfirmationTitleDisable",
       IDS_SETTINGS_CROSTINI_ARC_ADB_CONFIRMATION_TITLE_DISABLE},
      {"crostiniContainerUpgradeButton",
       IDS_SETTINGS_CROSTINI_CONTAINER_UPGRADE_BUTTON},
      {"crostiniPortForwarding", IDS_SETTINGS_CROSTINI_PORT_FORWARDING},
      {"crostiniPortForwardingDescription",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_DESCRIPTION},
      {"crostiniPortForwardingNoPorts",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_NO_PORTS},
      {"crostiniPortForwardingTableTitle",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_TABLE_TITLE},
      {"crostiniPortForwardingListPortNumber",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_LIST_PORT_NUMBER},
      {"crostiniPortForwardingListLabel",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_LIST_LABEL},
      {"crostiniPortForwardingAddPortButton",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_PORT_BUTTON},
      {"crostiniPortForwardingAddPortButtonDescription",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_PORT_BUTTON_DESCRIPTION},
      {"crostiniPortForwardingAddPortDialogTitle",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_PORT_DIALOG_TITLE},
      {"crostiniPortForwardingAddPortDialogPortNumberLabel",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_PORT_DIALOG_PORT_NUMBER_LABEL},
      {"crostiniPortForwardingAddPortDialogLabelLabel",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_PORT_DIALOG_LABEL_LABEL},
      {"crostiniPortForwardingTCP", IDS_SETTINGS_CROSTINI_PORT_FORWARDING_TCP},
      {"crostiniPortForwardingUDP", IDS_SETTINGS_CROSTINI_PORT_FORWARDING_UDP},
      {"crostiniPortForwardingAddError",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_ERROR},
      {"crostiniPortForwardingAddExisting",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ADD_EXISTING},
      {"crostiniPortForwardingRemoveAllPorts",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_REMOVE_ALL_PORTS},
      {"crostiniPortForwardingRemovePort",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_REMOVE_PORT},
      {"crostiniPortForwardingActivatePortError",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_ACTIVATE_PORT_ERROR},
      {"crostiniPortForwardingToggleAriaLabel",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_TOGGLE_PORT_ARIA_LABEL},
      {"crostiniPortForwardingRemoveAllPortsAriaLabel",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_REMOVE_ALL_PORTS_ARIA_LABEL},
      {"crostiniPortForwardingShowMoreActionsAriaLabel",
       IDS_SETTINGS_CROSTINI_PORT_FORWARDING_SHOW_MORE_ACTIONS_ARIA_LABEL},
      {"crostiniDiskResizeTitle", IDS_SETTINGS_CROSTINI_DISK_RESIZE_TITLE},
      {"crostiniDiskResizeShowButton",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_SHOW_BUTTON},
      {"crostiniDiskResizeShowButtonAriaLabel",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_SHOW_BUTTON_ARIA_LABEL},
      {"crostiniDiskSizeCalculating", IDS_SETTINGS_STORAGE_SIZE_CALCULATING},
      {"crostiniDiskReserveSizeButton",
       IDS_SETTINGS_CROSTINI_DISK_RESERVE_SIZE_BUTTON},
      {"crostiniDiskReserveSizeButtonAriaLabel",
       IDS_SETTINGS_CROSTINI_DISK_RESERVE_SIZE_BUTTON_ARIA_LABEL},
      {"crostiniDiskResizeLabel", IDS_SETTINGS_CROSTINI_DISK_RESIZE_LABEL},
      {"crostiniDiskResizeDynamicallyAllocatedSubtext",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_DYNAMICALLY_ALLOCATED_SUBTEXT},
      {"crostiniDiskResizeNotSupportedSubtext",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_NOT_SUPPORTED_SUBTEXT},
      {"crostiniDiskResizeUnsupported",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_UNSUPPORTED},
      {"crostiniDiskResizeLoading", IDS_SETTINGS_CROSTINI_DISK_RESIZE_LOADING},
      {"crostiniDiskResizeError", IDS_SETTINGS_CROSTINI_DISK_RESIZE_ERROR},
      {"crostiniDiskResizeErrorRetry",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_ERROR_RETRY},
      {"crostiniDiskResizeCancel", IDS_SETTINGS_CROSTINI_DISK_RESIZE_CANCEL},
      {"crostiniDiskResizeGoButton",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_GO_BUTTON},
      {"crostiniDiskResizeInProgress",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_IN_PROGRESS},
      {"crostiniDiskResizeResizingError",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_RESIZING_ERROR},
      {"crostiniDiskResizeConfirmationDialogTitle",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_CONFIRMATION_DIALOG_TITLE},
      {"crostiniDiskResizeConfirmationDialogMessage",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_CONFIRMATION_DIALOG_MESSAGE},
      {"crostiniDiskResizeConfirmationDialogButton",
       IDS_SETTINGS_CROSTINI_DISK_RESIZE_CONFIRMATION_DIALOG_BUTTON},
      {"crostiniDiskResizeDone", IDS_SETTINGS_CROSTINI_DISK_RESIZE_DONE},
      {"crostiniMicTitle", IDS_SETTINGS_CROSTINI_MIC_TITLE},
      {"crostiniMicDialogLabel", IDS_SETTINGS_CROSTINI_MIC_DIALOG_LABEL},
      {"crostiniMicDialogShutdownButton",
       IDS_SETTINGS_CROSTINI_MIC_DIALOG_SHUTDOWN_BUTTON},
      {"crostiniRemove", IDS_SETTINGS_CROSTINI_REMOVE},
      {"crostiniExtraContainersLabel",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_LABEL},
      {"crostiniExtraContainersDescription",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_DESCRIPTION},
      {"crostiniExtraContainersCreate",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_CREATE},
      {"crostiniExtraContainersDelete",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_DELETE},
      {"crostiniExtraContainersStop",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_STOP},
      {"crostiniExtraContainersTableTitle",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_TABLE_TITLE},
      {"crostiniExtraContainersVmNameLabel",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_VM_NAME_LABEL},
      {"crostiniExtraContainersContainerNameLabel",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_CONTAINER_NAME_LABEL},
      {"crostiniExtraContainersContainerIpLabel",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_CONTAINER_IP_LABEL},
      {"crostiniExtraContainersShareMicrophone",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_SHARE_MICROPHONE},
      {"crostiniExtraContainersAppBadgeColor",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_APP_BADGE_COLOR},
      {"crostiniExtraContainersCreateDialogTitle",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_CREATE_DIALOG_TITLE},
      {"crostiniExtraContainersCreateDialogContainerExistsError",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_CREATE_DIALOG_CONTAINER_EXISTS_ERROR},
      {"crostiniExtraContainersCreateDialogEmptyContainerNameError",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_CREATE_DIALOG_EMPTY_CONTAINER_NAME_ERROR},
      {"crostiniExtraContainersCreateDialogImageServer",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_CREATE_DIALOG_IMAGE_SERVER},
      {"crostiniExtraContainersCreateDialogImageAlias",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_CREATE_DIALOG_IMAGE_ALIAS},
      {"crostiniExtraContainersCreateDialogAddContainerFile",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_CREATE_DIALOG_ADD_CONTAINER_LABEL},
      {"crostiniExtraContainersCreateDialogAddContainerButtonLabel",
       IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_CREATE_DIALOG_ADD_CONTAINER_BUTTON_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString(
      "crostiniContainerUpgrade",
      l10n_util::GetStringUTF16(
          IDS_OS_SETTINGS_CROSTINI_CONTAINER_UPGRADE_BOOKWORM_MESSAGE));

  if (auto* pretty_name_value = guest_os::GetContainerPrefValue(
          profile_, crostini::DefaultContainerId(),
          guest_os::prefs::kContainerOsPrettyNameKey)) {
    std::string pretty_name = pretty_name_value->GetString();
    html_source->AddString("crostiniContainerUpgradeSubtext",
                           l10n_util::GetStringFUTF16(
                               IDS_SETTINGS_CROSTINI_CONTAINER_UPGRADE_SUBTEXT,
                               base::UTF8ToUTF16(pretty_name)));
  } else {
    // Blank the subtext if we don't know what the pretty version name is. This
    // is just a fallback for users that haven't opened crostini since before we
    // started recording that.
    html_source->AddString("crostiniContainerUpgradeSubtext", "");
  }

  // Crostini section in settings is always displayed.
  // Should we show that Crostini is supported?
  html_source->AddBoolean(
      "isCrostiniSupported",
      crostini::CrostiniFeatures::Get()->CouldBeAllowed(profile_));

  // Should we actually enable the button to install it?
  html_source->AddBoolean(
      "isCrostiniAllowed",
      crostini::CrostiniFeatures::Get()->IsAllowedNow(profile_));

  // Should Bruschetta be displayed in the settings at all?
  html_source->AddBoolean("showBruschetta", ShouldShowBruschetta(profile_));

  auto bruschetta_name = bruschetta::GetOverallVmName(profile_);

  html_source->AddString("bruschettaPageLabel",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_BRUSCHETTA_LABEL, bruschetta_name));

  auto learn_more_url =
      base::UTF8ToUTF16(bruschetta::GetLearnMoreUrl(profile_).spec());
  if (learn_more_url.empty()) {
    html_source->AddString(
        "bruschettaSubtext",
        l10n_util::GetStringFUTF16(IDS_SETTINGS_BRUSCHETTA_SUBTEXT_NO_LINK,
                                   ui::GetChromeOSDeviceName()));
  } else {
    html_source->AddString("bruschettaSubtext",
                           l10n_util::GetStringFUTF16(
                               IDS_SETTINGS_BRUSCHETTA_SUBTEXT,
                               ui::GetChromeOSDeviceName(), learn_more_url));
  }

  html_source->AddString(
      "bruschettaSharedUsbDevicesDescription",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_BRUSCHETTA_SHARED_USB_DEVICES_DESCRIPTION,
          bruschetta_name));
  html_source->AddString(
      "bruschettaSharedPathsInstructionsLocate",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_BRUSCHETTA_SHARED_PATHS_INSTRUCTIONS_LOCATE,
          bruschetta_name,
          base::ASCIIToUTF16(
              bruschetta::BruschettaChromeOSBaseDirectory().value())));
  html_source->AddString(
      "bruschettaSharedPathsInstructionsAdd",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_BRUSCHETTA_SHARED_PATHS_INSTRUCTIONS_ADD,
          bruschetta_name));
  html_source->AddString(
      "bruschettaSharedPathsRemoveFailureDialogMessage",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_BRUSCHETTA_SHARED_PATHS_REMOVE_FAILURE_DIALOG_MESSAGE,
          bruschetta_name));
  html_source->AddString("bruschettaRemove",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_BRUSCHETTA_REMOVE, bruschetta_name));
  html_source->AddString(
      "bruschettaMicTitle",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_BRUSCHETTA_MIC_TITLE,
                                 bruschetta_name));
  html_source->AddString(
      "bruschettaMicDialogLabel",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_BRUSCHETTA_MIC_DIALOG_LABEL,
                                 bruschetta_name));

  html_source->AddString(
      "crostiniSubtext",
      kIsRevampEnabled
          ? l10n_util::GetStringFUTF16(
                IDS_OS_SETTINGS_REVAMP_CROSTINI_SUBTEXT,
                GetHelpUrlWithBoard(chrome::kLinuxAppsLearnMoreURL))
          : l10n_util::GetStringFUTF16(
                IDS_SETTINGS_CROSTINI_SUBTEXT, ui::GetChromeOSDeviceName(),
                GetHelpUrlWithBoard(chrome::kLinuxAppsLearnMoreURL)));
  html_source->AddString(
      "crostiniSubtextNotSupported",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_SUBTEXT_NOT_SUPPORTED,
          ui::GetChromeOSDeviceName(),
          GetHelpUrlWithBoard(chrome::kLinuxAppsLearnMoreURL)));
  html_source->AddString(
      "crostiniArcAdbPowerwashRequiredSublabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_ARC_ADB_POWERWASH_REQUIRED_SUBLABEL,
          chrome::kArcAdbSideloadingLearnMoreURL));
  html_source->AddString(
      "crostiniArcAdbConfirmationMessageEnable",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_ARC_ADB_CONFIRMATION_MESSAGE_ENABLE,
          ui::GetChromeOSDeviceName()));
  html_source->AddString(
      "crostiniArcAdbConfirmationMessageDisable",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_ARC_ADB_CONFIRMATION_MESSAGE_DISABLE,
          ui::GetChromeOSDeviceName()));
  html_source->AddString(
      "crostiniSharedPathsInstructionsLocate",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_SHARED_PATHS_INSTRUCTIONS_LOCATE,
          base::ASCIIToUTF16(
              crostini::ContainerChromeOSBaseDirectory().value())));
  html_source->AddString(
      "crostiniDiskResizeRecommended",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_DISK_RESIZE_RECOMMENDED,
          ui::FormatBytes(crostini::disk::kRecommendedDiskSizeBytes)));
  html_source->AddString(
      "crostiniDiskResizeRecommendedWarning",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_DISK_RESIZE_RECOMMENDED_WARNING,
          ui::FormatBytes(crostini::disk::kRecommendedDiskSizeBytes)));

  html_source->AddBoolean("showCrostiniExportImport", IsExportImportAllowed());
  html_source->AddBoolean("arcAdbSideloadingSupported",
                          IsAdbSideloadingAllowed());
  html_source->AddBoolean("showCrostiniPortForwarding",
                          IsPortForwardingAllowed());
  html_source->AddBoolean("showCrostiniExtraContainers",
                          IsMultiContainerAllowed());
  html_source->AddBoolean("isOwnerProfile",
                          ProfileHelper::IsOwnerProfile(profile_));
  html_source->AddBoolean("isEnterpriseManaged",
                          IsDeviceManaged() || IsProfileManaged(profile_));
  html_source->AddBoolean("showCrostiniContainerUpgrade",
                          IsContainerUpgradeAllowed());
}

void CrostiniSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(std::make_unique<GuestOsHandler>(profile_));
  web_ui->AddMessageHandler(std::make_unique<CrostiniHandler>(profile_));
}

int CrostiniSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_CROSTINI_TITLE;
}

mojom::Section CrostiniSection::GetSection() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::Section::kAboutChromeOs
             : mojom::Section::kCrostini;
}

mojom::SearchResultIcon CrostiniSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kDeveloperTags;
}

const char* CrostiniSection::GetSectionPath() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::kAboutChromeOsSectionPath
             : mojom::kCrostiniSectionPath;
}

bool CrostiniSection::LogMetric(mojom::Setting setting,
                                base::Value& value) const {
  // Unimplemented.
  return false;
}

void CrostiniSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kSetUpCrostini);

  // Crostini details.
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_CROSTINI_LABEL,
                                     mojom::Subpage::kCrostiniDetails,
                                     mojom::SearchResultIcon::kDeveloperTags,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kCrostiniDetailsSubpagePath);
  static constexpr mojom::Setting kCrostiniDetailsSettings[] = {
      mojom::Setting::kCrostiniContainerUpgrade,
      mojom::Setting::kCrostiniDiskResize,
      mojom::Setting::kCrostiniMicAccess,
      mojom::Setting::kUninstallCrostini,
      mojom::Setting::kBruschettaMicAccess,
      mojom::Setting::kGuestUsbNotification,
      mojom::Setting::kGuestUsbPersistentPassthrough,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kCrostiniDetails,
                            kCrostiniDetailsSettings, generator);

  // Manage shared folders.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_GUEST_OS_SHARED_PATHS,
      mojom::Subpage::kCrostiniManageSharedFolders,
      mojom::Subpage::kCrostiniDetails, mojom::SearchResultIcon::kPenguin,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kCrostiniManageSharedFoldersSubpagePath);

  // USB preferences.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_LABEL,
      mojom::Subpage::kCrostiniUsbPreferences, mojom::Subpage::kCrostiniDetails,
      mojom::SearchResultIcon::kPenguin,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kCrostiniUsbPreferencesSubpagePath);

  // Backup and restore.
  generator->RegisterNestedSubpage(IDS_SETTINGS_CROSTINI_EXPORT_IMPORT_TITLE,
                                   mojom::Subpage::kCrostiniBackupAndRestore,
                                   mojom::Subpage::kCrostiniDetails,
                                   mojom::SearchResultIcon::kPenguin,
                                   mojom::SearchResultDefaultRank::kMedium,
                                   mojom::kCrostiniBackupAndRestoreSubpagePath);
  static constexpr mojom::Setting kCrostiniBackupAndRestoreSettings[] = {
      mojom::Setting::kBackupLinuxAppsAndFiles,
      mojom::Setting::kRestoreLinuxAppsAndFiles,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kCrostiniBackupAndRestore,
                            kCrostiniBackupAndRestoreSettings, generator);

  // Develop Android apps.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_CROSTINI_ARC_ADB_TITLE,
      mojom::Subpage::kCrostiniDevelopAndroidApps,
      mojom::Subpage::kCrostiniDetails, mojom::SearchResultIcon::kDeveloperTags,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kCrostiniDevelopAndroidAppsSubpagePath);
  generator->RegisterNestedSetting(mojom::Setting::kCrostiniAdbDebugging,
                                   mojom::Subpage::kCrostiniDevelopAndroidApps);

  // Port forwarding.
  generator->RegisterNestedSubpage(IDS_SETTINGS_CROSTINI_PORT_FORWARDING,
                                   mojom::Subpage::kCrostiniPortForwarding,
                                   mojom::Subpage::kCrostiniDetails,
                                   mojom::SearchResultIcon::kPenguin,
                                   mojom::SearchResultDefaultRank::kMedium,
                                   mojom::kCrostiniPortForwardingSubpagePath);

  // Extra containers.
  generator->RegisterNestedSubpage(IDS_SETTINGS_CROSTINI_EXTRA_CONTAINERS_LABEL,
                                   mojom::Subpage::kCrostiniExtraContainers,
                                   mojom::Subpage::kCrostiniDetails,
                                   mojom::SearchResultIcon::kPenguin,
                                   mojom::SearchResultDefaultRank::kMedium,
                                   mojom::kCrostiniExtraContainersSubpagePath);

  // Bruschetta subpage.
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_BRUSCHETTA_LABEL,
                                     mojom::Subpage::kBruschettaDetails,
                                     mojom::SearchResultIcon::kDeveloperTags,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kBruschettaDetailsSubpagePath);

  // USB preferences.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_LABEL,
      mojom::Subpage::kBruschettaUsbPreferences,
      mojom::Subpage::kBruschettaDetails, mojom::SearchResultIcon::kPenguin,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kBruschettaUsbPreferencesSubpagePath);

  // Manage shared folders.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_GUEST_OS_SHARED_PATHS,
      mojom::Subpage::kBruschettaManageSharedFolders,
      mojom::Subpage::kBruschettaDetails, mojom::SearchResultIcon::kPenguin,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kBruschettaManageSharedFoldersSubpagePath);
}

bool CrostiniSection::IsExportImportAllowed() const {
  return crostini::CrostiniFeatures::Get()->IsExportImportUIAllowed(profile_);
}

bool CrostiniSection::IsContainerUpgradeAllowed() const {
  return crostini::ShouldAllowContainerUpgrade(profile_);
}

bool CrostiniSection::IsPortForwardingAllowed() const {
  return crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_);
}

bool CrostiniSection::IsMultiContainerAllowed() const {
  return crostini::CrostiniFeatures::Get()->IsMultiContainerAllowed(profile_);
}

void CrostiniSection::UpdateSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  updater.RemoveSearchTags(GetCrostiniOptedInSearchConcepts());
  updater.RemoveSearchTags(
      GetCrostiniOptedOutSearchConcepts(GetSection(), GetSectionPath()));
  updater.RemoveSearchTags(GetCrostiniExportImportSearchConcepts());
  updater.RemoveSearchTags(GetCrostiniAdbSideloadingSearchConcepts());
  updater.RemoveSearchTags(GetCrostiniPortForwardingSearchConcepts());
  updater.RemoveSearchTags(GetCrostiniContainerUpgradeSearchConcepts());
  updater.RemoveSearchTags(GetCrostiniDiskResizingSearchConcepts());

  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile_) ||
      !pref_service_->GetBoolean(crostini::prefs::kCrostiniEnabled)) {
    updater.AddSearchTags(
        GetCrostiniOptedOutSearchConcepts(GetSection(), GetSectionPath()));
    return;
  }

  updater.AddSearchTags(GetCrostiniOptedInSearchConcepts());

  if (IsExportImportAllowed()) {
    updater.AddSearchTags(GetCrostiniExportImportSearchConcepts());
  }

  if (IsAdbSideloadingAllowed() &&
      pref_service_->GetBoolean(arc::prefs::kArcEnabled)) {
    updater.AddSearchTags(GetCrostiniAdbSideloadingSearchConcepts());
  }

  if (IsPortForwardingAllowed()) {
    updater.AddSearchTags(GetCrostiniPortForwardingSearchConcepts());
  }

  if (IsContainerUpgradeAllowed()) {
    updater.AddSearchTags(GetCrostiniContainerUpgradeSearchConcepts());
  }

  updater.AddSearchTags(GetCrostiniDiskResizingSearchConcepts());

  // TODO(crbug:1261319): search concepts for extras containers.
}

}  // namespace ash::settings
