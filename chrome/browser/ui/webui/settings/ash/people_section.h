// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PEOPLE_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PEOPLE_SECTION_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_section.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/sync_service_observer.h"

class PrefService;
class Profile;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace ash {

class AccountAppsAvailability;

namespace settings {

class SearchTagRegistry;

// Provides UI strings and search tags for People settings. Search tags are only
// added for non-guest sessions.
//
// Fingerprint and Parental Controls search tags are only shown if they are
// allowed by policy/flags. Different sets of Sync tags are shown depending on
// whether the feature is enabed or disabled.
class PeopleSection : public OsSettingsSection,
                      public account_manager::AccountManagerFacade::Observer {
 public:
  PeopleSection(Profile* profile,
                SearchTagRegistry* search_tag_registry,
                syncer::SyncService* sync_service,
                signin::IdentityManager* identity_manager,
                PrefService* pref_service);
  ~PeopleSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;
  int GetSectionNameMessageId() const override;
  chromeos::settings::mojom::Section GetSection() const override;
  mojom::SearchResultIcon GetSectionIcon() const override;
  std::string GetSectionPath() const override;
  bool LogMetric(chromeos::settings::mojom::Setting setting,
                 base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;

  // AccountManagerFacade::Observer:
  void OnAccountUpserted(const ::account_manager::Account& account) override;
  void OnAccountRemoved(const ::account_manager::Account& account) override;
  void OnAuthErrorChanged(const account_manager::AccountKey& account,
                          const GoogleServiceAuthError& error) override;

  bool AreFingerprintSettingsAllowed();
  void FetchAccounts();
  void UpdateAccountManagerSearchTags(
      const std::vector<::account_manager::Account>& accounts);

  account_manager::AccountManager* account_manager_ = nullptr;
  account_manager::AccountManagerFacade* account_manager_facade_ = nullptr;
  AccountAppsAvailability* account_apps_availability_ = nullptr;
  signin::IdentityManager* identity_manager_;
  PrefService* pref_service_;

  // An observer for `AccountManagerFacade`. Automatically deregisters when
  // `this` is destructed.
  base::ScopedObservation<account_manager::AccountManagerFacade,
                          account_manager::AccountManagerFacade::Observer>
      account_manager_facade_observation_{this};

  base::WeakPtrFactory<PeopleSection> weak_factory_{this};
};

}  // namespace settings
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PEOPLE_SECTION_H_
