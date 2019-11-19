// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_LEAK_HISTORY_CONSUMER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_LEAK_HISTORY_CONSUMER_H_

#include <vector>

#include "base/task/cancelable_task_tracker.h"

namespace password_manager {

struct CompromisedCredentials;

// Reads information associated with compromised credentials history from the
// PasswordStore. Reads are done asynchronously on a separate thread. It
// provides the virtual callback method, which is guaranteed to be executed on
// this (the UI) thread. It also provides the base::CancelableTaskTracker
// member, which cancels any outstanding tasks upon destruction.
class PasswordLeakHistoryConsumer {
 public:
  PasswordLeakHistoryConsumer();

  // Called when the GetAllCompromisedCredentials() request is finished, with
  // the associated |compromised_credentials|.
  virtual void OnGetCompromisedCredentials(
      std::vector<CompromisedCredentials> compromised_credentials) = 0;

  // The base::CancelableTaskTracker can be used for cancelling the tasks
  // associated with the consumer.
  base::CancelableTaskTracker* cancelable_task_tracker() {
    return &cancelable_task_tracker_;
  }

  base::WeakPtr<PasswordLeakHistoryConsumer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  virtual ~PasswordLeakHistoryConsumer();

 private:
  base::CancelableTaskTracker cancelable_task_tracker_;
  base::WeakPtrFactory<PasswordLeakHistoryConsumer> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_LEAK_HISTORY_CONSUMER_H_
