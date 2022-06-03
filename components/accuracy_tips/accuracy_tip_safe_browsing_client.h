// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_SAFE_BROWSING_CLIENT_H_
#define COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_SAFE_BROWSING_CLIENT_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"

class GURL;

namespace base {
class SequencedTaskRunner;
}

namespace accuracy_tips {

class AccuracyTipSafeBrowsingClient
    : public base::RefCountedThreadSafe<AccuracyTipSafeBrowsingClient>,
      public safe_browsing::SafeBrowsingDatabaseManager::Client {
 public:
  using AccuracyCheckCallback = base::OnceCallback<void(AccuracyTipStatus)>;

  AccuracyTipSafeBrowsingClient(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> sb_database,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Check status of URL with SafeBrowsingDatabaseManager. Will call
  // |callback| with result on UI thread.
  void CheckAccuracyStatus(const GURL& url, AccuracyCheckCallback callback);

  // Cancels pending tasks in |sb_database|.
  void Shutdown();

 private:
  // Check status of URL with SafeBrowsingDatabaseManager. Will call
  // |callback| with result on UI thread.
  void CheckAccuracyStatusOnIOThread(const GURL& url,
                                     AccuracyCheckCallback callback);

  // Replies to |callback| with |status| and ensure that this happens on the
  // ui thread.
  void ReplyOnUIThread(AccuracyCheckCallback callback,
                       AccuracyTipStatus status);

  void ShutdownOnIOThread();

  // SafeBrowsingDatabaseManager::Client:
  void OnCheckUrlForAccuracyTip(bool should_show_accuracy_tip) override;

  friend class base::RefCountedThreadSafe<AccuracyTipSafeBrowsingClient>;
  ~AccuracyTipSafeBrowsingClient() override;

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> sb_database_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  AccuracyCheckCallback pending_callback_;  // accessed on io thread!
};

}  // namespace accuracy_tips

#endif  // COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_SAFE_BROWSING_CLIENT_H_
