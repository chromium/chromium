// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INSECURE_CREDENTIALS_CONSUMER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INSECURE_CREDENTIALS_CONSUMER_H_

#include <vector>

#include "base/task/cancelable_task_tracker.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"

namespace password_manager {

class PasswordStore;

// Reads information associated with insecure credentials history from the
// PasswordStore. Reads are done asynchronously on a separate thread. It
// provides the virtual callback method, which is guaranteed to be executed on
// this (the UI) thread. It also provides the base::CancelableTaskTracker
// member, which cancels any outstanding tasks upon destruction.
class InsecureCredentialsConsumer {
 public:
  InsecureCredentialsConsumer();

  // Called when the GetAllInsecureCredentials() request is finished, with
  // the associated |insecure_credentials|.
  virtual void OnGetInsecureCredentials(
      std::vector<InsecureCredential> insecure_credentials) = 0;

  // Like OnGetInsecureCredentials(), but also receives the originating
  // PasswordStore as a parameter. This is useful for consumers that query
  // both the profile-scoped and the account-scoped store. The default
  // implementation simply calls OnGetInsecureCredentials(), so consumers
  // that don't care about the store can just ignore this.
  virtual void OnGetInsecureCredentialsFrom(
      PasswordStore* store,
      std::vector<InsecureCredential> insecure_credentials);

  // The base::CancelableTaskTracker can be used for cancelling the tasks
  // associated with the consumer.
  base::CancelableTaskTracker* cancelable_task_tracker() {
    return &cancelable_task_tracker_;
  }

  base::WeakPtr<InsecureCredentialsConsumer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  virtual ~InsecureCredentialsConsumer();

 private:
  base::CancelableTaskTracker cancelable_task_tracker_;
  base::WeakPtrFactory<InsecureCredentialsConsumer> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INSECURE_CREDENTIALS_CONSUMER_H_
