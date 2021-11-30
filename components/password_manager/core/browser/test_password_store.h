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
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
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

  TestPasswordStore(const TestPasswordStore&) = delete;
  TestPasswordStore& operator=(const TestPasswordStore&) = delete;

  using PasswordMap = std::map<std::string /* signon_realm */,
                               std::vector<PasswordForm>,
                               std::less<>>;

  const PasswordMap& stored_passwords() const;

  void Clear();

  // Returns true if no passwords are stored in the store. Note that this is not
  // as simple as asking whether stored_passwords().empty(), because the map can
  // have entries of size 0.
  bool IsEmpty() const;

  int fill_matching_logins_calls() const { return fill_matching_logins_calls_; }

  bool IsAccountStore() const;

 protected:
  ~TestPasswordStore() override;

  // PasswordStoreBackend interface
  base::WeakPtr<PasswordStoreBackend> GetWeakPtr() override;
  void InitBackend(RemoteChangesReceived remote_form_changes_received,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void Shutdown(base::OnceClosure shutdown_completed) override;
  void GetAllLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(LoginsOrErrorReply callback) override;
  void FillMatchingLoginsAsync(
      LoginsReply callback,
      bool include_psl,
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
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  FieldInfoStore* GetFieldInfoStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;

 private:
  LoginsResult GetAllLoginsInternal();
  LoginsResult GetAutofillableLoginsInternal();
  LoginsResult FillMatchingLogins(const PasswordFormDigest& form,
                                  bool include_psl);
  LoginsResult FillMatchingLoginsBulk(
      const std::vector<PasswordFormDigest>& forms,
      bool include_psl);
  PasswordStoreChangeList AddLoginImpl(const PasswordForm& form);
  PasswordStoreChangeList UpdateLoginImpl(const PasswordForm& form);
  PasswordStoreChangeList RemoveLoginImpl(const PasswordForm& form);

  const password_manager::IsAccountStore is_account_store_;

  PasswordMap stored_passwords_;

  const std::unique_ptr<PasswordStoreSync::MetadataStore> metadata_store_;

  // TaskRunner for tasks that run on the main sequence (usually the UI thread).
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // TaskRunner for all the background operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Number of calls of FillMatchingLogins() method.
  int fill_matching_logins_calls_ = 0;

  base::WeakPtrFactory<TestPasswordStore> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_TEST_PASSWORD_STORE_H_
