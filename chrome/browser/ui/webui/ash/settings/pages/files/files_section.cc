// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/files/files_section.h"

#include "ash/constants/ash_features.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_handler.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_shares_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/user.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kFilesSectionPath;
using ::chromeos::settings::mojom::kGoogleDriveSubpagePath;
using ::chromeos::settings::mojom::kNetworkFileSharesSubpagePath;
using ::chromeos::settings::mojom::kOfficeFilesSubpagePath;
using ::chromeos::settings::mojom::kOneDriveSubpagePath;
using ::chromeos::settings::mojom::kSystemPreferencesSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetDefaultSearchConcepts(
    mojom::Section section,
    const char* section_path) {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_FILES,
       section_path,
       mojom::SearchResultIcon::kFolder,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = section}},
      {IDS_OS_SETTINGS_TAG_FILES_NETWORK_FILE_SHARES,
       mojom::kNetworkFileSharesSubpagePath,
       mojom::SearchResultIcon::kFolderShared,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kNetworkFileShares},
       {IDS_OS_SETTINGS_TAG_FILES_NETWORK_FILE_SHARES_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetFilesMicrosoft365SearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_FILES_MICROSOFT_365,
        mojom::kOfficeFilesSubpagePath,
        mojom::SearchResultIcon::kFolder,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSubpage,
        {.subpage = mojom::Subpage::kOfficeFiles},
        {IDS_OS_SETTINGS_TAG_FILES_MICROSOFT_365_ALT1,
         SearchConcept::kAltTagEnd}}});
  return *tags;
}

const std::vector<SearchConcept>& GetFilesOneDriveSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_FILES_ONEDRIVE,
        mojom::kOneDriveSubpagePath,
        mojom::SearchResultIcon::kOneDrive,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSubpage,
        {.subpage = mojom::Subpage::kOneDrive}}});
  return *tags;
}

// Returns specific search terms to surface the "File sync" feature.
const std::vector<SearchConcept>& GetFilesGoogleDriveFileSyncSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_FILES_GOOGLE_DRIVE_FILE_SYNC,
        mojom::kGoogleDriveSubpagePath,
        mojom::SearchResultIcon::kGoogleDrive,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSubpage,
        {.setting = mojom::Setting::kGoogleDriveFileSync},
        {IDS_OS_SETTINGS_TAG_FILES_GOOGLE_DRIVE_FILE_SYNC_ALT1,
         SearchConcept::kAltTagEnd}}});
  return *tags;
}

// Returns search terms to navigate to the Google Drive subpage when the feature
// is enabled.
const std::vector<SearchConcept>& GetFilesGoogleDriveSubpageSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_FILES_GOOGLE_DRIVE,
        mojom::kGoogleDriveSubpagePath,
        mojom::SearchResultIcon::kGoogleDrive,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSubpage,
        {.subpage = mojom::Subpage::kGoogleDrive}},
       {IDS_OS_SETTINGS_TAG_FILES_REMOVE_GOOGLE_DRIVE_ACCESS,
        mojom::kGoogleDriveSubpagePath,
        mojom::SearchResultIcon::kGoogleDrive,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kGoogleDriveRemoveAccess}}});
  return *tags;
}

}  // namespace

FilesSection::FilesSection(Profile* profile,
                           SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(
      GetDefaultSearchConcepts(GetSection(), GetSectionPath()));
  if (chromeos::IsEligibleAndEnabledUploadOfficeToCloud(profile)) {
    updater.AddSearchTags(GetFilesMicrosoft365SearchConcepts());
    updater.AddSearchTags(GetFilesOneDriveSearchConcepts());
  }

  if (drive::util::IsDriveFsBulkPinningAvailable(profile)) {
    updater.AddSearchTags(GetFilesGoogleDriveFileSyncSearchConcepts());
  }

  updater.AddSearchTags(GetFilesGoogleDriveSubpageSearchConcepts());
}

FilesSection::~FilesSection() = default;

void FilesSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"disconnectGoogleDriveAccount", IDS_SETTINGS_DISCONNECT_GOOGLE_DRIVE},
      {"googleDriveLabel", IDS_SETTINGS_GOOGLE_DRIVE},
      {"googleDriveConnectLabel", IDS_SETTINGS_GOOGLE_DRIVE_CONNECT},
      {"googleDriveRemoveAccessDialogTitle",
       IDS_SETTINGS_GOOGLE_DRIVE_REMOVE_ACCESS_DIALOG_TITLE},
      {"googleDriveRemoveAccessDialogBody",
       IDS_SETTINGS_GOOGLE_DRIVE_REMOVE_ACCESS_DIALOG_BODY},
      {"googleDriveRemoveButtonText",
       IDS_SETTINGS_GOOGLE_DRIVE_REMOVE_BUTTON_TEXT},
      {"googleDriveRemoveDriveAccessButtonText",
       IDS_SETTINGS_GOOGLE_DRIVE_REMOVE_ACCESS_BUTTON_LABEL},
      {"googleDriveFileSyncTitle", IDS_SETTINGS_GOOGLE_DRIVE_FILE_SYNC_TITLE},
      {"googleDriveFileSyncSubtitleWithStorage",
       IDS_SETTINGS_GOOGLE_DRIVE_FILE_SYNC_SUBTITLE_WITH_STORAGE},
      {"googleDriveFileSyncSubtitleWithoutStorage",
       IDS_SETTINGS_GOOGLE_DRIVE_FILE_SYNC_SUBTITLE_WITHOUT_STORAGE},
      {"googleDriveOfflineStorageTitle",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_STORAGE_TITLE},
      {"googleDriveOfflineStorageSpaceTaken",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_STORAGE_SPACE_TAKEN},
      {"googleDriveOfflineClearCalculatingSubtitle",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_CLEAR_CALCULATING_SUBTITLE},
      {"googleDriveOfflineClearErrorSubtitle",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_CLEAR_ERROR_SUBTITLE},
      {"googleDriveCleanUpStorageAction",
       IDS_SETTINGS_GOOGLE_DRIVE_CLEAN_UP_STORAGE_ACTION},
      {"googleDriveOfflineCleanStorageDialogTitle",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_CLEAN_UP_STORAGE_TITLE},
      {"googleDriveOfflineCleanStorageDialogBody",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_CLEAN_UP_STORAGE_BODY},
      {"googleDriveCleanUpStorageDisabledFileSyncTooltip",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_CLEAN_UP_STORAGE_DISABLED_FILE_SYNC_TOOLTIP},
      {"googleDriveCleanUpStorageDisabledTooltip",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_CLEAN_UP_STORAGE_DISABLED_TOOLTIP},
      {"googleDriveCleanUpStorageDisabledUnknownStorageTooltip",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_CLEAN_UP_STORAGE_DISABLED_UNKNOWN_STORAGE_TOOLTIP},
      {"googleDriveTurnOffLabel",
       IDS_SETTINGS_GOOGLE_DRIVE_TURN_OFF_BUTTON_LABEL},
      {"googleDriveFileSyncTurnOffTitle",
       IDS_SETTINGS_GOOGLE_DRIVE_FILE_SYNC_TURN_OFF_TITLE_TEXT},
      {"googleDriveFileSyncTurnOffBody",
       IDS_SETTINGS_GOOGLE_DRIVE_FILE_SYNC_TURN_OFF_BODY_TEXT},
      {"googleDriveFileSyncListingFilesTitle",
       IDS_SETTINGS_GOOGLE_DRIVE_FILE_SYNC_LISTING_FILES_TITLE_TEXT},
      {"googleDriveFileSyncListingFilesItemsFoundBody",
       IDS_SETTINGS_GOOGLE_DRIVE_FILE_SYNC_LISTING_FILES_ITEMS_FOUND_BODY_TEXT},
      {"googleDriveNotEnoughSpaceTitle",
       IDS_SETTINGS_GOOGLE_DRIVE_BULK_PINNING_NOT_ENOUGH_SPACE_TITLE_TEXT},
      {"googleDriveNotEnoughSpaceBody",
       IDS_SETTINGS_GOOGLE_DRIVE_BULK_PINNING_NOT_ENOUGH_SPACE_BODY_TEXT},
      {"googleDriveFileSyncUnexpectedErrorTitle",
       IDS_SETTINGS_GOOGLE_DRIVE_FILE_SYNC_UNEXPECTED_ERROR_TITLE_TEXT},
      {"googleDriveFileSyncUnexpectedErrorBody",
       IDS_SETTINGS_GOOGLE_DRIVE_FILE_SYNC_UNEXPECTED_ERROR_BODY_TEXT},
      {"googleDriveFileSyncOfflineErrorTitle",
       IDS_SETTINGS_GOOGLE_DRIVE_FILE_SYNC_OFFLINE_ERROR_TITLE_TEXT},
      {"googleDriveFileSyncOfflineErrorBody",
       IDS_SETTINGS_GOOGLE_DRIVE_FILE_SYNC_OFFLINE_ERROR_BODY_TEXT},
      {"googleDriveDismissButtonText",
       IDS_SETTINGS_GOOGLE_DRIVE_DISMISS_BUTTON_TEXT},
      {"googleDriveOkButtonText", IDS_SETTINGS_GOOGLE_DRIVE_OK_BUTTON_TEXT},
      {"googleDriveNotSignedInSublabel",
       IDS_SETTINGS_GOOGLE_DRIVE_NOT_SIGNED_IN_SUBLABEL},
      {"googleDriveFileSyncOnSublabel",
       IDS_SETTINGS_GOOGLE_DRIVE_FILE_SYNC_ON_SUBLABEL},
      {"googleDriveEnabledOnMeteredNetworkLabel",
       IDS_SETTINGS_GOOGLE_DRIVE_ENABLED_ON_METERED_NETWORK_LABEL},
      {"filesPageTitle", IDS_OS_SETTINGS_FILES},
      {"smbSharesTitle", IDS_SETTINGS_DOWNLOADS_SMB_SHARES},
      {"smbSharesLearnMoreLabel",
       IDS_SETTINGS_DOWNLOADS_SMB_SHARES_LEARN_MORE_LABEL},
      {"addSmbShare", IDS_SETTINGS_DOWNLOADS_SMB_SHARES_ADD_SHARE},
      {"smbShareAddedSuccessfulMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_SUCCESS_MESSAGE},
      {"smbShareAddedErrorMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_ERROR_MESSAGE},
      {"smbShareAddedAuthFailedMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_AUTH_FAILED_MESSAGE},
      {"smbShareAddedNotFoundMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_NOT_FOUND_MESSAGE},
      {"smbShareAddedUnsupportedDeviceMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_UNSUPPORTED_DEVICE_MESSAGE},
      {"smbShareAddedMountExistsMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_EXISTS_MESSAGE},
      {"smbShareAddedTooManyMountsMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_TOO_MANY_MOUNTS_MESSAGE},
      {"smbShareAddedInvalidURLMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_INVALID_URL_MESSAGE},
      {"smbShareAddedInvalidSSOURLMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_INVALID_SSO_URL_MESSAGE},
      {"oneDriveLabel", IDS_SETTINGS_ONE_DRIVE_LABEL},
      {"oneDriveSignedInAs", IDS_SETTINGS_ONE_DRIVE_SIGNED_IN_AS},
      {"oneDriveDisconnected", IDS_SETTINGS_ONE_DRIVE_DISCONNECTED},
      {"oneDriveLoading", IDS_OS_SETTINGS_ONE_DRIVE_LOADING},
      {"oneDriveConnect", IDS_SETTINGS_ONE_DRIVE_CONNECT},
      {"oneDriveDisconnect", IDS_SETTINGS_ONE_DRIVE_DISCONNECT},
      {"openOneDriveFolder", IDS_SETTINGS_OPEN_ONE_DRIVE_FOLDER},
      {"officeLabel", IDS_SETTINGS_OFFICE_LABEL},
      {"officeSublabel", IDS_SETTINGS_OFFICE_SUBLABEL},
      {"officeSubpageTitle", IDS_SETTINGS_OFFICE_SUBPAGE_TITLE},
      {"alwaysMoveToDrivePreferenceLabel",
       IDS_SETTINGS_ALWAYS_MOVE_OFFICE_TO_DRIVE_PREFERENCE_LABEL},
      {"alwaysMoveToOneDrivePreferenceLabel",
       IDS_SETTINGS_ALWAYS_MOVE_OFFICE_TO_ONEDRIVE_PREFERENCE_LABEL},
      {"smbSharesTitleDescription",
       IDS_OS_SETTINGS_REVAMP_DOWNLOADS_SMB_SHARES_DESCRIPTION},
      {"googleDriveFileSyncSectionTitle",
       IDS_SETTINGS_GOOGLE_DRIVE_FILE_SYNC_SECTION_TITLE},
      {"googleDriveMirrorSyncLabel",
       IDS_SETTINGS_GOOGLE_DRIVE_MIRROR_SYNC_TOGGLE_LABEL},
      {"googleDriveMirrorSyncDescription",
       IDS_SETTINGS_GOOGLE_DRIVE_MIRROR_SYNC_TOGGLE_DESCRIPTION}};
  html_source->AddLocalizedStrings(kLocalizedStrings);

  smb_dialog::AddLocalizedStrings(html_source);

  html_source->AddString("smbSharesLearnMoreURL",
                         GetHelpUrlWithBoard(chrome::kSmbSharesLearnMoreURL));

  html_source->AddString(
      "googleDriveCleanUpStorageLearnMoreLink",
      GetHelpUrlWithBoard(chrome::kGoogleDriveCleanUpStorageLearnMoreURL));

  html_source->AddString(
      "googleDriveFileSyncLearnMoreLink",
      GetHelpUrlWithBoard(chrome::kGoogleDriveOfflineLearnMoreURL));

  html_source->AddBoolean(
      "showOneDriveSettings",
      ash::cloud_upload::
          IsMicrosoftOfficeOneDriveIntegrationAllowedAndOdfsInstalled(
              profile()));

  html_source->AddBoolean(
      "showOfficeSettings",
      chromeos::cloud_upload::IsMicrosoftOfficeCloudUploadAllowed(profile()));

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile());
  if (user && user->GetAccountId().is_valid()) {
    html_source->AddString(
        "googleDriveSignedInAs",
        l10n_util::GetStringFUTF16(IDS_SETTINGS_GOOGLE_DRIVE_SIGNED_IN_AS,
                                   base::ASCIIToUTF16(user->display_email())));
    html_source->AddString(
        "googleDriveReconnectAs",
        l10n_util::GetStringFUTF16(IDS_SETTINGS_GOOGLE_DRIVE_RECONNECT_AS,
                                   base::ASCIIToUTF16(user->display_email())));
  }

  html_source->AddBoolean(
      "enableDriveFsBulkPinning",
      drive::util::IsDriveFsBulkPinningAvailable(profile()));

  html_source->AddBoolean(
      "enableSkyVault",
      base::FeatureList::IsEnabled(::features::kSkyVault) &&
          base::FeatureList::IsEnabled(::features::kSkyVaultV2));

  html_source->AddBoolean("enableDriveFsMirrorSync",
                          drive::util::IsDriveFsMirrorSyncAvailable(profile()));
}

void FilesSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<smb_dialog::SmbHandler>(profile(), base::DoNothing()));
}

int FilesSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_FILES;
}

mojom::Section FilesSection::GetSection() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::Section::kSystemPreferences
             : mojom::Section::kFiles;
}

mojom::SearchResultIcon FilesSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kFolder;
}

const char* FilesSection::GetSectionPath() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::kSystemPreferencesSectionPath
             : mojom::kFilesSectionPath;
}

