// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_CONSUMER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_CONSUMER_H_

#include <memory>
#include <variant>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"

namespace password_manager {

struct InteractionsStats;
class PasswordStoreInterface;

using LoginsResult = std::vector<StoredCredential>;
using LoginsResultOrError =
    std::variant<LoginsResult, PasswordStoreBackendError>;

// Reads from the PasswordStoreInterface are done asynchronously on a separate
// thread. PasswordStoreConsumer provides the virtual callback method, which is
// guaranteed to be executed on this (the UI) thread. It also provides the
// base::CancelableTaskTracker member, which cancels any outstanding
// tasks upon destruction.
class PasswordStoreConsumer {
 public:
  // TODO(crbug.com/40238167): Use base::expected instead of std::variant.
  PasswordStoreConsumer();

  // Called when `GetLogins()` request is finished, with a vector of forms or
  // with an error if the logins couldn't be fetched. Receives the originating
  // `store`, useful for differentiateing between the profile-scoped and
  // account-scoped password stores.
  virtual void OnGetPasswordStoreResultsOrErrorFrom(
      PasswordStoreInterface* store,
      LoginsResultOrError results_or_error) = 0;

  // Called when the GetSiteStats() request is finished, with the associated
  // site statistics.
  virtual void OnGetSiteStatistics(std::vector<InteractionsStats> stats);

  // The base::CancelableTaskTracker can be used for cancelling the
  // tasks associated with the consumer.
  base::CancelableTaskTracker* cancelable_task_tracker() {
    return &cancelable_task_tracker_;
  }

 protected:
  virtual ~PasswordStoreConsumer();

 private:
  base::CancelableTaskTracker cancelable_task_tracker_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_CONSUMER_H_
