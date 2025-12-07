// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_counter.h"

#include <cstddef>
#include <ranges>
#include <variant>

#include "base/check_op.h"
#include "base/notreached.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"

namespace password_manager {

namespace {

bool IsAutofillableCredential(const PasswordForm& form) {
  return !form.blocked_by_user && !form.IsFederatedCredential() &&
         form.scheme != PasswordForm::Scheme::kUsernameOnly;
}

}  // namespace

PasswordCounter::PasswordCounter(PasswordStoreInterface* profile_store,
                                 PasswordStoreInterface* account_store)
    : profile_store_(profile_store), account_store_(account_store) {
  if (profile_store) {
    profile_store->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
  }
  if (account_store) {
    account_store->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
  }
}

PasswordCounter::~PasswordCounter() = default;

void PasswordCounter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PasswordCounter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PasswordCounter::OnGetPasswordStoreResultsOrErrorFrom(
    PasswordStoreInterface* store,
    LoginsResultOrError results_or_error) {
  if (std::holds_alternative<password_manager::PasswordStoreBackendError>(
          results_or_error)) {
    return;
  }
  size_t counter = std::ranges::count_if(
      std::get<password_manager::LoginsResult>(results_or_error),
      &IsAutofillableCredential);
  if (store == profile_store_) {
    profile_passwords_ = counter;
    profile_observer_.Observe(store);
  } else {
    account_passwords_ = counter;
    account_observer_.Observe(store);
  }
  NotifyObservers();
}

void PasswordCounter::OnLoginsChanged(PasswordStoreInterface* store,
                                      const PasswordStoreChangeList& changes) {
  size_t& counter = profile_observer_.IsObservingSource(store)
                        ? profile_passwords_
                        : account_passwords_;
  const size_t old_value = counter;
  for (const PasswordStoreChange& change : changes) {
    switch (change.type()) {
      case PasswordStoreChange::ADD:
        if (IsAutofillableCredential(change.form())) {
          counter++;
        }
        break;
      case PasswordStoreChange::UPDATE:
        break;
      case PasswordStoreChange::REMOVE:
        if (IsAutofillableCredential(change.form())) {
          counter--;
        }
        break;
    }
  }
  if (old_value != counter) {
    NotifyObservers();
  }
}

void PasswordCounter::OnLoginsRetained(
    PasswordStoreInterface* store,
    const std::vector<PasswordForm>& retained_passwords) {
  NOTREACHED() << "Needs to be implemented for Android if needed";
}

void PasswordCounter::NotifyObservers() {
  observers_.Notify(&Observer::OnPasswordCounterChanged);
}

}  // namespace password_manager
