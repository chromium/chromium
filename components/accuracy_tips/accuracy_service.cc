// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/accuracy_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/accuracy_tips/accuracy_tip_ui.h"
#include "components/accuracy_tips/features.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"

namespace accuracy_tips {

using AccuracyCheckCallback = AccuracyService::AccuracyCheckCallback;

class AccuracyTipSafeBrowsingClient
    : public base::RefCountedThreadSafe<AccuracyTipSafeBrowsingClient>,
      public safe_browsing::SafeBrowsingDatabaseManager::Client {
 public:
  AccuracyTipSafeBrowsingClient(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> sb_database,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner)
      : sb_database_(sb_database),
        ui_task_runner_(std::move(ui_task_runner)),
        io_task_runner_(std::move(io_task_runner)) {}

  void ShutdownOnIOThread();

  // Check status of URL with SafeBrowsingDatabaseManager. Will call
  // |callback| with result on UI thread.
  void CheckAccuracyStatusOnIOThread(const GURL& url,
                                     AccuracyCheckCallback callback);
  // Replies to |callback| with |status| and ensure that this happens on the
  // ui thread.
  void ReplyOnUIThread(AccuracyCheckCallback callback,
                       AccuracyTipStatus status);

  // SafeBrowsingDatabaseManager::Client:
  void OnCheckUrlForAccuracyTip(bool should_show_accuracy_tip) override;

 private:
  friend class base::RefCountedThreadSafe<AccuracyTipSafeBrowsingClient>;
  ~AccuracyTipSafeBrowsingClient() override = default;

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> sb_database_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  AccuracyCheckCallback pending_callback_;  // accessed on io thread!
};

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

void AccuracyTipSafeBrowsingClient::ShutdownOnIOThread() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (pending_callback_)
    sb_database_->CancelCheck(this);
  sb_database_ = nullptr;
}

AccuracyService::AccuracyService(
    std::unique_ptr<AccuracyTipUI> ui,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> sb_database,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : ui_(std::move(ui)),
      ui_task_runner_(ui_task_runner),
      io_task_runner_(io_task_runner),
      sample_url_(GURL(kSampleUrl.Get())) {
  if (sb_database) {
    sb_client_ = base::MakeRefCounted<AccuracyTipSafeBrowsingClient>(
        std::move(sb_database), std::move(ui_task_runner),
        std::move(io_task_runner));
  }
}

AccuracyService::~AccuracyService() = default;

void AccuracyService::Shutdown() {
  if (sb_client_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AccuracyTipSafeBrowsingClient::ShutdownOnIOThread,
                       std::move(sb_client_)));
  }
}

void AccuracyService::CheckAccuracyStatus(const GURL& url,
                                          AccuracyCheckCallback callback) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  if (sample_url_.is_valid() && url == sample_url_) {
    std::move(callback).Run(AccuracyTipStatus::kShowAccuracyTip);
    return;
  }

  if (!sb_client_) {
    std::move(callback).Run(AccuracyTipStatus::kNone);
    return;
  }

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AccuracyTipSafeBrowsingClient::CheckAccuracyStatusOnIOThread,
          sb_client_, url, std::move(callback)));
}

void AccuracyService::MaybeShowAccuracyTip(content::WebContents* web_contents) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug.com/1210891): Implement rate limiting.
  ui_->ShowAccuracyTip(
      web_contents, AccuracyTipStatus::kShowAccuracyTip,
      base::BindOnce(&AccuracyService::OnAccuracyTipClosed,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void AccuracyService::OnAccuracyTipClosed(
    base::TimeTicks time_opened,
    AccuracyTipUI::Interaction interaction) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  base::UmaHistogramEnumeration("Privacy.AccuracyTip.AccuracyTipInteraction",
                                interaction);
  base::UmaHistogramMediumTimes("Privacy.AccuracyTip.AccuracyTipTimeOpen",
                                base::TimeTicks::Now() - time_opened);
}

void AccuracyService::SetSampleUrlForTesting(const GURL& url) {
  sample_url_ = url;
}

}  // namespace accuracy_tips
