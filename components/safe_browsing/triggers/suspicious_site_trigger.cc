// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/suspicious_site_trigger.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/triggers/trigger_manager.h"
#include "components/safe_browsing/triggers/trigger_throttler.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {
// Number of milliseconds to allow data collection to run before sending a
// report (since this trigger runs in the background).
const int64_t kSuspiciousSiteCollectionPeriodMilliseconds = 5000;
}  // namespace

const char kSuspiciousSiteTriggerEventMetricName[] =
    "SafeBrowsing.Triggers.SuspiciousSite.Event";

const char kSuspiciousSiteTriggerReportRejectionMetricName[] =
    "SafeBrowsing.Triggers.SuspiciousSite.ReportRejectionReason";

const char kSuspiciousSiteTriggerReportDelayStateMetricName[] =
    "SafeBrowsing.Triggers.SuspiciousSite.DelayTimerState";

void NotifySuspiciousSiteTriggerDetected(
    const base::RepeatingCallback<content::WebContents*()>&
        web_contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents = web_contents_getter.Run();
  if (web_contents) {
    safe_browsing::SuspiciousSiteTrigger* trigger =
        safe_browsing::SuspiciousSiteTrigger::FromWebContents(web_contents);
    if (trigger)
      trigger->SuspiciousSiteDetected();
  }
}

SuspiciousSiteTrigger::SuspiciousSiteTrigger(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager,
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service,
    bool monitor_mode)
    : content::WebContentsObserver(web_contents),
      finish_report_delay_ms_(kSuspiciousSiteCollectionPeriodMilliseconds),
      current_state_(monitor_mode ? TriggerState::MONITOR_MODE
                                  : TriggerState::IDLE),
      trigger_manager_(trigger_manager),
      prefs_(prefs),
      url_loader_factory_(url_loader_factory),
      history_service_(history_service),
      task_runner_(
          base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})) {}

SuspiciousSiteTrigger::~SuspiciousSiteTrigger() {}

// static
void SuspiciousSiteTrigger::CreateForWebContents(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager,
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service,
    bool monitor_mode) {
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(), base::WrapUnique(new SuspiciousSiteTrigger(
                           web_contents, trigger_manager, prefs,
                           url_loader_factory, history_service, monitor_mode)));
  }
}

bool SuspiciousSiteTrigger::MaybeStartReport() {
  SBErrorOptions error_options =
      TriggerManager::GetSBErrorDisplayOptions(*prefs_, web_contents());

  security_interstitials::UnsafeResource resource;
  resource.threat_type = SB_THREAT_TYPE_SUSPICIOUS_SITE;
  resource.url = web_contents()->GetLastCommittedURL();
  resource.web_contents_getter = resource.GetWebContentsGetter(
      web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID());

  TriggerManagerReason reason;
  if (!trigger_manager_->StartCollectingThreatDetailsWithReason(
          TriggerType::SUSPICIOUS_SITE, web_contents(), resource,
          url_loader_factory_, history_service_, error_options, &reason)) {
    UMA_HISTOGRAM_ENUMERATION(kSuspiciousSiteTriggerEventMetricName,
                              SuspiciousSiteTriggerEvent::REPORT_START_FAILED);
    UMA_HISTOGRAM_ENUMERATION(kSuspiciousSiteTriggerReportRejectionMetricName,
                              reason);
    return false;
  }

  // Call back into the trigger after a short delay, allowing the report
  // to complete.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SuspiciousSiteTrigger::ReportDelayTimerFired,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(finish_report_delay_ms_));

  UMA_HISTOGRAM_ENUMERATION(kSuspiciousSiteTriggerEventMetricName,
                            SuspiciousSiteTriggerEvent::REPORT_STARTED);
  return true;
}

void SuspiciousSiteTrigger::FinishReport() {
  SBErrorOptions error_options =
      TriggerManager::GetSBErrorDisplayOptions(*prefs_, web_contents());
  if (trigger_manager_->FinishCollectingThreatDetails(
          TriggerType::SUSPICIOUS_SITE, web_contents(), base::TimeDelta(),
          /*did_proceed=*/false, /*num_visits=*/0, error_options)) {
    UMA_HISTOGRAM_ENUMERATION(kSuspiciousSiteTriggerEventMetricName,
                              SuspiciousSiteTriggerEvent::REPORT_FINISHED);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kSuspiciousSiteTriggerEventMetricName,
                              SuspiciousSiteTriggerEvent::REPORT_FINISH_FAILED);
  }
}

void SuspiciousSiteTrigger::SuspiciousSiteDetectedWhenMonitoring() {
  DCHECK_EQ(TriggerState::MONITOR_MODE, current_state_);
  SBErrorOptions error_options =
      TriggerManager::GetSBErrorDisplayOptions(*prefs_, web_contents());
  TriggerManagerReason reason;
  if (trigger_manager_->CanStartDataCollectionWithReason(
          error_options, TriggerType::SUSPICIOUS_SITE, &reason) ||
      reason == TriggerManagerReason::DAILY_QUOTA_EXCEEDED) {
    UMA_HISTOGRAM_ENUMERATION(
        kSuspiciousSiteTriggerEventMetricName,
        SuspiciousSiteTriggerEvent::REPORT_POSSIBLE_BUT_SKIPPED);
  }
}

