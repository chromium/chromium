// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCOUNT_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCOUNT_MANAGER_HANDLER_H_

#include <string>
#include <vector>

#include "ash/components/account_manager/account_manager.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace chromeos {
namespace settings {

class AccountManagerUIHandler
    : public ::settings::SettingsPageUIHandler,
      public account_manager::AccountManagerFacade::Observer,
      public signin::IdentityManager::Observer {
 public:
  // Accepts non-owning pointers to |AccountManager|, |AccountManagerFacade|
  // and |IdentityManager|. Both of these must outlive |this| instance.
  AccountManagerUIHandler(
      AccountManager* account_manager,
      account_manager::AccountManagerFacade* account_manager_facade,
      signin::IdentityManager* identity_manager);
  ~AccountManagerUIHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // |AccountManagerFacade::Observer| overrides.
  // |AccountManager| is considered to be the source of truth for account
  // information.
  void OnAccountUpserted(const ::account_manager::Account& account) override;
  void OnAccountRemoved(const ::account_manager::Account& account) override;

  // |signin::IdentityManager::Observer| overrides.
  void OnRefreshTokenUpdatedForAccount(const CoreAccountInfo& info) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error) override;

 private:
  friend class AccountManagerUIHandlerTest;

  void SetProfileForTesting(Profile* profile);

  // WebUI "getAccounts" message callback.
  void HandleGetAccounts(const base::ListValue* args);

  // WebUI "addAccount" message callback.
  void HandleAddAccount(const base::ListValue* args);

  // WebUI "reauthenticateAccount" message callback.
  void HandleReauthenticateAccount(const base::ListValue* args);

  // WebUI "migrateAccount" message callback.
  void HandleMigrateAccount(const base::ListValue* args);

  // WebUI "removeAccount" message callback.
  void HandleRemoveAccount(const base::ListValue* args);

  // WebUI "showWelcomeDialogIfRequired" message callback.
  void HandleShowWelcomeDialogIfRequired(const base::ListValue* args);

  // |AccountManager::CheckDummyGaiaTokenForAllAccounts| callback.
  void OnCheckDummyGaiaTokenForAllAccounts(
      base::Value callback_id,
      const std::vector<std::pair<::account_manager::Account, bool>>&
          account_dummy_token_list);

  // Returns secondary Gaia accounts from |stored_accounts| list. If the Device
  // Account is a Gaia account, populates |device_account| with information
  // about that account, otherwise does not modify |device_account|.
  // If user (device account) is child - |is_child_user| should be set to true,
  // in this case "unmigrated" property will be always false for secondary
  // accounts.
  base::ListValue GetSecondaryGaiaAccounts(
      const std::vector<std::pair<::account_manager::Account, bool>>&
          account_dummy_token_list,
      const AccountId device_account_id,
      const bool is_child_user,
      base::DictionaryValue* device_account);

  // Refreshes the UI.
  void RefreshUI();

  Profile* profile_ = nullptr;

  // A non-owning pointer to |AccountManager|.
  AccountManager* const account_manager_;

  // A non-owning pointer to |AccountManagerFacade|.
  account_manager::AccountManagerFacade* const account_manager_facade_;

  // A non-owning pointer to |IdentityManager|.
  signin::IdentityManager* const identity_manager_;

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

  base::WeakPtrFactory<AccountManagerUIHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AccountManagerUIHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ACCOUNT_MANAGER_HANDLER_H_
