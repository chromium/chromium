// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_TEST_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_TEST_PASSWORD_STORE_H_

#include <functional>
#include <type_traits>

#include "base/callback_list.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

// A matcher that compares two PasswordForm or StoredCredential instances but
// ignores the |in_store| member.
MATCHER_P(MatchesFormExceptStore, expected, "") {
  auto to_form = [](const auto& x) {
    if constexpr (std::is_same_v<std::decay_t<decltype(x)>, StoredCredential>) {
      return ToPasswordForm(x);
    } else {
      return x;
    }
  };

  auto arg_form = to_form(arg);
  auto expected_form = to_form(expected);
  arg_form.in_store = expected_form.in_store;

  return arg_form == expected_form;
}

// A very simple PasswordStore implementation that keeps all of the passwords
// in memory and does all its manipulations on the main thread. Since this
// is only used for testing, only the parts of the interface that are needed
// for testing have been implemented.
// TODO(crbug.com/40774158): Implement only the PasswordStoreInterface.
class TestPasswordStore : public PasswordStore {
 public:
  // TODO(crbug.com/40774158): Clean up all references using this.
  using PasswordMap =
      std::map<std::string, std::vector<PasswordForm>, std::less<>>;

  // We need to qualify password_manager::IsAccountStore with the full
  // namespace, otherwise, it's confused with the method
  // PasswordStoreSync::IsAccountStore().
  explicit TestPasswordStore(password_manager::IsAccountStore is_account_store =
                                 password_manager::IsAccountStore(false));

  TestPasswordStore(const TestPasswordStore&) = delete;
  TestPasswordStore& operator=(const TestPasswordStore&) = delete;

  ::password_manager::IsAccountStore IsAccountStore() const;

  base::CallbackListSubscription AddSyncEnabledOrDisabledCallback(
      base::RepeatingClosure sync_enabled_or_disabled_cb) override;

  void CallSyncEnabledOrDisabledCallbacks();

  void TriggerOnLoginsRetainedForAndroid(
      const std::vector<StoredCredential>& credentials);

  void ReturnErrorOnRequest(
      PasswordStoreBackendError password_store_backend_error);

  void SetError(ActionableError error);

  void NotifyAboutError();

 protected:
  ~TestPasswordStore() override;

 private:
  FakePasswordStoreBackend* fake_backend();
  const FakePasswordStoreBackend* fake_backend() const;

  base::RepeatingClosureList sync_enabled_or_disabled_cbs_;
};

TestPasswordStore::PasswordMap GetAllLoginsSync(PasswordStoreInterface* store);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_TEST_PASSWORD_STORE_H_
