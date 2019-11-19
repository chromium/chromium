// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_

#include <memory>
#include <string>
#include <vector>

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordStore : public PasswordStore {
 public:
  MockPasswordStore();

  MOCK_METHOD1(RemoveLogin, void(const autofill::PasswordForm&));
  MOCK_METHOD2(GetLogins,
               void(const PasswordStore::FormDigest&, PasswordStoreConsumer*));
  MOCK_METHOD2(GetLoginsByPassword,
               void(const base::string16&, PasswordStoreConsumer*));
  MOCK_METHOD1(AddLogin, void(const autofill::PasswordForm&));
  MOCK_METHOD1(UpdateLogin, void(const autofill::PasswordForm&));
  MOCK_METHOD2(UpdateLoginWithPrimaryKey,
               void(const autofill::PasswordForm&,
                    const autofill::PasswordForm&));
  MOCK_METHOD3(ReportMetrics, void(const std::string&, bool, bool));
  MOCK_METHOD2(ReportMetricsImpl, void(const std::string&, bool));
  MOCK_METHOD2(AddLoginImpl,
               PasswordStoreChangeList(const autofill::PasswordForm&,
                                       AddLoginError* error));
  MOCK_METHOD2(UpdateLoginImpl,
               PasswordStoreChangeList(const autofill::PasswordForm&,
                                       UpdateLoginError* error));
  MOCK_METHOD1(RemoveLoginImpl,
               PasswordStoreChangeList(const autofill::PasswordForm&));
  MOCK_METHOD3(RemoveLoginsByURLAndTimeImpl,
               PasswordStoreChangeList(const base::Callback<bool(const GURL&)>&,
                                       base::Time,
                                       base::Time));
  MOCK_METHOD2(RemoveLoginsCreatedBetweenImpl,
               PasswordStoreChangeList(base::Time, base::Time));
  MOCK_METHOD3(RemoveStatisticsByOriginAndTimeImpl,
               bool(const base::Callback<bool(const GURL&)>&,
                    base::Time,
                    base::Time));
  MOCK_METHOD1(
      DisableAutoSignInForOriginsImpl,
      PasswordStoreChangeList(const base::Callback<bool(const GURL&)>&));
  std::vector<std::unique_ptr<autofill::PasswordForm>> FillMatchingLogins(
      const PasswordStore::FormDigest& form) override {
    return std::vector<std::unique_ptr<autofill::PasswordForm>>();
  }
  MOCK_METHOD1(FillMatchingLoginsByPassword,
               std::vector<std::unique_ptr<autofill::PasswordForm>>(
                   const base::string16&));
  MOCK_METHOD1(FillAutofillableLogins,
               bool(std::vector<std::unique_ptr<autofill::PasswordForm>>*));
  MOCK_METHOD1(FillBlacklistLogins,
               bool(std::vector<std::unique_ptr<autofill::PasswordForm>>*));
  MOCK_METHOD0(DeleteUndecryptableLogins, DatabaseCleanupResult());
  MOCK_METHOD1(NotifyLoginsChanged, void(const PasswordStoreChangeList&));
  MOCK_METHOD0(GetAllSiteStatsImpl, std::vector<InteractionsStats>());
  MOCK_METHOD1(GetSiteStatsImpl,
               std::vector<InteractionsStats>(const GURL& origin_domain));
  MOCK_METHOD1(AddSiteStatsImpl, void(const InteractionsStats&));
  MOCK_METHOD1(RemoveSiteStatsImpl, void(const GURL&));
  MOCK_METHOD1(AddCompromisedCredentialsImpl,
               void(const CompromisedCredentials&));
  MOCK_METHOD2(RemoveCompromisedCredentialsImpl,
               void(const GURL&, const base::string16&));
  MOCK_METHOD0(GetAllCompromisedCredentialsImpl,
               std::vector<CompromisedCredentials>());
  MOCK_METHOD3(RemoveCompromisedCredentialsByUrlAndTimeImpl,
               void(const base::RepeatingCallback<bool(const GURL&)>&,
                    base::Time,
                    base::Time));
  MOCK_METHOD1(AddFieldInfoImpl, void(const FieldInfo&));
  MOCK_METHOD0(GetAllFieldInfoImpl, std::vector<FieldInfo>());
  MOCK_METHOD2(RemoveFieldInfoByTimeImpl, void(base::Time, base::Time));

  MOCK_CONST_METHOD0(IsAbleToSavePasswords, bool());

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  MOCK_METHOD3(CheckReuse,
               void(const base::string16&,
                    const std::string&,
                    PasswordReuseDetectorConsumer*));
  MOCK_METHOD3(SaveGaiaPasswordHash,
               void(const std::string&,
                    const base::string16&,
                    metrics_util::GaiaPasswordHashChange));
  MOCK_METHOD2(SaveEnterprisePasswordHash,
               void(const std::string&, const base::string16&));
  MOCK_METHOD1(ClearGaiaPasswordHash, void(const std::string&));
  MOCK_METHOD0(ClearAllGaiaPasswordHash, void());
  MOCK_METHOD0(ClearAllEnterprisePasswordHash, void());
#endif
  MOCK_METHOD0(BeginTransaction, bool());
  MOCK_METHOD0(RollbackTransaction, void());
  MOCK_METHOD0(CommitTransaction, bool());
  MOCK_METHOD1(ReadAllLogins, FormRetrievalResult(PrimaryKeyToFormMap*));
  MOCK_METHOD1(RemoveLoginByPrimaryKeySync, PasswordStoreChangeList(int));
  MOCK_METHOD0(GetMetadataStore, PasswordStoreSync::MetadataStore*());
  MOCK_CONST_METHOD0(IsAccountStore, bool());
  MOCK_METHOD0(DeleteAndRecreateDatabaseFile, bool());

  PasswordStoreSync* GetSyncInterface() { return this; }

 protected:
  ~MockPasswordStore() override;

 private:
  // PasswordStore:
  scoped_refptr<base::SequencedTaskRunner> CreateBackgroundTaskRunner()
      const override;
  bool InitOnBackgroundSequence(
      const syncer::SyncableService::StartSyncFlare& flare) override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_
