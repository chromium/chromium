// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_MOCK_ACCOUNT_MANAGER_FACADE_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_MOCK_ACCOUNT_MANAGER_FACADE_H_

#include "components/account_manager_core/account_manager_facade.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace account_manager {

class MockAccountManagerFacadeObserver
    : public account_manager::AccountManagerFacade::Observer {
 public:
  MockAccountManagerFacadeObserver();
  ~MockAccountManagerFacadeObserver() override;

  MOCK_METHOD(void,
              OnAccountUpserted,
              (const account_manager::Account&),
              (override));
  MOCK_METHOD(void,
              OnAccountRemoved,
              (const account_manager::Account&),
              (override));
  MOCK_METHOD(void,
              OnAuthErrorChanged,
              (const account_manager::AccountKey&,
               const GoogleServiceAuthError&),
              (override));
  MOCK_METHOD(void, OnSigninDialogClosed, (), (override));
};

class MockAccountManagerFacade : public account_manager::AccountManagerFacade {
 public:
  MockAccountManagerFacade();
  ~MockAccountManagerFacade() override;

  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(void,
              GetAccounts,
              (base::OnceCallback<void(const std::vector<Account>&)>),
              (override));
  MOCK_METHOD(void,
              GetPersistentErrorForAccount,
              (const AccountKey&,
               base::OnceCallback<void(const GoogleServiceAuthError&)>),
              (override));
  MOCK_METHOD(void, ShowAddAccountDialog, (AccountAdditionSource), (override));
  MOCK_METHOD(void,
              ShowAddAccountDialog,
              (AccountAdditionSource,
               base::OnceCallback<void(const AccountUpsertionResult& result)>),
              (override));
  MOCK_METHOD(void,
              ShowReauthAccountDialog,
              (AccountAdditionSource,
               const std::string&,
               base::OnceCallback<void(const AccountUpsertionResult& result)>),
              (override));
  MOCK_METHOD(void, ShowManageAccountsSettings, (), (override));
  MOCK_METHOD(void,
              ReportAuthError,
              (const AccountKey&, const GoogleServiceAuthError&),
              (override));
  MOCK_METHOD(std::unique_ptr<OAuth2AccessTokenFetcher>,
              CreateAccessTokenFetcher,
              (const AccountKey&, OAuth2AccessTokenConsumer*),
              (override));
  MOCK_METHOD(void,
              UpsertAccountForTesting,
              (const Account&, const std::string&),
              (override));
  MOCK_METHOD(void, RemoveAccountForTesting, (const AccountKey&), (override));
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_MOCK_ACCOUNT_MANAGER_FACADE_H_
