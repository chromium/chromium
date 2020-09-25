// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_TEST_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_TEST_PASSWORD_STORE_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/password_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

// A matcher that compares two PasswordForm instances but ignores the |in_store|
// member.
MATCHER_P(MatchesFormExceptStore, expected, "") {
  PasswordForm arg_copy = arg;
  arg_copy.in_store = expected.in_store;
  return arg_copy == expected;
}

// A very simple PasswordStore implementation that keeps all of the passwords
// in memory and does all its manipulations on the main thread. Since this
// is only used for testing, only the parts of the interface that are needed
// for testing have been implemented.
class TestPasswordStore : public PasswordStore {
 public:
  // We need to qualify password_manager::IsAccountStore with the full
  // namespace, otherwise, it's confused with the method
  // PasswordStoreSync::IsAccountStore().
  explicit TestPasswordStore(password_manager::IsAccountStore is_account_store =
                                 password_manager::IsAccountStore(false));

  using PasswordMap = std::map<std::string /* signon_realm */,
                               std::vector<PasswordForm>,
                               std::less<>>;

  struct CompromisedCredentialsLess {
    bool operator()(const CompromisedCredentials& lhs,
                    const CompromisedCredentials& rhs) const {
      // Only compare members that are part of the unique key in the database.
      return std::tie(lhs.signon_realm, lhs.username, lhs.compromise_type) <
             std::tie(rhs.signon_realm, rhs.username, rhs.compromise_type);
    }
  };

  using CompromisedCredentialsStorage =
      base::flat_set<CompromisedCredentials, CompromisedCredentialsLess>;

  const PasswordMap& stored_passwords() const;

  const CompromisedCredentialsStorage& compromised_credentials() const {
    return compromised_credentials_;
  }

  void Clear();

  // Returns true if no passwords are stored in the store. Note that this is not
  // as simple as asking whether stored_passwords().empty(), because the map can
  // have entries of size 0.
  bool IsEmpty() override;

  int fill_matching_logins_calls() const { return fill_matching_logins_calls_; }

 protected:
  ~TestPasswordStore() override;

  scoped_refptr<base::SequencedTaskRunner> CreateBackgroundTaskRunner()
      const override;

  // PasswordStore interface
  PasswordStoreChangeList AddLoginImpl(const PasswordForm& form,
                                       AddLoginError* error) override;
  PasswordStoreChangeList UpdateLoginImpl(const PasswordForm& form,
                                          UpdateLoginError* error) override;
  PasswordStoreChangeList RemoveLoginImpl(const PasswordForm& form) override;
  std::vector<std::unique_ptr<PasswordForm>> FillMatchingLogins(
      const FormDigest& form) override;
  std::vector<std::unique_ptr<PasswordForm>> FillMatchingLoginsByPassword(
      const base::string16& plain_text_password) override;
  bool FillAutofillableLogins(
      std::vector<std::unique_ptr<PasswordForm>>* forms) override;
  bool FillBlacklistLogins(
      std::vector<std::unique_ptr<PasswordForm>>* forms) override;
  DatabaseCleanupResult DeleteUndecryptableLogins() override;
  std::vector<InteractionsStats> GetSiteStatsImpl(
      const GURL& origin_domain) override;

  // Unused portions of PasswordStore interface
  void ReportMetricsImpl(const std::string& sync_username,
                         bool custom_passphrase_sync_enabled,
                         BulkCheckDone bulk_check_done) override;
  PasswordStoreChangeList RemoveLoginsByURLAndTimeImpl(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time begin,
      base::Time end) override;
  PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time begin,
      base::Time end) override;
  PasswordStoreChangeList DisableAutoSignInForOriginsImpl(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter) override;
  bool RemoveStatisticsByOriginAndTimeImpl(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end) override;
  void AddSiteStatsImpl(const InteractionsStats& stats) override;
  void RemoveSiteStatsImpl(const GURL& origin_domain) override;
  std::vector<InteractionsStats> GetAllSiteStatsImpl() override;
  bool AddCompromisedCredentialsImpl(
      const CompromisedCredentials& compromised_credentials) override;
  bool RemoveCompromisedCredentialsByCompromiseTypeImpl(
      const std::string& signon_realm,
      const base::string16& username,
      const CompromiseType& compromise_type,
      RemoveCompromisedCredentialsReason reason) override;
  bool RemoveCompromisedCredentialsImpl(
      const std::string& signon_realm,
      const base::string16& username,
      RemoveCompromisedCredentialsReason reason) override;
  std::vector<CompromisedCredentials> GetAllCompromisedCredentialsImpl()
      override;
  std::vector<CompromisedCredentials> GetMatchingCompromisedCredentialsImpl(
      const std::string& signon_realm) override;
  bool RemoveCompromisedCredentialsByUrlAndTimeImpl(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time remove_begin,
      base::Time remove_end) override;
  void AddFieldInfoImpl(const FieldInfo& field_info) override;
  std::vector<FieldInfo> GetAllFieldInfoImpl() override;
  void RemoveFieldInfoByTimeImpl(base::Time remove_begin,
                                 base::Time remove_end) override;

  // PasswordStoreSync interface.
  bool BeginTransaction() override;
  void RollbackTransaction() override;
  bool CommitTransaction() override;
  FormRetrievalResult ReadAllLogins(
      PrimaryKeyToFormMap* key_to_form_map) override;
  PasswordStoreChangeList RemoveLoginByPrimaryKeySync(int primary_key) override;
  PasswordStoreSync::MetadataStore* GetMetadataStore() override;
  bool IsAccountStore() const override;
  bool DeleteAndRecreateDatabaseFile() override;

 private:
  const password_manager::IsAccountStore is_account_store_;

  PasswordMap stored_passwords_;
  CompromisedCredentialsStorage compromised_credentials_;

  const std::unique_ptr<PasswordStoreSync::MetadataStore> metadata_store_;

  // Number of calls of FillMatchingLogins() method.
  int fill_matching_logins_calls_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestPasswordStore);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_TEST_PASSWORD_STORE_H_
