// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/files_section.h"

#include "ash/constants/ash_features.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_handler.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_shares_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/ash/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/user.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kFilesSectionPath;
using ::chromeos::settings::mojom::kNetworkFileSharesSubpagePath;
using ::chromeos::settings::mojom::kOfficeFilesSubpagePath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetFilesSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_FILES,
       mojom::kFilesSectionPath,
       mojom::SearchResultIcon::kFolder,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kFiles}},
      {IDS_OS_SETTINGS_TAG_FILES_DISCONNECT_GOOGLE_DRIVE,
       mojom::kFilesSectionPath,
       mojom::SearchResultIcon::kDrive,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kGoogleDriveConnection}},
      {IDS_OS_SETTINGS_TAG_FILES_NETWORK_FILE_SHARES,
       mojom::kNetworkFileSharesSubpagePath,
       mojom::SearchResultIcon::kFolder,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kNetworkFileShares},
       {IDS_OS_SETTINGS_TAG_FILES_NETWORK_FILE_SHARES_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetFilesOfficeSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_FILES_OFFICE,
        mojom::kOfficeFilesSubpagePath,
        mojom::SearchResultIcon::kFolder,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSubpage,
        {.subpage = mojom::Subpage::kOfficeFiles}}});
  return *tags;
}

}  // namespace

FilesSection::FilesSection(Profile* profile,
                           SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetFilesSearchConcepts());
  if (ash::features::IsUploadOfficeToCloudEnabled()) {
    updater.AddSearchTags(GetFilesOfficeSearchConcepts());
  }
}

FilesSection::~FilesSection() = default;

void FilesSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"disconnectGoogleDriveAccount", IDS_SETTINGS_DISCONNECT_GOOGLE_DRIVE},
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
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  smb_dialog::AddLocalizedStrings(html_source);

  html_source->AddString("smbSharesLearnMoreURL",
                         GetHelpUrlWithBoard(chrome::kSmbSharesLearnMoreURL));

  html_source->AddBoolean("showOfficeSettings",
                          ash::features::IsUploadOfficeToCloudEnabled());

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile());
  html_source->AddBoolean("isActiveDirectoryUser",
                          user && user->IsActiveDirectoryUser());
}

void FilesSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<smb_dialog::SmbHandler>(profile(), base::DoNothing()));
}

int FilesSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_FILES;
}

mojom::Section FilesSection::GetSection() const {
  return mojom::Section::kFiles;
}

mojom::SearchResultIcon FilesSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kFolder;
}

std::string FilesSection::GetSectionPath() const {
  return mojom::kFilesSectionPath;
}

bool FilesSection::LogMetric(mojom::Setting setting, base::Value& value) const {
  // Unimplemented.
  return false;
}

void FilesSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kGoogleDriveConnection);

  // Network file shares.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_DOWNLOADS_SMB_SHARES, mojom::Subpage::kNetworkFileShares,
      mojom::SearchResultIcon::kFolder, mojom::SearchResultDefaultRank::kMedium,
      mojom::kNetworkFileSharesSubpagePath);

  // Office.
  // TODO(b:264314789): Correct string (not smb).
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_DOWNLOADS_SMB_SHARES, mojom::Subpage::kOfficeFiles,
      mojom::SearchResultIcon::kFolder, mojom::SearchResultDefaultRank::kMedium,
      mojom::kNetworkFileSharesSubpagePath);
}

}  // namespace ash::settings