bool FilesSection::LogMetric(mojom::Setting setting, base::Value& value) const {
  // No metrics are logged.
  return false;
}

void FilesSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kGoogleDriveConnection);
  generator->RegisterTopLevelSetting(mojom::Setting::kGoogleDriveFileSync);
  generator->RegisterTopLevelSetting(mojom::Setting::kGoogleDriveRemoveAccess);

  // Network file shares.
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_DOWNLOADS_SMB_SHARES,
                                     mojom::Subpage::kNetworkFileShares,
                                     mojom::SearchResultIcon::kFolderShared,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kNetworkFileSharesSubpagePath);

  // MS Office.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_OFFICE_LABEL, mojom::Subpage::kOfficeFiles,
      mojom::SearchResultIcon::kFolder, mojom::SearchResultDefaultRank::kMedium,
      mojom::kNetworkFileSharesSubpagePath);

  // Google Drive.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_GOOGLE_DRIVE, mojom::Subpage::kGoogleDrive,
      mojom::SearchResultIcon::kGoogleDrive,
      mojom::SearchResultDefaultRank::kMedium, mojom::kGoogleDriveSubpagePath);

  // One Drive
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_ONE_DRIVE_LABEL, mojom::Subpage::kOneDrive,
      mojom::SearchResultIcon::kOneDrive,
      mojom::SearchResultDefaultRank::kMedium, mojom::kOneDriveSubpagePath);
}

}  // namespace ash::settings
