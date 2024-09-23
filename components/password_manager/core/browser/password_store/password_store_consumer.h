// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_CONSUMER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_CONSUMER_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace password_manager {

struct InteractionsStats;
struct PasswordForm;
class PasswordStoreInterface;

using LoginsResult = std::vector<PasswordForm>;
using LoginsResultOrError =
    absl::variant<LoginsResult, PasswordStoreBackendError>;

// Reads from the PasswordStoreInterface are done asynchronously on a separate
// thread. PasswordStoreConsumer provides the virtual callback method, which is
// guaranteed to be executed on this (the UI) thread. It also provides the
// base::CancelableTaskTracker member, which cancels any outstanding
// tasks upon destruction.
class PasswordStoreConsumer {
 public:
  // TODO(crbug.com/40238167): Use base::expected instead of absl::variant.
  PasswordStoreConsumer();

  // Called when `GetLogins()` request is finished, with a vector of forms or
  // with an error if the logins couldn't be fetched. The default implementation
  // calls `OnGetPasswordStoreResultsFrom` with the results or an empty vector
  // on error. Receives the originating `store`, useful for differentiateing
  // between the profile-scoped and account-scoped password stores.
  virtual void OnGetPasswordStoreResultsOrErrorFrom(
      PasswordStoreInterface* store,
      LoginsResultOrError results_or_error);

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

  // Called when the GetLogins() request is finished, with the associated
  // |results|.
  // TODO(crbug.com/40863002): Remove when the `FormsOrError` version is
  // implemented by all consumers.
  virtual void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results);

  // Like OnGetPasswordStoreResults(), but also receives the originating
  // PasswordStoreInterface as a parameter. This is useful for consumers that
  // query both the profile-scoped and the account-scoped store. The default
  // implementation simply calls OnGetPasswordStoreResults(), so consumers that
  // don't care about the store can just ignore this.
  // TODO(crbug.com/40863002): Remove when the `FormsOrError` version is
  // implemented by all consumers.
  virtual void OnGetPasswordStoreResultsFrom(
      PasswordStoreInterface* store,
      std::vector<std::unique_ptr<PasswordForm>> results);

 private:
  base::CancelableTaskTracker cancelable_task_tracker_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_CONSUMER_H_
