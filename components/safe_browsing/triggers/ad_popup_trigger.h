// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_TRIGGERS_AD_POPUP_TRIGGER_H_
#define COMPONENTS_SAFE_BROWSING_TRIGGERS_AD_POPUP_TRIGGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_user_data.h"

class PrefService;

namespace history {
class HistoryService;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {
class TriggerManager;

// Metric for tracking what the Ad Popup trigger does on each navigation.
extern const char kAdPopupTriggerActionMetricName[];

enum class AdPopupTriggerAction {
  // An event occurred that caused the trigger to perform its checks.
  POPUP_CHECK = 0,
  // A popup cause by an ad was detected and a report was collected.
  POPUP_REPORTED = 1,
  // No ad was detected.
  POPUP_NO_GOOGLE_AD = 2,
  // An ad was detected on the page causing a popup and could have been
  // reported, but the trigger manager rejected the report (eg: because user is
  // incognito or has not opted into extended reporting).
  POPUP_COULD_NOT_START_REPORT = 3,
  // Daily quota for ads that caused blocked popups was met.
  POPUP_DAILY_QUOTA_EXCEEDED = 4,
  // New events must be added before kMaxValue, and the value of kMaxValue
  // updated.
  kMaxValue = POPUP_DAILY_QUOTA_EXCEEDED
};

// This class is notified when a popup caused by an ad in the browser is
// blocked. If the Ad is a Google Ad, this class sends a report to Google.
// Design doc: go/extending-chrind-q2-2019-1
class AdPopupTrigger : public content::WebContentsUserData<AdPopupTrigger> {
 public:
  ~AdPopupTrigger() override;

  static void CreateForWebContents(
      content::WebContents* web_contents,
      TriggerManager* trigger_manager,
      PrefService* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service);

  void PopupWasBlocked(content::RenderFrameHost* render_frame);

 private:
  friend class AdPopupTriggerTest;
  friend class content::WebContentsUserData<AdPopupTrigger>;

  AdPopupTrigger(
      content::WebContents* web_contents,
      TriggerManager* trigger_manager,
      PrefService* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service);

  // Called to create an ad popup report.
  void CreateAdPopupReport();

  // Sets a task runner to use for tests.
  void SetTaskRunnerForTest(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // WebContents of the current tab.
  content::WebContents* web_contents_;

  // The delay (in milliseconds) to wait before starting a report. Can be
  // ovewritten for tests.
  int64_t start_report_delay_ms_;

  // The delay (in milliseconds) to wait before finishing a report. Can be
  // overwritten for tests.
  int64_t finish_report_delay_ms_;

  // TriggerManager gets called if this trigger detects apopup caused by ad and
  // wants to collect some data about it. Not owned.
  TriggerManager* trigger_manager_;

  PrefService* prefs_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  history::HistoryService* history_service_;

  // Task runner for posting delayed tasks. Normally set to the runner for the
  // UI thread, but can be overwritten for tests.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<AdPopupTrigger> weak_ptr_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(AdPopupTrigger);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_TRIGGERS_AD_POPUP_TRIGGER_H_
