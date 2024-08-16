// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_counter.h"

#include <cstddef>

#include "base/check_op.h"
#include "base/notreached.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"

namespace password_manager {

PasswordCounter::PasswordCounter(PasswordStoreInterface* profile_store,
                                 PasswordStoreInterface* account_store,
                                 Delegate* delegate)
    : delegate_(delegate),
      profile_store_(profile_store),
      account_store_(account_store),
      profile_observer_(this),
      account_observer_(this) {
  profile_store->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
  if (account_store) {
    account_store->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
  }
}

PasswordCounter::~PasswordCounter() = default;

void PasswordCounter::OnGetPasswordStoreResultsOrErrorFrom(
    PasswordStoreInterface* store,
    LoginsResultOrError results_or_error) {
  if (absl::holds_alternative<password_manager::PasswordStoreBackendError>(
          results_or_error)) {
    return;
  }
  size_t counter =
      absl::get<password_manager::LoginsResult>(results_or_error).size();
  if (store == profile_store_) {
    profile_passwords_ = counter;
    profile_observer_.Observe(store);
  } else {
    account_passwords_ = counter;
    account_observer_.Observe(store);
  }
  NotifyDelegate();
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
        if (!change.form().blocked_by_user) {
          counter++;
        }
        break;
      case PasswordStoreChange::UPDATE:
        break;
      case PasswordStoreChange::REMOVE:
        if (!change.form().blocked_by_user) {
          counter--;
        }
        break;
    }
  }
  if (old_value != counter) {
    NotifyDelegate();
  }
}

void PasswordCounter::OnLoginsRetained(
    PasswordStoreInterface* store,
    const std::vector<PasswordForm>& retained_passwords) {
  NOTREACHED() << "Needs to be implemented for Android if needed";
}

void PasswordCounter::NotifyDelegate() {
  if (delegate_) {
    delegate_->OnPasswordCounterChanged();
  }
}

}  // namespace password_manager
