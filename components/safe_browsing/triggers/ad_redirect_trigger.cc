// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/ad_redirect_trigger.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/triggers/trigger_manager.h"
#include "components/safe_browsing/triggers/trigger_throttler.h"
#include "components/safe_browsing/triggers/trigger_util.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace safe_browsing {

// Number of milliseconds to allow data collection to run before sending a
// report (since this trigger runs in the background).
const int64_t kAdRedirectCollectionPeriodMilliseconds = 5000;

// Range of number of milliseconds to wait after a page finished loading before
// starting a report. Allows ads which load in the background to finish loading.
const int64_t kMaxAdRedirectCollectionStartDelayMilliseconds = 5000;
const int64_t kMinAdRedirectCollectionStartDelayMilliseconds = 500;

// Metric for tracking what the Ad Redirect trigger does on each navigation.
const char kAdRedirectTriggerActionMetricName[] =
    "SafeBrowsing.Triggers.AdRedirect.Action";

namespace {

void RecordAdRedirectTriggerAction(AdRedirectTriggerAction action) {
  UMA_HISTOGRAM_ENUMERATION(kAdRedirectTriggerActionMetricName, action);
}

}  // namespace

AdRedirectTrigger::AdRedirectTrigger(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager,
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service)
    : web_contents_(web_contents),
      start_report_delay_ms_(
          base::RandInt(kMinAdRedirectCollectionStartDelayMilliseconds,
                        kMaxAdRedirectCollectionStartDelayMilliseconds)),
      finish_report_delay_ms_(kAdRedirectCollectionPeriodMilliseconds),
      trigger_manager_(trigger_manager),
      prefs_(prefs),
      url_loader_factory_(url_loader_factory),
      history_service_(history_service),
      task_runner_(
          base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})) {}

AdRedirectTrigger::~AdRedirectTrigger() {}

// static
void AdRedirectTrigger::CreateForWebContents(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager,
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service) {
  DCHECK(web_contents);
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(UserDataKey(),
                              base::WrapUnique(new AdRedirectTrigger(
                                  web_contents, trigger_manager, prefs,
                                  url_loader_factory, history_service)));
  }
}

void AdRedirectTrigger::CreateAdRedirectReport() {
  SBErrorOptions error_options =
      TriggerManager::GetSBErrorDisplayOptions(*prefs_, web_contents_);
  security_interstitials::UnsafeResource resource;
  resource.threat_type = SB_THREAT_TYPE_BLOCKED_AD_REDIRECT;
  resource.url = web_contents_->GetURL();
  resource.web_contents_getter = resource.GetWebContentsGetter(
      web_contents_->GetMainFrame()->GetProcess()->GetID(),
      web_contents_->GetMainFrame()->GetRoutingID());
  TriggerManagerReason reason;
  if (!trigger_manager_->StartCollectingThreatDetailsWithReason(
          TriggerType::AD_REDIRECT, web_contents_, resource,
          url_loader_factory_, history_service_, error_options, &reason)) {
    if (reason == TriggerManagerReason::DAILY_QUOTA_EXCEEDED) {
      RecordAdRedirectTriggerAction(
          AdRedirectTriggerAction::REDIRECT_DAILY_QUOTA_EXCEEDED);
    } else {
      RecordAdRedirectTriggerAction(
          AdRedirectTriggerAction::REDIRECT_COULD_NOT_START_REPORT);
    }
    return;
  }
  // Call into TriggerManager to finish the reports after a short delay. Any
  // ads that are detected during this delay will be rejected by TriggerManager
  // because a report is already being collected, so we won't send multiple
  // reports for the same page.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          IgnoreResult(&TriggerManager::FinishCollectingThreatDetails),
          base::Unretained(trigger_manager_), TriggerType::AD_REDIRECT,
          base::Unretained(web_contents_), base::TimeDelta(),
          /*did_proceed=*/false, /*num_visits=*/0, error_options),
      base::TimeDelta::FromMilliseconds(finish_report_delay_ms_));
  RecordAdRedirectTriggerAction(AdRedirectTriggerAction::AD_REDIRECT);
}

void AdRedirectTrigger::OnDidBlockNavigation(const GURL& initiator_url) {
  RecordAdRedirectTriggerAction(AdRedirectTriggerAction::REDIRECT_CHECK);
  content::RenderFrameHost* initiator_frame =
      web_contents_->GetOriginalOpener();
  // Use focused frame as proxy if there is no opener.
  if (!initiator_frame)
    initiator_frame = web_contents_->GetFocusedFrame();
  if (!DetectGoogleAd(initiator_frame, initiator_url)) {
    RecordAdRedirectTriggerAction(
        AdRedirectTriggerAction::REDIRECT_NO_GOOGLE_AD);
    return;
  }
  // Create a report after a short delay.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AdRedirectTrigger::CreateAdRedirectReport,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(start_report_delay_ms_));
}

void AdRedirectTrigger::SetDelayForTest(int start_report_delay,
                                        int finish_report_delay) {
  start_report_delay_ms_ = start_report_delay;
  finish_report_delay_ms_ = finish_report_delay;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AdRedirectTrigger)
}  // namespace safe_browsing
