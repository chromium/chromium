// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_ACCOUNT_MANAGER_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_ACCOUNT_MANAGER_UI_HANDLER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace ash::settings {

class AccountManagerUIHandler
    : public ::settings::SettingsPageUIHandler,
      public account_manager::AccountManagerFacade::Observer,
      public signin::IdentityManager::Observer,
      public AccountAppsAvailability::Observer {
 public:
  // Accepts non-owning pointers to |account_manager::AccountManager|,
  // |AccountManagerFacade| and |IdentityManager|. Both of these must outlive
  // |this| instance.
  AccountManagerUIHandler(
      account_manager::AccountManager* account_manager,
      account_manager::AccountManagerFacade* account_manager_facade,
      signin::IdentityManager* identity_manager,
      AccountAppsAvailability* account_apps_availability);

  AccountManagerUIHandler(const AccountManagerUIHandler&) = delete;
  AccountManagerUIHandler& operator=(const AccountManagerUIHandler&) = delete;

  ~AccountManagerUIHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // |AccountManagerFacade::Observer| overrides.
  // |account_manager::AccountManager| is considered to be the source of truth
  // for account information.
  void OnAccountUpserted(const ::account_manager::Account& account) override;
  void OnAccountRemoved(const ::account_manager::Account& account) override;
  void OnAuthErrorChanged(const account_manager::AccountKey& account,
                          const GoogleServiceAuthError& error) override;

  // |signin::IdentityManager::Observer| overrides.
  void OnRefreshTokenUpdatedForAccount(const CoreAccountInfo& info) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;

  // |AccountAppsAvailability::Observer| overrides.
  void OnAccountAvailableInArc(
      const ::account_manager::Account& account) override;
  void OnAccountUnavailableInArc(
      const ::account_manager::Account& account) override;

 private:
  friend class AccountManagerUIHandlerTest;
  friend class AccountManagerUIHandlerTestWithArcAccountRestrictions;
  friend class AccountManagerUIHandlerTestWithManagedArcAccountRestriction;

  void SetProfileForTesting(Profile* profile);

  // WebUI "getAccounts" message callback.
  void HandleGetAccounts(const base::Value::List& args);

  // WebUI "addAccount" message callback.
  void HandleAddAccount(const base::Value::List& args);

  // WebUI "reauthenticateAccount" message callback.
  void HandleReauthenticateAccount(const base::Value::List& args);

  // WebUI "migrateAccount" message callback.
  void HandleMigrateAccount(const base::Value::List& args);

  // WebUI "removeAccount" message callback.
  void HandleRemoveAccount(const base::Value::List& args);

  // WebUI "changeArcAvailability" message callback.
  void HandleChangeArcAvailability(const base::Value::List& args);

  // |account_manager::AccountManager::CheckDummyGaiaTokenForAllAccounts|
  // callback.
  void OnCheckDummyGaiaTokenForAllAccounts(
      base::Value callback_id,
      const std::vector<std::pair<::account_manager::Account, bool>>&
          account_dummy_token_list);

  void FinishHandleGetAccounts(
      base::Value callback_id,
      const std::vector<std::pair<::account_manager::Account, bool>>&
          account_dummy_token_list,
      const base::flat_set<account_manager::Account>& arc_accounts);

  // Returns secondary Gaia accounts from |stored_accounts| list. If the Device
  // Account is a Gaia account, populates |device_account| with information
  // about that account, otherwise does not modify |device_account|.
  // If user (device account) is child - |is_child_user| should be set to true,
  // in this case "unmigrated" property will be always false for secondary
  // accounts.
  base::Value::List GetSecondaryGaiaAccounts(
      const std::vector<std::pair<::account_manager::Account, bool>>&
          account_dummy_token_list,
      const base::flat_set<account_manager::Account>& arc_accounts,
      const AccountId device_account_id,
      const bool is_child_user,
      base::Value::Dict* device_account);

  // Refreshes the UI.
  void RefreshUI();

  // Whether some accounts may not be available in ARC due to policy or config.
  static bool AreArcAccountsRestricted();

  raw_ptr<Profile> profile_ = nullptr;

  // A non-owning pointer to |account_manager::AccountManager|.
  const raw_ptr<account_manager::AccountManager> account_manager_;

  // A non-owning pointer to |AccountManagerFacade|.
  const raw_ptr<account_manager::AccountManagerFacade> account_manager_facade_;

  // A non-owning pointer to |IdentityManager|.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // A non-owning pointer to |AccountAppsAvailability| which is a KeyedService
  // and should outlive this class.
  raw_ptr<AccountAppsAvailability> account_apps_availability_ = nullptr;

  // An observer for |AccountManagerFacade|. Automatically deregisters when
  // |this| is destructed.
  base::ScopedObservation<account_manager::AccountManagerFacade,
                          account_manager::AccountManagerFacade::Observer>
      account_manager_facade_observation_{this};

  // An observer for |signin::IdentityManager|. Automatically deregisters when
  // |this| is destructed.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // An observer for |AccountAppsAvailability|. Registered on
  // |OnJavascriptAllowed| and deregistered on |OnJavascriptDisallowed|.
  base::ScopedObservation<AccountAppsAvailability,
                          AccountAppsAvailability::Observer>
      account_apps_availability_observation_{this};

  base::WeakPtrFactory<AccountManagerUIHandler> weak_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_ACCOUNT_MANAGER_UI_HANDLER_H_
