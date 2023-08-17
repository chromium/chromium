// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_SIGNIN_CONFIRMATION_HELPER_H_
#define COMPONENTS_BROWSER_SYNC_SIGNIN_CONFIRMATION_HELPER_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/cancelable_task_tracker.h"

namespace base {
class SequencedTaskRunner;
}

namespace history {
class HistoryService;
class QueryResults;
}  // namespace history

namespace browser_sync {

// Helper class for sync signin to check some asynchronous conditions. Call
// either CheckHasHistory or CheckHasTypedUrls or both, and |return_result|
// will be called with true if either returns true, otherwise false.
class SigninConfirmationHelper {
 public:
  SigninConfirmationHelper(history::HistoryService* history_service,
                           base::OnceCallback<void(bool)> return_result);

  SigninConfirmationHelper(const SigninConfirmationHelper&) = delete;
  SigninConfirmationHelper& operator=(const SigninConfirmationHelper&) = delete;

  // This helper checks if there are history entries in the history service.
  void CheckHasHistory(int max_entries);

  // This helper checks if there are typed URLs in the history service.
  void CheckHasTypedURLs();

 private:
  // Deletes itself.
  ~SigninConfirmationHelper();

  // Callback helper function for CheckHasHistory.
  void OnHistoryQueryResults(size_t max_entries, history::QueryResults results);

  // Posts the given result to the origin sequence.
  void PostResult(bool result);

  // Calls |return_result_| if |result| == true or if it's the result of the
  // last pending check.
  void ReturnResult(bool result);

  // The task runner for the sequence this object was constructed on.
  const scoped_refptr<base::SequencedTaskRunner> origin_sequence_;

  // Pointer to the history service.
  const raw_ptr<history::HistoryService> history_service_;

  // Used for async tasks.
  base::CancelableTaskTracker task_tracker_;

  // Keep track of how many async requests are pending.
  int pending_requests_;

  // Callback to pass the result back to the caller.
  base::OnceCallback<void(bool)> return_result_;
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_SIGNIN_CONFIRMATION_HELPER_H_
