// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/test_password_store.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "components/password_manager/core/browser/password_store/password_store_results_observer.h"

namespace password_manager {

TestPasswordStore::TestPasswordStore(
    password_manager::IsAccountStore is_account_store)
    : PasswordStore(
          std::make_unique<FakePasswordStoreBackend>(is_account_store)) {}

IsAccountStore TestPasswordStore::IsAccountStore() const {
  DCHECK(fake_backend()) << "Store has already been shut down!";
  return fake_backend()->is_account_store();
}

base::CallbackListSubscription
TestPasswordStore::AddSyncEnabledOrDisabledCallback(
    base::RepeatingClosure sync_enabled_or_disabled_cb) {
  return sync_enabled_or_disabled_cbs_.Add(
      std::move(sync_enabled_or_disabled_cb));
}

void TestPasswordStore::CallSyncEnabledOrDisabledCallbacks() {
  sync_enabled_or_disabled_cbs_.Notify();
}

void TestPasswordStore::TriggerOnLoginsRetainedForAndroid(
    const std::vector<StoredCredential>& credentials) {
  fake_backend()->TriggerOnLoginsRetainedForAndroid(credentials);
}

void TestPasswordStore::ReturnErrorOnRequest(
    PasswordStoreBackendError password_store_backend_error) {
  fake_backend()->ReturnErrorOnRequest(password_store_backend_error);
}

void TestPasswordStore::SetError(ActionableError error) {
  fake_backend()->SetError(error);
}

void TestPasswordStore::NotifyAboutError() {
  fake_backend()->NotifyAboutError();
}

TestPasswordStore::~TestPasswordStore() = default;

FakePasswordStoreBackend* TestPasswordStore::fake_backend() {
  return static_cast<FakePasswordStoreBackend*>(GetBackendForTesting());
}

const FakePasswordStoreBackend* TestPasswordStore::fake_backend() const {
  return const_cast<TestPasswordStore*>(this)->fake_backend();
}

TestPasswordStore::PasswordMap GetAllLoginsSync(PasswordStoreInterface* store) {
  PasswordStoreResultsObserver observer;
  store->GetAllLogins(observer.GetWeakPtr());
  std::vector<StoredCredential> results = observer.WaitForResults();
  TestPasswordStore::PasswordMap map;
  for (auto& result : results) {
    std::string signon_realm = result.signon_realm;
    map[signon_realm].push_back(ToPasswordForm(std::move(result)));
  }
  return map;
}

}  // namespace password_manager
