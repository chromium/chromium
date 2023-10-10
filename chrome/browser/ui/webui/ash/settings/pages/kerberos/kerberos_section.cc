// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/kerberos/kerberos_section.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/pages/kerberos/kerberos_accounts_handler.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kKerberosAccountsV2SubpagePath;
using ::chromeos::settings::mojom::kKerberosSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

// Provides search tags that are always available when the feature is enabled by
// policy/flag.
const std::vector<SearchConcept>& GetFixedKerberosSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_KERBEROS_SECTION,
       mojom::kKerberosSectionPath,
       mojom::SearchResultIcon::kAuthKey,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kKerberos}},
      {IDS_OS_SETTINGS_TAG_KERBEROS,
       mojom::kKerberosAccountsV2SubpagePath,
       mojom::SearchResultIcon::kAuthKey,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kKerberosAccountsV2}},
      {IDS_OS_SETTINGS_TAG_KERBEROS_ADD,
       mojom::kKerberosAccountsV2SubpagePath,
       mojom::SearchResultIcon::kAuthKey,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddKerberosTicketV2}},
  });
  return *tags;
}

// Provides search tags that are only available when the feature is enabled by
// policy/flag and there is at least one Kerberos ticket.
const std::vector<SearchConcept>& GetDynamicKerberosSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_KERBEROS_REMOVE,
       mojom::kKerberosAccountsV2SubpagePath,
       mojom::SearchResultIcon::kAuthKey,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kRemoveKerberosTicketV2}},
      {IDS_OS_SETTINGS_TAG_KERBEROS_ACTIVE,
       mojom::kKerberosAccountsV2SubpagePath,
       mojom::SearchResultIcon::kAuthKey,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSetActiveKerberosTicketV2}},
  });
  return *tags;
}

}  // namespace

KerberosSection::KerberosSection(
    Profile* profile,
    SearchTagRegistry* search_tag_registry,
    KerberosCredentialsManager* kerberos_credentials_manager)
    : OsSettingsSection(profile, search_tag_registry),
      kerberos_credentials_manager_(kerberos_credentials_manager) {
  if (kerberos_credentials_manager_) {
    // Kerberos search tags are added/removed dynamically.
    kerberos_credentials_manager_->AddObserver(this);
    UpdateKerberosSearchConcepts();
  }
}

KerberosSection::~KerberosSection() {
  if (kerberos_credentials_manager_) {
    kerberos_credentials_manager_->RemoveObserver(this);
  }
}

void KerberosSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("kerberosPageTitle",
                                  IDS_OS_SETTINGS_KERBEROS);

  KerberosAccountsHandler::AddLoadTimeKerberosStrings(
      html_source, kerberos_credentials_manager_);
}

void KerberosSection::AddHandlers(content::WebUI* web_ui) {
  std::unique_ptr<KerberosAccountsHandler> kerberos_accounts_handler =
      KerberosAccountsHandler::CreateIfKerberosEnabled(profile());
  if (kerberos_accounts_handler) {
    // Note that the UI is enabled only if Kerberos is enabled.
    web_ui->AddMessageHandler(std::move(kerberos_accounts_handler));
  }
}

int KerberosSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_KERBEROS;
}

mojom::Section KerberosSection::GetSection() const {
  return mojom::Section::kKerberos;
}

mojom::SearchResultIcon KerberosSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kAuthKey;
}

const char* KerberosSection::GetSectionPath() const {
  return mojom::kKerberosSectionPath;
}

bool KerberosSection::LogMetric(mojom::Setting setting,
                                base::Value& value) const {
  // Unimplemented.
  return false;
}

void KerberosSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_KERBEROS_ACCOUNTS_PAGE_TITLE,
                                     mojom::Subpage::kKerberosAccountsV2,
                                     mojom::SearchResultIcon::kAuthKey,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kKerberosAccountsV2SubpagePath);
  static constexpr mojom::Setting kKerberosAccountsV2Settings[] = {
      mojom::Setting::kAddKerberosTicketV2,
      mojom::Setting::kRemoveKerberosTicketV2,
      mojom::Setting::kSetActiveKerberosTicketV2,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kKerberosAccountsV2,
                            kKerberosAccountsV2Settings, generator);
}

void KerberosSection::OnAccountsChanged() {
  UpdateKerberosSearchConcepts();
}

void KerberosSection::OnKerberosEnabledStateChanged() {
  UpdateKerberosSearchConcepts();
}

// Updates search tags according to the KerberosEnabled state and the presence
// of Kerberos tickets in the system.
void KerberosSection::UpdateKerberosSearchConcepts() {
  CHECK(kerberos_credentials_manager_);

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  // Removes all search tags first. They will be added conditionally later.
  updater.RemoveSearchTags(GetFixedKerberosSearchConcepts());
  updater.RemoveSearchTags(GetDynamicKerberosSearchConcepts());

  if (kerberos_credentials_manager_->IsKerberosEnabled()) {
    updater.AddSearchTags(GetFixedKerberosSearchConcepts());

    const std::string account_name =
        kerberos_credentials_manager_->GetActiveAccount();
    if (!account_name.empty()) {
      updater.AddSearchTags(GetDynamicKerberosSearchConcepts());
    }
  }
}

}  // namespace ash::settings
