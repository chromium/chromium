// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPONENTS_SAFE_BROWSING_TRIGGERS_AD_REDIRECT_TRIGGER_H_
#define COMPONENTS_SAFE_BROWSING_TRIGGERS_AD_REDIRECT_TRIGGER_H_

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

using FrameTreeNodeId = int;

// Metric for tracking what the Ad Redirect trigger does on each navigation.
extern const char kAdRedirectTriggerActionMetricName[];

// Actions performed by this trigger. These values are written to logs. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
enum class AdRedirectTriggerAction {
  // A redirect event occurred that caused the trigger to perform its checks.
  REDIRECT_CHECK = 0,
  // A redirect caused by an ad was detected and a report was collected.
  AD_REDIRECT = 1,
  // No google ad was detected from the frame causing an autoredirect.
  REDIRECT_NO_GOOGLE_AD = 2,
  // An ad was detected on the page causing the redirect and could have been
  // reported, but the trigger manager rejected the report (eg: because user is
  // incognito or has not opted into extended reporting).
  REDIRECT_COULD_NOT_START_REPORT = 3,
  // Daily quota for blocked ad redirect reporting was met.
  REDIRECT_DAILY_QUOTA_EXCEEDED = 4,
  // New events must be added before kMaxValue, and the value of kMaxValue
  // updated.
  kMaxValue = REDIRECT_DAILY_QUOTA_EXCEEDED
};

// This class if notified when a redirect navigation is blocked in the renderer
// because there was no user gesture(an autoredirect). If the frame causing the
// autoredirect navigation is a Google Ad frame, this class sends a report to
// Google.
// Design doc: go/extending-chrind-q2-2019-1
class AdRedirectTrigger
    : public content::WebContentsUserData<AdRedirectTrigger> {
 public:
  ~AdRedirectTrigger() override;

  // |web_contents| is the WebContent of the page that a redirect event took
  // place on. |trigger_manager| ensures the trigger is only fired when
  // appropriate and keeps track of how often the trigger is fired. |prefs|
  // allows us to check that the user has opted in to Extended Reporting.
  // |url_loader_factory| allows us to access mojom::URLLoaderFactory.
  // |history_service| records page titles, visit times, and favicons, as well
  // as information about downloads.
  static void CreateForWebContents(
      content::WebContents* web_contents,
      TriggerManager* trigger_manager,
      PrefService* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service);

  // This method is called on the browser thread when a frame tries to navigate
  // its top level frame from the |initiator_url| without ever having received a
  // user gesture. If the frame causing the redirect navigation is a Google Ad
  // frame a report is sent to Google.
  void OnDidBlockNavigation(const GURL& initiator_url);

 private:
  friend class AdRedirectTriggerBrowserTest;
  friend class content::WebContentsUserData<AdRedirectTrigger>;

  AdRedirectTrigger(
      content::WebContents* web_contents,
      TriggerManager* trigger_manager,
      PrefService* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service);

  // Called to create an ad redirect report.
  void CreateAdRedirectReport();

  // Sets report delay for test.
  void SetDelayForTest(int start_report_delay, int finish_report_delay);

  // WebContents of the current tab.
  content::WebContents* web_contents_;

  // The delay (in milliseconds) to wait before starting a report. Can be
  // ovewritten for tests.
  int64_t start_report_delay_ms_;

  // The delay (in milliseconds) to wait before finishing a report. Can be
  // overwritten for tests.
  int64_t finish_report_delay_ms_;

  // TriggerManager gets called if this trigger detects an autoredirect caused
  // by a page with an ad and wants to collect some data about it. Not owned.
  TriggerManager* trigger_manager_;

  PrefService* prefs_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  history::HistoryService* history_service_;

  // Task runner for posting delayed tasks. Normally set to the runner for the
  // UI thread, but can be overwritten for tests.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<AdRedirectTrigger> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(AdRedirectTrigger);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_TRIGGERS_AD_REDIRECT_TRIGGER_H_
