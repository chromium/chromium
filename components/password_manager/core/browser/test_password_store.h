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
#include "components/password_manager/core/browser/insecure_credentials_table.h"
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
// TODO(crbug.com/1222591): Implement only the PasswordStoreInterface.
class TestPasswordStore : public PasswordStore, public PasswordStoreBackend {
 public:
  // We need to qualify password_manager::IsAccountStore with the full
  // namespace, otherwise, it's confused with the method
  // PasswordStoreSync::IsAccountStore().
  explicit TestPasswordStore(password_manager::IsAccountStore is_account_store =
                                 password_manager::IsAccountStore(false));

  using PasswordMap = std::map<std::string /* signon_realm */,
                               std::vector<PasswordForm>,
                               std::less<>>;

  struct InsecureCredentialLess {
    bool operator()(const InsecureCredential& lhs,
                    const InsecureCredential& rhs) const {
      // Only compare members that are part of the unique key in the database.
      return std::tie(lhs.signon_realm, lhs.username, lhs.insecure_type) <
             std::tie(rhs.signon_realm, rhs.username, rhs.insecure_type);
    }
  };

  using InsecureCredentialsStorage =
      base::flat_set<InsecureCredential, InsecureCredentialLess>;

  const PasswordMap& stored_passwords() const;

  const InsecureCredentialsStorage& insecure_credentials() const {
    return insecure_credentials_;
  }

  void Clear();

  // Returns true if no passwords are stored in the store. Note that this is not
  // as simple as asking whether stored_passwords().empty(), because the map can
  // have entries of size 0.
  bool IsEmpty() override;

  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegateOnBackgroundSequence() override;

  int fill_matching_logins_calls() const { return fill_matching_logins_calls_; }

  bool IsAccountStore() const;

 protected:
  ~TestPasswordStore() override;

  scoped_refptr<base::SequencedTaskRunner> CreateBackgroundTaskRunner()
      const override;

  // PasswordStoreBackend interface
  void InitBackend(RemoteChangesReceived remote_form_changes_received,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void GetAllLoginsAsync(LoginsReply callback) override;
  void GetAutofillableLoginsAsync(LoginsReply callback) override;
  void FillMatchingLoginsAsync(
      LoginsReply callback,
      const std::vector<PasswordFormDigest>& forms) override;
  void AddLoginAsync(const PasswordForm& form,
                     PasswordStoreChangeListReply callback) override;
  void UpdateLoginAsync(const PasswordForm& form,
                        PasswordStoreChangeListReply callback) override;
  void RemoveLoginAsync(const PasswordForm& form,
                        PasswordStoreChangeListReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      base::Time delete_begin,
      base::Time delete_end,
      PasswordStoreChangeListReply callback) override;
  void RemoveLoginsByURLAndTimeAsync(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordStoreChangeListReply callback) override;

  // PasswordStore interface
  std::vector<std::unique_ptr<PasswordForm>> FillMatchingLogins(
      const PasswordFormDigest& form) override;
  std::vector<std::unique_ptr<PasswordForm>> FillMatchingLoginsByPassword(
      const std::u16string& plain_text_password) override;
  std::vector<InteractionsStats> GetSiteStatsImpl(
      const GURL& origin_domain) override;

  // Unused portions of PasswordStore interface
  void ReportMetricsImpl(const std::string& sync_username,
                         bool custom_passphrase_sync_enabled,
                         BulkCheckDone bulk_check_done) override;
  PasswordStoreChangeList DisableAutoSignInForOriginsImpl(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter) override;
  bool RemoveStatisticsByOriginAndTimeImpl(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end) override;
  void AddSiteStatsImpl(const InteractionsStats& stats) override;
  void RemoveSiteStatsImpl(const GURL& origin_domain) override;
  PasswordStoreChangeList AddInsecureCredentialImpl(
      const InsecureCredential& insecure_credentials) override;
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
  void SetUnsyncedCredentialsDeletionNotifier(
      std::unique_ptr<UnsyncedCredentialsDeletionNotifier> deletion_notifier)
      override;
 private:
  LoginsResult GetAllLoginsInternal();
  LoginsResult GetAutofillableLoginsInternal();
  LoginsResult FillMatchingLoginsBulk(
      const std::vector<PasswordFormDigest>& forms);
  PasswordStoreChangeList AddLoginImpl(const PasswordForm& form);
  PasswordStoreChangeList UpdateLoginImpl(const PasswordForm& form);
  PasswordStoreChangeList RemoveLoginImpl(const PasswordForm& form);

  const password_manager::IsAccountStore is_account_store_;

  PasswordMap stored_passwords_;
  InsecureCredentialsStorage insecure_credentials_;

  const std::unique_ptr<PasswordStoreSync::MetadataStore> metadata_store_;

  // Number of calls of FillMatchingLogins() method.
  int fill_matching_logins_calls_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestPasswordStore);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_TEST_PASSWORD_STORE_H_
