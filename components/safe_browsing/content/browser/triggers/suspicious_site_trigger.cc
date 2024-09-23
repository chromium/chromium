// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/triggers/suspicious_site_trigger.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/task/sequenced_task_runner.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/triggers/trigger_manager.h"
#include "components/safe_browsing/content/browser/triggers/trigger_throttler.h"
#include "components/safe_browsing/content/browser/unsafe_resource_util.h"
#include "components/safe_browsing/content/browser/web_contents_key.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
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

const char kSuspiciousSiteTriggerReportRejectionTestMetricName[] =
    "SafeBrowsing.Triggers.SuspiciousSite.ReportRejectionReason";

const char kSuspiciousSiteTriggerReportDelayStateTestMetricName[] =
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
    ReferrerChainProvider* referrer_chain_provider,
    bool monitor_mode)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SuspiciousSiteTrigger>(*web_contents),
      finish_report_delay_ms_(kSuspiciousSiteCollectionPeriodMilliseconds),
      current_state_(monitor_mode ? TriggerState::MONITOR_MODE
                                  : TriggerState::IDLE),
      trigger_manager_(trigger_manager),
      prefs_(prefs),
      url_loader_factory_(url_loader_factory),
      history_service_(history_service),
      referrer_chain_provider_(referrer_chain_provider),
      task_runner_(content::GetUIThreadTaskRunner({})) {}

SuspiciousSiteTrigger::~SuspiciousSiteTrigger() {}

bool SuspiciousSiteTrigger::MaybeStartReport() {
  SBErrorOptions error_options =
      TriggerManager::GetSBErrorDisplayOptions(*prefs_, web_contents());

  // We use primary page's main document to construct `resource` below, since
  // `WebContents::StartLoading()` is invoked for navigations which updates it.
  // For more details, refer to the below document:
  // https://docs.google.com/document/d/1WqzPSpWtJ9bqaaecYWTWvg6h2Q1UMh7kVB8C61o8QL4/edit?resourcekey=0-q9YIhsh5LS-ZfBXcBQCOew#heading=h.b3xiyzalkc6q
  content::RenderFrameHost& primary_rfh =
      web_contents()->GetPrimaryPage().GetMainDocument();
  const content::GlobalRenderFrameHostId primary_rfh_id =
      primary_rfh.GetGlobalId();

  security_interstitials::UnsafeResource resource;
  resource.threat_type = SBThreatType::SB_THREAT_TYPE_SUSPICIOUS_SITE;
  resource.url = primary_rfh.GetLastCommittedURL();
  resource.render_process_id = primary_rfh_id.child_id;
  resource.render_frame_token = primary_rfh.GetFrameToken().value();

  TriggerManagerReason reason;
  if (!trigger_manager_->StartCollectingThreatDetailsWithReason(
          TriggerType::SUSPICIOUS_SITE, web_contents(), resource,
          url_loader_factory_, history_service_, referrer_chain_provider_,
          error_options, &reason)) {
    UMA_HISTOGRAM_ENUMERATION(kSuspiciousSiteTriggerEventMetricName,
                              SuspiciousSiteTriggerEvent::REPORT_START_FAILED);
    UMA_HISTOGRAM_ENUMERATION(
        kSuspiciousSiteTriggerReportRejectionTestMetricName, reason);
    return false;
  }

  // Call back into the trigger after a short delay, allowing the report
  // to complete.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SuspiciousSiteTrigger::ReportDelayTimerFired,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(finish_report_delay_ms_));

  UMA_HISTOGRAM_ENUMERATION(kSuspiciousSiteTriggerEventMetricName,
                            SuspiciousSiteTriggerEvent::REPORT_STARTED);
  return true;
}

void SuspiciousSiteTrigger::FinishReport() {
  SBErrorOptions error_options =
      TriggerManager::GetSBErrorDisplayOptions(*prefs_, web_contents());
  auto result = trigger_manager_->FinishCollectingThreatDetails(
      TriggerType::SUSPICIOUS_SITE, GetWebContentsKey(web_contents()),
      base::TimeDelta(),
      /*did_proceed=*/false, /*num_visits=*/0, error_options);
  if (result.IsReportSent()) {
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
  // This local histogram is used as a signal for testing.
  UMA_HISTOGRAM_ENUMERATION(
      kSuspiciousSiteTriggerReportDelayStateTestMetricName, current_state_);
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
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SuspiciousSiteTrigger);

}  // namespace safe_browsing
