// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store_results_observer.h"

#include <utility>

#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

PasswordStoreResultsObserver::PasswordStoreResultsObserver() = default;
PasswordStoreResultsObserver::~PasswordStoreResultsObserver() = default;

void PasswordStoreResultsObserver::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> results) {
  results_ = std::move(results);
  run_loop_.Quit();
}

base::WeakPtr<password_manager::PasswordStoreConsumer>
PasswordStoreResultsObserver::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::vector<std::unique_ptr<password_manager::PasswordForm>>
PasswordStoreResultsObserver::WaitForResults() {
  run_loop_.Run();
  return std::move(results_);
}

}  // namespace password_manager
