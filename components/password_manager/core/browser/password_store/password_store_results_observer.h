// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_RESULTS_OBSERVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_RESULTS_OBSERVER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

namespace password_manager {

struct PasswordForm;

// A helper class that synchronously waits until the password store handles a
// GetLogins() request.
class PasswordStoreResultsObserver : public PasswordStoreConsumer {
 public:
  PasswordStoreResultsObserver();

  PasswordStoreResultsObserver(const PasswordStoreResultsObserver&) = delete;
  PasswordStoreResultsObserver& operator=(const PasswordStoreResultsObserver&) =
      delete;

  ~PasswordStoreResultsObserver() override;

  // Waits for OnGetPasswordStoreResults() and returns the result.
  std::vector<PasswordForm> WaitForResults();

  base::WeakPtr<PasswordStoreConsumer> GetWeakPtr();

 private:
  void OnGetPasswordStoreResultsOrErrorFrom(
      PasswordStoreInterface* store,
      LoginsResultOrError results_or_error) override;

  base::RunLoop run_loop_;
  std::vector<PasswordForm> results_;
  base::WeakPtrFactory<PasswordStoreResultsObserver> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_RESULTS_OBSERVER_H_
