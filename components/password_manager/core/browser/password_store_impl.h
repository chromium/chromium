// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {

// Simple password store implementation that delegates everything to
// the LoginDatabase.
class PasswordStoreImpl : public PasswordStore {
 public:
  // The |login_db| must not have been Init()-ed yet. It will be initialized in
  // a deferred manner on the background sequence.
  explicit PasswordStoreImpl(std::unique_ptr<LoginDatabase> login_db);

  void ShutdownOnUIThread() override;

  // To be used only for testing or in subclasses.
  LoginDatabase* login_db() const { return login_db_.get(); }

 protected:
  ~PasswordStoreImpl() override;

  // Opens |login_db_| on the background sequence.
  bool InitOnBackgroundSequence(
      bool upload_phished_credentials_to_sync) override;

  // Implements PasswordStore interface.
  void ReportMetricsImpl(const std::string& sync_username,
                         bool custom_passphrase_sync_enabled,
                         BulkCheckDone bulk_check_done) override;
  PasswordStoreChangeList AddLoginImpl(const PasswordForm& form,
                                       AddLoginError* error) override;
  PasswordStoreChangeList UpdateLoginImpl(const PasswordForm& form,
                                          UpdateLoginError* error) override;
  PasswordStoreChangeList RemoveLoginImpl(const PasswordForm& form) override;
  PasswordStoreChangeList RemoveLoginsByURLAndTimeImpl(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end) override;
  PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) override;
  PasswordStoreChangeList DisableAutoSignInForOriginsImpl(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter) override;
  bool RemoveStatisticsByOriginAndTimeImpl(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end) override;
  std::vector<std::unique_ptr<PasswordForm>> FillMatchingLogins(
      const FormDigest& form) override;
  std::vector<std::unique_ptr<PasswordForm>> FillMatchingLoginsByPassword(
      const std::u16string& plain_text_password) override;
  bool FillAutofillableLogins(
      std::vector<std::unique_ptr<PasswordForm>>* forms) override;
  bool FillBlocklistLogins(
      std::vector<std::unique_ptr<PasswordForm>>* forms) override;
  DatabaseCleanupResult DeleteUndecryptableLogins() override;
  void AddSiteStatsImpl(const InteractionsStats& stats) override;
  void RemoveSiteStatsImpl(const GURL& origin_domain) override;
  std::vector<InteractionsStats> GetAllSiteStatsImpl() override;
  std::vector<InteractionsStats> GetSiteStatsImpl(
      const GURL& origin_domain) override;
  PasswordStoreChangeList AddInsecureCredentialImpl(
      const InsecureCredential& insecure_credential) override;
  PasswordStoreChangeList RemoveInsecureCredentialsImpl(
      const std::string& signon_realm,
      const std::u16string& username,
      RemoveInsecureCredentialsReason reason) override;
  std::vector<InsecureCredential> GetAllInsecureCredentialsImpl() override;
  std::vector<InsecureCredential> GetMatchingInsecureCredentialsImpl(
      const std::string& signon_realm) override;

  void AddFieldInfoImpl(const FieldInfo& field_info) override;
  std::vector<FieldInfo> GetAllFieldInfoImpl() override;
  void RemoveFieldInfoByTimeImpl(base::Time remove_begin,
                                 base::Time remove_end) override;

  bool IsEmpty() override;

  // Implements PasswordStoreSync interface.
  bool BeginTransaction() override;
  void RollbackTransaction() override;
  bool CommitTransaction() override;
  FormRetrievalResult ReadAllLogins(
      PrimaryKeyToFormMap* key_to_form_map) override;
  std::vector<InsecureCredential> ReadSecurityIssues(
      FormPrimaryKey parent_key) override;
  PasswordStoreChangeList RemoveLoginByPrimaryKeySync(
      FormPrimaryKey primary_key) override;
  PasswordStoreSync::MetadataStore* GetMetadataStore() override;
  bool IsAccountStore() const override;
  bool DeleteAndRecreateDatabaseFile() override;

 private:
  // Resets |login_db_| on the background sequence.
  void ResetLoginDB();

  // The login SQL database. The LoginDatabase instance is received via the
  // in an uninitialized state, so as to allow injecting mocks, then Init() is
  // called on the background sequence in a deferred manner. If opening the DB
  // fails, |login_db_| will be reset and stay NULL for the lifetime of |this|.
  std::unique_ptr<LoginDatabase> login_db_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreImpl);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_IMPL_H_
