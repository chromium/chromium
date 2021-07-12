// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_

#include <memory>
#include <string>
#include <vector>

#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordStore : public PasswordStore {
 public:
  MockPasswordStore();

  // PasswordStoreInterface

  MOCK_METHOD(void, GetAutofillableLogins, (PasswordStoreConsumer*), (override));

  MOCK_METHOD(void, RemoveLogin, (const PasswordForm&), (override));
  MOCK_METHOD(void,
              Unblocklist,
              (const PasswordFormDigest&, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              GetLogins,
              (const PasswordFormDigest&, PasswordStoreConsumer*),
              (override));
  MOCK_METHOD(void, AddLogin, (const PasswordForm&), (override));
  MOCK_METHOD(void, UpdateLogin, (const PasswordForm&), (override));
  MOCK_METHOD(void,
              UpdateLoginWithPrimaryKey,
              (const PasswordForm&, const PasswordForm&),
              (override));
  MOCK_METHOD(void,
              ReportMetrics,
              (const std::string&, bool, bool),
              (override));
  MOCK_METHOD(void,
              ReportMetricsImpl,
              (const std::string&, bool, BulkCheckDone),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              AddLoginImpl,
              (const PasswordForm&, AddLoginError* error),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              UpdateLoginImpl,
              (const PasswordForm&, UpdateLoginError* error),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              RemoveLoginImpl,
              (const PasswordForm&),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              RemoveLoginsByURLAndTimeImpl,
              (const base::RepeatingCallback<bool(const GURL&)>&,
               base::Time,
               base::Time),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              RemoveLoginsCreatedBetweenImpl,
              (base::Time, base::Time),
              (override));
  MOCK_METHOD(bool,
              RemoveStatisticsByOriginAndTimeImpl,
              (const base::RepeatingCallback<bool(const GURL&)>&,
               base::Time,
               base::Time),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              DisableAutoSignInForOriginsImpl,
              (const base::RepeatingCallback<bool(const GURL&)>&),
              (override));
  std::vector<std::unique_ptr<PasswordForm>> FillMatchingLogins(
      const PasswordFormDigest& form) override {
    return std::vector<std::unique_ptr<PasswordForm>>();
  }
  MOCK_METHOD(std::vector<std::unique_ptr<PasswordForm>>,
              FillMatchingLoginsByPassword,
              (const std::u16string&),
              (override));
  MOCK_METHOD(DatabaseCleanupResult, DeleteUndecryptableLogins, (), (override));
  MOCK_METHOD(void,
              NotifyLoginsChanged,
              (const PasswordStoreChangeList&),
              (override));
  MOCK_METHOD(std::vector<InteractionsStats>,
              GetSiteStatsImpl,
              (const GURL& origin_domain),
              (override));
  MOCK_METHOD(void, AddSiteStatsImpl, (const InteractionsStats&));
  MOCK_METHOD(void, RemoveSiteStatsImpl, (const GURL&), (override));
  MOCK_METHOD(PasswordStoreChangeList,
              AddInsecureCredentialImpl,
              (const InsecureCredential&),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              RemoveInsecureCredentialsImpl,
              (const std::string&,
               const std::u16string&,
               RemoveInsecureCredentialsReason),
              (override));
  MOCK_METHOD(std::vector<InsecureCredential>,
              GetAllInsecureCredentialsImpl,
              (),
              (override));
  MOCK_METHOD(std::vector<InsecureCredential>,
              GetMatchingInsecureCredentialsImpl,
              (const std::string&),
              (override));
  MOCK_METHOD(void, AddFieldInfoImpl, (const FieldInfo&), (override));
  MOCK_METHOD(std::vector<FieldInfo>, GetAllFieldInfoImpl, (), (override));
  MOCK_METHOD(void,
              RemoveFieldInfoByTimeImpl,
              (base::Time, base::Time),
              (override));
  MOCK_METHOD(bool, IsEmpty, (), (override));
  MOCK_METHOD(base::WeakPtr<syncer::ModelTypeControllerDelegate>,
              GetSyncControllerDelegateOnBackgroundSequence,
              (),
              (override));
  MOCK_METHOD(void,
              GetAllLoginsWithAffiliationAndBrandingInformation,
              (PasswordStoreConsumer*),
              (override));

  MOCK_METHOD(bool, IsAbleToSavePasswords, (), (override, const));

  // TODO(crbug.bom/1226042): Remove this after PasswordStore no longer
  // inherits PasswordStoreSync.
  MOCK_METHOD(PasswordStoreChangeList,
              AddLoginSync,
              (const PasswordForm&, AddLoginError*),
              (override));
  MOCK_METHOD(bool,
              AddInsecureCredentialsSync,
              (base::span<const InsecureCredential>),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              UpdateLoginSync,
              (const PasswordForm&, UpdateLoginError*),
              (override));
  MOCK_METHOD(bool,
              UpdateInsecureCredentialsSync,
              (const PasswordForm&, base::span<const InsecureCredential>),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              RemoveLoginSync,
              (const PasswordForm& form),
              (override));
  MOCK_METHOD(bool, BeginTransaction, ());
  MOCK_METHOD(void, RollbackTransaction, (), (override));
  MOCK_METHOD(bool, CommitTransaction, (), (override));
  MOCK_METHOD(FormRetrievalResult,
              ReadAllLogins,
              (PrimaryKeyToFormMap*),
              (override));
  MOCK_METHOD(std::vector<InsecureCredential>,
              ReadSecurityIssues,
              (FormPrimaryKey),
              (override));
  MOCK_METHOD(PasswordStoreChangeList,
              RemoveLoginByPrimaryKeySync,
              (FormPrimaryKey),
              (override));
  MOCK_METHOD(PasswordStoreSync::MetadataStore*,
              GetMetadataStore,
              (),
              (override));
  MOCK_METHOD(bool, IsAccountStore, (), (override, const));
  MOCK_METHOD(bool, DeleteAndRecreateDatabaseFile, (), (override));

  PasswordStoreSync* GetSyncInterface() { return this; }

 protected:
  ~MockPasswordStore() override;

 private:
  // PasswordStore:
  scoped_refptr<base::SequencedTaskRunner> CreateBackgroundTaskRunner()
      const override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_
