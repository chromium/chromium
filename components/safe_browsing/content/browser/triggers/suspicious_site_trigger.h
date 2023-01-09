// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_SUSPICIOUS_SITE_TRIGGER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_SUSPICIOUS_SITE_TRIGGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PrefService;

namespace history {
class HistoryService;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {
class ReferrerChainProvider;
class TriggerManager;

// Metric for tracking what the Suspicious Site trigger does on each event.
extern const char kSuspiciousSiteTriggerEventMetricName[];

// Local metric for tracking how often reports from this trigger are rejected
// by the trigger manager, and for what reason.
extern const char kSuspiciousSiteTriggerReportRejectionTestMetricName[];

// Local metric for tracking the state of the trigger when the report delay
// timer fires.
extern const char kSuspiciousSiteTriggerReportDelayStateTestMetricName[];

// Tracks events this trigger listens for or actions it performs. These values
// are written to logs. New enum values can be added, but existing enums must
// never be renumbered or deleted and reused.
enum class SuspiciousSiteTriggerEvent {
  // A page load started.
  PAGE_LOAD_START = 0,
  // A page load finished.
  PAGE_LOAD_FINISH = 1,
  // A suspicious site was detected.
  SUSPICIOUS_SITE_DETECTED = 2,
  // The report delay timer fired.
  REPORT_DELAY_TIMER = 3,
  // A suspicious site report was started.
  REPORT_STARTED = 4,
  // A suspicious site report was created and sent.
  REPORT_FINISHED = 5,
  // The trigger was waiting for a load to finish before creating a report but
  // a new load started before the previous load could finish, so the report
  // was cancelled.
  PENDING_REPORT_CANCELLED_BY_LOAD = 6,
  // The trigger tried to start the report but it was rejected by the trigger
  // manager.
  REPORT_START_FAILED = 7,
  // The trigger tried to finish the report but it was rejected by the trigger
  // manager.
  REPORT_FINISH_FAILED = 8,
  // The trigger could have sent a report but it was skipped, typically because
  // the trigger was out of quota.
  REPORT_POSSIBLE_BUT_SKIPPED = 9,
  // New events must be added before kMaxValue, and the value of kMaxValue
  // updated.
  kMaxValue = REPORT_POSSIBLE_BUT_SKIPPED
};

// Notify a suspicious site trigger on a particular tab that a suspicious site
// was detected. |web_contents_getter| specifies the tab where the site was
// detected.
// Must be called on UI thread.
void NotifySuspiciousSiteTriggerDetected(
    const base::RepeatingCallback<content::WebContents*()>&
        web_contents_getter);

// This class watches tab-level events such as the start and end of a page
// load, and also listens for events from the SuspiciousSiteURLThrottle that
// indicate there was a hit on the suspicious site list. This trigger is
// repsonsible for creating reports about the page at the right time, based on
// the sequence of such events.
class SuspiciousSiteTrigger
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SuspiciousSiteTrigger> {
 public:
  // The different states the trigger could be in.
  // These values are written to logs. New enum values can be added, but
  // existing enums must never be renumbered or deleted and reused.
  enum class TriggerState {
    // Trigger is idle, page is not loading, no report requested.
    IDLE = 0,
    // Page load has started, no report requested.
    LOADING = 1,
    // Page load has started and a report is requested. The report will be
    // created when the page load finishes.
    LOADING_WILL_REPORT = 2,
    // A page load finished and a report for the page has started.
    REPORT_STARTED = 3,
    // The trigger is in monitoring mode where it listens for events and
    // increments some metrics but never sends reports. The trigger will never
    // leave this state.
    MONITOR_MODE = 4,
    // New states must be added before kMaxValue and the value of kMaxValue
    // updated.
    kMaxValue = MONITOR_MODE
  };

  SuspiciousSiteTrigger(const SuspiciousSiteTrigger&) = delete;
  SuspiciousSiteTrigger& operator=(const SuspiciousSiteTrigger&) = delete;

  ~SuspiciousSiteTrigger() override;

  // content::WebContentsObserver implementations.
  void DidStartLoading() override;
  void DidStopLoading() override;

  // Called when a suspicious site has been detected on the tab that this
  // trigger is running on.
  void SuspiciousSiteDetected();

 private:
  friend class content::WebContentsUserData<SuspiciousSiteTrigger>;
  friend class SuspiciousSiteTriggerTest;

  SuspiciousSiteTrigger(
      content::WebContents* web_contents,
      TriggerManager* trigger_manager,
      PrefService* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service,
      ReferrerChainProvider* referrer_chain_provider,
      bool monitor_mode);

  // Tries to start a report. Returns whether a report started successfully.
  // If a report is started, a delayed callback will also begin to notify
  // the trigger when the report should be completed and sent.
  bool MaybeStartReport();

  // Calls into the trigger manager to finish the active report and send it.
  void FinishReport();

  // Called when a suspicious site is detected while in monitor mode. We update
  // metrics if we determine that a report could have been sent had the trigger
  // been active.
  void SuspiciousSiteDetectedWhenMonitoring();

  // Called when the report delay timer fires, indicating that the active
  // report should be completed and sent.
  void ReportDelayTimerFired();

  // Sets a task runner to use for tests.
  void SetTaskRunnerForTest(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // The delay (in milliseconds) to wait before finishing a report. Can be
  // overwritten for tests.
  int64_t finish_report_delay_ms_;

  // Current state of the trigger. Used to synchronize page load events with
  // suspicious site list hit events so that reports can be generated at the
  // right time.
  TriggerState current_state_;

  // TriggerManager gets called if this trigger detects a suspicious site and
  // wants to collect data abou tit. Not owned.
  raw_ptr<TriggerManager> trigger_manager_;

  raw_ptr<PrefService> prefs_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<history::HistoryService> history_service_;
  raw_ptr<ReferrerChainProvider> referrer_chain_provider_;

  // Task runner for posting delayed tasks. Normally set to the runner for the
  // UI thread, but can be overwritten for tests.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<SuspiciousSiteTrigger> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_SUSPICIOUS_SITE_TRIGGER_H_
