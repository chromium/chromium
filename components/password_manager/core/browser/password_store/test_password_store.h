// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_TEST_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_TEST_PASSWORD_STORE_H_

#include <functional>

#include "base/callback_list.h"
#include "components/password_manager/core/browser/password_store/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
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
// TODO(crbug.com/40774158): Implement only the PasswordStoreInterface.
class TestPasswordStore : public PasswordStore {
 public:
  // TODO(crbug.com/40774158): Clean up all references using this.
  using PasswordMap = password_manager::PasswordMap;

  // We need to qualify password_manager::IsAccountStore with the full
  // namespace, otherwise, it's confused with the method
  // PasswordStoreSync::IsAccountStore().
  explicit TestPasswordStore(password_manager::IsAccountStore is_account_store =
                                 password_manager::IsAccountStore(false));

  TestPasswordStore(const TestPasswordStore&) = delete;
  TestPasswordStore& operator=(const TestPasswordStore&) = delete;

  void Clear();

  // Returns true if no passwords are stored in the store. Note that this is not
  // as simple as asking whether stored_passwords().empty(), because the map can
  // have entries of size 0.
  bool IsEmpty() const;

  // TODO(crbug.com/40214044): Clean up non-essential methods.
  const TestPasswordStore::PasswordMap& stored_passwords() const;
  ::password_manager::IsAccountStore IsAccountStore() const;

  base::CallbackListSubscription AddSyncEnabledOrDisabledCallback(
      base::RepeatingClosure sync_enabled_or_disabled_cb) override;

  void CallSyncEnabledOrDisabledCallbacks();

  void TriggerOnLoginsRetainedForAndroid(
      const std::vector<PasswordForm>& password_forms);

 protected:
  ~TestPasswordStore() override;

 private:
  FakePasswordStoreBackend* fake_backend();
  const FakePasswordStoreBackend* fake_backend() const;

  base::RepeatingClosureList sync_enabled_or_disabled_cbs_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_TEST_PASSWORD_STORE_H_
