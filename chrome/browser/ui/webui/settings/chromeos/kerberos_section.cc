// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/kerberos_section.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/chromeos/kerberos_accounts_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetKerberosSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(fsandrade): add Kerberos search tags here.
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
  // No search tags are registered if KerberosSettingsSection flag is disabled.
  if (!chromeos::features::IsKerberosSettingsSectionEnabled())
    return;

  if (kerberos_credentials_manager_) {
    // Kerberos search tags are added/removed dynamically.
    kerberos_credentials_manager_->AddObserver(this);
    OnKerberosEnabledStateChanged();
  }
}

KerberosSection::~KerberosSection() {
  // No observer has been added if KerberosSettingsSection flag is disabled.
  if (!chromeos::features::IsKerberosSettingsSectionEnabled())
    return;

  if (kerberos_credentials_manager_)
    kerberos_credentials_manager_->RemoveObserver(this);
}

void KerberosSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  // TODO(fsandrade): add Kerberos section title to corresponding UI element,
  // when it's created.

  KerberosAccountsHandler::AddLoadTimeKerberosStrings(
      html_source, kerberos_credentials_manager_);
}

void KerberosSection::AddHandlers(content::WebUI* web_ui) {
  // No handler is created/added if KerberosSettingsSection flag is disabled.
  if (!chromeos::features::IsKerberosSettingsSectionEnabled())
    return;

  std::unique_ptr<chromeos::settings::KerberosAccountsHandler>
      kerberos_accounts_handler =
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

std::string KerberosSection::GetSectionPath() const {
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

void KerberosSection::OnKerberosEnabledStateChanged() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (kerberos_credentials_manager_->IsKerberosEnabled())
    updater.AddSearchTags(GetKerberosSearchConcepts());
  else
    updater.RemoveSearchTags(GetKerberosSearchConcepts());
}

}  // namespace settings
}  // namespace chromeos
