// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_CONSUMER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_CONSUMER_H_

#include <memory>
#include <vector>

#include "base/task/cancelable_task_tracker.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

struct FieldInfo;
struct InteractionsStats;

// Reads from the PasswordStore are done asynchronously on a separate
// thread. PasswordStoreConsumer provides the virtual callback method, which is
// guaranteed to be executed on this (the UI) thread. It also provides the
// base::CancelableTaskTracker member, which cancels any outstanding
// tasks upon destruction.
class PasswordStoreConsumer {
 public:
  PasswordStoreConsumer();

  // Called when the GetLogins() request is finished, with the associated
  // |results|.
  virtual void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) = 0;

  // Called when the GetSiteStats() request is finished, with the associated
  // site statistics.
  virtual void OnGetSiteStatistics(std::vector<InteractionsStats> stats);

  // Called when the GetAllFieldInfo() request is finished, with the associated
  // field info.
  virtual void OnGetAllFieldInfo(std::vector<FieldInfo> field_info);

  // The base::CancelableTaskTracker can be used for cancelling the
  // tasks associated with the consumer.
  base::CancelableTaskTracker* cancelable_task_tracker() {
    return &cancelable_task_tracker_;
  }

  base::WeakPtr<PasswordStoreConsumer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void CancelAllRequests();

 protected:
  virtual ~PasswordStoreConsumer();

 private:
  base::CancelableTaskTracker cancelable_task_tracker_;
  base::WeakPtrFactory<PasswordStoreConsumer> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_CONSUMER_H_
