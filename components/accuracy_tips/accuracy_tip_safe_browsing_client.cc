// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/accuracy_tip_safe_browsing_client.h"

#include <utility>

#include "base/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "url/gurl.h"

namespace accuracy_tips {

AccuracyTipSafeBrowsingClient::AccuracyTipSafeBrowsingClient(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> sb_database,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : sb_database_(sb_database),
      ui_task_runner_(std::move(ui_task_runner)),
      io_task_runner_(std::move(io_task_runner)) {}

AccuracyTipSafeBrowsingClient::~AccuracyTipSafeBrowsingClient() = default;

void AccuracyTipSafeBrowsingClient::CheckAccuracyStatus(
    const GURL& url,
    AccuracyCheckCallback callback) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AccuracyTipSafeBrowsingClient::CheckAccuracyStatusOnIOThread, this,
          url, std::move(callback)));
}

void AccuracyTipSafeBrowsingClient::CheckAccuracyStatusOnIOThread(
    const GURL& url,
    AccuracyCheckCallback callback) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (pending_callback_) {
    sb_database_->CancelCheck(this);
    ReplyOnUIThread(std::move(pending_callback_), AccuracyTipStatus::kNone);
  }

  pending_callback_ = std::move(callback);
  if (sb_database_->CheckUrlForAccuracyTips(url, this)) {
    ReplyOnUIThread(std::move(pending_callback_), AccuracyTipStatus::kNone);
  }
}

void AccuracyTipSafeBrowsingClient::OnCheckUrlForAccuracyTip(
    bool should_show_accuracy_tip) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(pending_callback_);
  ReplyOnUIThread(std::move(pending_callback_),
                  should_show_accuracy_tip ? AccuracyTipStatus::kShowAccuracyTip
                                           : AccuracyTipStatus::kNone);
}

void AccuracyTipSafeBrowsingClient::ReplyOnUIThread(
    AccuracyCheckCallback callback,
    AccuracyTipStatus status) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(std::move(callback), status));
}

void AccuracyTipSafeBrowsingClient::Shutdown() {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AccuracyTipSafeBrowsingClient::ShutdownOnIOThread, this));
}

void AccuracyTipSafeBrowsingClient::ShutdownOnIOThread() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (pending_callback_)
    sb_database_->CancelCheck(this);
  sb_database_ = nullptr;
}

}  // namespace accuracy_tips