void SuspiciousSiteTrigger::DidStartLoading() {
  UMA_HISTOGRAM_ENUMERATION(kSuspiciousSiteTriggerEventMetricName,
                            SuspiciousSiteTriggerEvent::PAGE_LOAD_START);
  switch (current_state_) {
    case TriggerState::IDLE:
      // Load started, move to loading state.
      current_state_ = TriggerState::LOADING;
      return;

    case TriggerState::LOADING:
      // No-op, still loading.
      return;

    case TriggerState::LOADING_WILL_REPORT:
      // This happens if the user leaves the suspicious page before it
      // finishes loading. A report can't be created in this case since the
      // page is now gone.
      UMA_HISTOGRAM_ENUMERATION(
          kSuspiciousSiteTriggerEventMetricName,
          SuspiciousSiteTriggerEvent::PENDING_REPORT_CANCELLED_BY_LOAD);
      current_state_ = TriggerState::LOADING;
      return;

    case TriggerState::REPORT_STARTED:
      // A new page load has started while creating the current report.
      // Finish the report immediately with whatever data has been captured
      // so far. A report timer will have already started, but it will be
      // ignored when it fires.
      current_state_ = TriggerState::LOADING;
      FinishReport();
      return;

    case TriggerState::MONITOR_MODE:
      // No-op, monitoring only.
      return;
  }
}

void SuspiciousSiteTrigger::DidStopLoading() {
  UMA_HISTOGRAM_ENUMERATION(kSuspiciousSiteTriggerEventMetricName,
                            SuspiciousSiteTriggerEvent::PAGE_LOAD_FINISH);

  switch (current_state_) {
    case TriggerState::IDLE:
      // No-op, load stopped and we're already idle.
      return;

    case TriggerState::LOADING:
      // Load finished, return to Idle state.
      current_state_ = TriggerState::IDLE;
      return;

    case TriggerState::LOADING_WILL_REPORT:
      // Suspicious site detected mid-load and the page has now
      // finished loading, so try starting a report now.
      // If we fail to start a report for whatever reason, return to Idle.
      if (MaybeStartReport()) {
        current_state_ = TriggerState::REPORT_STARTED;
      } else {
        current_state_ = TriggerState::IDLE;
      }
      return;

    case TriggerState::REPORT_STARTED:
      // No-op. Let the report continue running.
      return;

    case TriggerState::MONITOR_MODE:
      // No-op, monitoring only.
      return;
  }
}

void SuspiciousSiteTrigger::SuspiciousSiteDetected() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  UMA_HISTOGRAM_ENUMERATION(
      kSuspiciousSiteTriggerEventMetricName,
      SuspiciousSiteTriggerEvent::SUSPICIOUS_SITE_DETECTED);

  switch (current_state_) {
    case TriggerState::IDLE:
      // Suspicious site detected while idle, start a report immediately.
      // If we fail to start a report for whatever reason, remain Idle.
      if (MaybeStartReport()) {
        current_state_ = TriggerState::REPORT_STARTED;
      }
      return;

    case TriggerState::LOADING:
      // Suspicious site detected in the middle of the load, remember this
      // and let the page finish loading. The report will be started after
      // the page has loaded.
      current_state_ = TriggerState::LOADING_WILL_REPORT;
      return;

    case TriggerState::LOADING_WILL_REPORT:
      // No-op. Current page has multiple suspicious URLs in it, remain in
      // the LOADING_WILL_REPORT state. A report will begin when the page
      // finishes loading.
      return;

    case TriggerState::REPORT_STARTED:
      // No-op. The current report should capture all suspicious sites.
      return;

    case TriggerState::MONITOR_MODE:
      // We monitor how often a suspicious site hit could result in a report.
      SuspiciousSiteDetectedWhenMonitoring();
      return;
  }
}

void SuspiciousSiteTrigger::ReportDelayTimerFired() {
  UMA_HISTOGRAM_ENUMERATION(kSuspiciousSiteTriggerEventMetricName,
                            SuspiciousSiteTriggerEvent::REPORT_DELAY_TIMER);
  UMA_HISTOGRAM_ENUMERATION(kSuspiciousSiteTriggerReportDelayStateMetricName,
                            current_state_);
  switch (current_state_) {
    case TriggerState::IDLE:
    case TriggerState::LOADING:
    case TriggerState::LOADING_WILL_REPORT:
    case TriggerState::MONITOR_MODE:
      // Invalid, expecting to be in REPORT_STARTED state.
      return;

    case TriggerState::REPORT_STARTED:
      // The delay timer has fired so complete the current report.
      current_state_ = TriggerState::IDLE;
      FinishReport();
      return;
  }
}

void SuspiciousSiteTrigger::SetTaskRunnerForTest(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SuspiciousSiteTrigger)

}  // namespace safe_browsing
