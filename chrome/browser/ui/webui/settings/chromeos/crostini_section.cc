// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/crostini_section.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_disk.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/chromeos/crostini_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/guest_os_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {
namespace settings {
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

const std::vector<SearchConcept>& GetCrostiniOptedOutSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CROSTINI,
       mojom::kCrostiniSectionPath,
       mojom::SearchResultIcon::kDeveloperTags,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kCrostini},
       {IDS_OS_SETTINGS_TAG_CROSTINI_ALT1, IDS_OS_SETTINGS_TAG_CROSTINI_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CROSTINI_SETUP,
       mojom::kCrostiniSectionPath,
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
  return webui::IsEnterpriseManaged();
}

bool IsAdbSideloadingAllowed() {
  return base::FeatureList::IsEnabled(features::kArcAdbSideloadingFeature);
}

bool IsDiskResizingAllowed() {
  return base::FeatureList::IsEnabled(features::kCrostiniDiskResizing);
}

}  // namespace

CrostiniSection::CrostiniSection(Profile* profile,
                                 SearchTagRegistry* search_tag_registry,
                                 PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      pref_service_(pref_service) {
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

void CrostiniSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"crostiniPageTitle", IDS_SETTINGS_CROSTINI_TITLE},
      {"crostiniPageLabel", IDS_SETTINGS_CROSTINI_LABEL},
      {"crostiniEnable", IDS_SETTINGS_TURN_ON},
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
      {"crostiniContainerUpgrade",
       IDS_SETTINGS_CROSTINI_CONTAINER_UPGRADE_MESSAGE},
      {"crostiniContainerUpgradeSubtext",
       IDS_SETTINGS_CROSTINI_CONTAINER_UPGRADE_SUBTEXT},
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
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  // Should the crostini section in settings be displayed?
  html_source->AddBoolean(
      "showCrostini",
      crostini::CrostiniFeatures::Get()->CouldBeAllowed(profile()));
  // Should we actually enable the button to install it?
  html_source->AddBoolean(
      "allowCrostini",
      crostini::CrostiniFeatures::Get()->IsAllowedNow(profile()));

  html_source->AddString(
      "crostiniSubtext",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_SUBTEXT, ui::GetChromeOSDeviceName(),
          GetHelpUrlWithBoard(chrome::kLinuxAppsLearnMoreURL)));
  html_source->AddString(
      "crostiniArcAdbPowerwashRequiredSublabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_CROSTINI_ARC_ADB_POWERWASH_REQUIRED_SUBLABEL,
          base::ASCIIToUTF16(chrome::kArcAdbSideloadingLearnMoreURL)));
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
  html_source->AddBoolean("isOwnerProfile",
                          chromeos::ProfileHelper::IsOwnerProfile(profile()));
  html_source->AddBoolean("isEnterpriseManaged",
                          IsDeviceManaged() || IsProfileManaged(profile()));
  html_source->AddBoolean("showCrostiniContainerUpgrade",
                          IsContainerUpgradeAllowed());
  html_source->AddBoolean("showCrostiniDiskResize", IsDiskResizingAllowed());
}

void CrostiniSection::AddHandlers(content::WebUI* web_ui) {
  if (crostini::CrostiniFeatures::Get()->CouldBeAllowed(profile())) {
    web_ui->AddMessageHandler(std::make_unique<GuestOsHandler>(profile()));
    web_ui->AddMessageHandler(std::make_unique<CrostiniHandler>(profile()));
  }
}

int CrostiniSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_CROSTINI_TITLE;
}

mojom::Section CrostiniSection::GetSection() const {
  return mojom::Section::kCrostini;
}

mojom::SearchResultIcon CrostiniSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kDeveloperTags;
}

std::string CrostiniSection::GetSectionPath() const {
  return mojom::kCrostiniSectionPath;
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
}

bool CrostiniSection::IsExportImportAllowed() {
  return crostini::CrostiniFeatures::Get()->IsExportImportUIAllowed(profile());
}

bool CrostiniSection::IsContainerUpgradeAllowed() {
  return crostini::ShouldAllowContainerUpgrade(profile());
}

bool CrostiniSection::IsPortForwardingAllowed() {
  return crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile());
}

void CrostiniSection::UpdateSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  updater.RemoveSearchTags(GetCrostiniOptedInSearchConcepts());
  updater.RemoveSearchTags(GetCrostiniOptedOutSearchConcepts());
  updater.RemoveSearchTags(GetCrostiniExportImportSearchConcepts());
  updater.RemoveSearchTags(GetCrostiniAdbSideloadingSearchConcepts());
  updater.RemoveSearchTags(GetCrostiniPortForwardingSearchConcepts());
  updater.RemoveSearchTags(GetCrostiniContainerUpgradeSearchConcepts());
  updater.RemoveSearchTags(GetCrostiniDiskResizingSearchConcepts());

  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile()))
    return;

  if (!pref_service_->GetBoolean(crostini::prefs::kCrostiniEnabled)) {
    updater.AddSearchTags(GetCrostiniOptedOutSearchConcepts());
    return;
  }

  updater.AddSearchTags(GetCrostiniOptedInSearchConcepts());

  if (IsExportImportAllowed())
    updater.AddSearchTags(GetCrostiniExportImportSearchConcepts());

  if (IsAdbSideloadingAllowed() &&
      pref_service_->GetBoolean(arc::prefs::kArcEnabled)) {
    updater.AddSearchTags(GetCrostiniAdbSideloadingSearchConcepts());
  }

  if (IsPortForwardingAllowed())
    updater.AddSearchTags(GetCrostiniPortForwardingSearchConcepts());

  if (IsContainerUpgradeAllowed())
    updater.AddSearchTags(GetCrostiniContainerUpgradeSearchConcepts());

  if (IsDiskResizingAllowed())
    updater.AddSearchTags(GetCrostiniDiskResizingSearchConcepts());
}

}  // namespace settings
}  // namespace chromeos
