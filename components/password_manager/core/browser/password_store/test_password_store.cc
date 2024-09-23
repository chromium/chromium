// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/test_password_store.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"

namespace password_manager {

TestPasswordStore::TestPasswordStore(
    password_manager::IsAccountStore is_account_store)
    : PasswordStore(
          std::make_unique<FakePasswordStoreBackend>(is_account_store)) {}

void TestPasswordStore::Clear() {
  DCHECK(fake_backend()) << "Store has already been shut down!";
  fake_backend()->Clear();
}

bool TestPasswordStore::IsEmpty() const {
  DCHECK(fake_backend()) << "Store has already been shut down!";
  // The store is empty, if the sum of all stored passwords across all entries
  // in |stored_passwords_| is 0.
  size_t number_of_passwords = 0u;
  for (auto it = fake_backend()->stored_passwords().begin();
       !number_of_passwords && it != fake_backend()->stored_passwords().end();
       ++it) {
    number_of_passwords += it->second.size();
  }
  return number_of_passwords == 0u;
}

const TestPasswordStore::PasswordMap& TestPasswordStore::stored_passwords()
    const {
  DCHECK(fake_backend()) << "Store has already been shut down!";
  return fake_backend()->stored_passwords();
}

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
    const std::vector<PasswordForm>& password_forms) {
  fake_backend()->TriggerOnLoginsRetainedForAndroid(password_forms);
}

TestPasswordStore::~TestPasswordStore() = default;

FakePasswordStoreBackend* TestPasswordStore::fake_backend() {
  return static_cast<FakePasswordStoreBackend*>(GetBackendForTesting());
}

const FakePasswordStoreBackend* TestPasswordStore::fake_backend() const {
  return const_cast<TestPasswordStore*>(this)->fake_backend();
}

}  // namespace password_manager
