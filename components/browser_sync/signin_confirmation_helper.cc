// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/signin_confirmation_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"

namespace browser_sync {

namespace {

// Determines whether there are any typed URLs in a history backend.
class HasTypedURLsTask : public history::HistoryDBTask {
 public:
  explicit HasTypedURLsTask(base::OnceCallback<void(bool)> cb)
      : has_typed_urls_(false), cb_(std::move(cb)) {}
  ~HasTypedURLsTask() override {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    history::URLRows rows;
    backend->GetAllTypedURLs(&rows);
    if (!rows.empty()) {
      DVLOG(1) << "SigninConfirmationHelper: history contains " << rows.size()
               << " typed URLs";
      has_typed_urls_ = true;
    }
    return true;
  }

  void DoneRunOnMainThread() override { std::move(cb_).Run(has_typed_urls_); }

 private:
  bool has_typed_urls_;
  base::OnceCallback<void(bool)> cb_;
};

}  // namespace

SigninConfirmationHelper::SigninConfirmationHelper(
    history::HistoryService* history_service,
    base::OnceCallback<void(bool)> return_result)
    : origin_sequence_(base::SequencedTaskRunnerHandle::Get()),
      history_service_(history_service),
      pending_requests_(0),
      return_result_(std::move(return_result)) {}

SigninConfirmationHelper::~SigninConfirmationHelper() {
  DCHECK(origin_sequence_->RunsTasksInCurrentSequence());
}

void SigninConfirmationHelper::OnHistoryQueryResults(
    size_t max_entries,
    history::QueryResults results) {
  bool too_much_history = results.size() >= max_entries;
  if (too_much_history) {
    DVLOG(1) << "SigninConfirmationHelper: profile contains " << results.size()
             << " history entries";
  }
  ReturnResult(too_much_history);
}

void SigninConfirmationHelper::CheckHasHistory(int max_entries) {
  pending_requests_++;
  if (!history_service_) {
    PostResult(false);
    return;
  }
  history::QueryOptions opts;
  opts.max_count = max_entries;
  history_service_->QueryHistory(
      base::string16(), opts,
      base::BindOnce(&SigninConfirmationHelper::OnHistoryQueryResults,
                     base::Unretained(this), max_entries),
      &task_tracker_);
}

void SigninConfirmationHelper::CheckHasTypedURLs() {
  pending_requests_++;
  if (!history_service_) {
    PostResult(false);
    return;
  }
  history_service_->ScheduleDBTask(
      FROM_HERE,
      std::make_unique<HasTypedURLsTask>(base::BindOnce(
          &SigninConfirmationHelper::ReturnResult, base::Unretained(this))),
      &task_tracker_);
}

void SigninConfirmationHelper::PostResult(bool result) {
  origin_sequence_->PostTask(
      FROM_HERE, base::BindOnce(&SigninConfirmationHelper::ReturnResult,
                                base::Unretained(this), result));
}

void SigninConfirmationHelper::ReturnResult(bool result) {
  DCHECK(origin_sequence_->RunsTasksInCurrentSequence());
  // Pass |true| into the callback as soon as one of the tasks passes a
  // result of |true|, otherwise pass the last returned result.
  if (--pending_requests_ == 0 || result) {
    std::move(return_result_).Run(result);

    // This leaks at shutdown if the HistoryService is destroyed, but
    // the process is going to die anyway.
    delete this;
  }
}

}  // namespace browser_sync
