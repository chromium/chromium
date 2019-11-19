// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/ad_popup_trigger.h"

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
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace safe_browsing {

namespace {

// Number of milliseconds to allow data collection to run before sending a
// report (since this trigger runs in the background).
const int64_t kAdPopupCollectionPeriodMilliseconds = 5000;

// Range of number of milliseconds to wait after a page finished loading before
// starting a report. Allows ads which load in the background to finish loading.
const int64_t kMaxAdPopupCollectionStartDelayMilliseconds = 5000;
const int64_t kMinAdPopupCollectionStartDelayMilliseconds = 500;

void RecordAdPopupTriggerAction(AdPopupTriggerAction action) {
  UMA_HISTOGRAM_ENUMERATION(kAdPopupTriggerActionMetricName, action);
}

}  // namespace

// Metric for tracking what the Ad Popup trigger does on each navigation.
const char kAdPopupTriggerActionMetricName[] =
    "SafeBrowsing.Triggers.AdPopup.Action";

AdPopupTrigger::AdPopupTrigger(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager,
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service)
    : web_contents_(web_contents),
      start_report_delay_ms_(
          base::RandInt(kMinAdPopupCollectionStartDelayMilliseconds,
                        kMaxAdPopupCollectionStartDelayMilliseconds)),
      finish_report_delay_ms_(kAdPopupCollectionPeriodMilliseconds),
      trigger_manager_(trigger_manager),
      prefs_(prefs),
      url_loader_factory_(url_loader_factory),
      history_service_(history_service),
      task_runner_(
          base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})) {}

AdPopupTrigger::~AdPopupTrigger() {}

// static
void AdPopupTrigger::CreateForWebContents(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager,
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service) {
  DCHECK(web_contents);
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(UserDataKey(),
                              base::WrapUnique(new AdPopupTrigger(
                                  web_contents, trigger_manager, prefs,
                                  url_loader_factory, history_service)));
  }
}

void AdPopupTrigger::CreateAdPopupReport() {
  SBErrorOptions error_options =
      TriggerManager::GetSBErrorDisplayOptions(*prefs_, web_contents_);
  security_interstitials::UnsafeResource resource;
  resource.threat_type = SB_THREAT_TYPE_BLOCKED_AD_POPUP;
  resource.url = web_contents_->GetURL();
  resource.web_contents_getter = resource.GetWebContentsGetter(
      web_contents_->GetMainFrame()->GetProcess()->GetID(),
      web_contents_->GetMainFrame()->GetRoutingID());
  TriggerManagerReason reason = TriggerManagerReason::NO_REASON;
  if (!trigger_manager_->StartCollectingThreatDetailsWithReason(
          TriggerType::AD_POPUP, web_contents_, resource, url_loader_factory_,
          history_service_, error_options, &reason)) {
    if (reason == TriggerManagerReason::DAILY_QUOTA_EXCEEDED) {
      RecordAdPopupTriggerAction(
          AdPopupTriggerAction::POPUP_DAILY_QUOTA_EXCEEDED);
    } else {
      RecordAdPopupTriggerAction(
          AdPopupTriggerAction::POPUP_COULD_NOT_START_REPORT);
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
          base::Unretained(trigger_manager_), TriggerType::AD_POPUP,
          base::Unretained(web_contents_), base::TimeDelta(),
          /*did_proceed=*/false, /*num_visits=*/0, error_options),
      base::TimeDelta::FromMilliseconds(finish_report_delay_ms_));

  RecordAdPopupTriggerAction(AdPopupTriggerAction::POPUP_REPORTED);
}

void AdPopupTrigger::PopupWasBlocked(content::RenderFrameHost* render_frame) {
  RecordAdPopupTriggerAction(AdPopupTriggerAction::POPUP_CHECK);
  if (!DetectGoogleAd(render_frame, web_contents_->GetURL())) {
    RecordAdPopupTriggerAction(AdPopupTriggerAction::POPUP_NO_GOOGLE_AD);
    return;
  }
  // Create a report after a short delay. The delay gives more time for ads to
  // finish loading in the background. This is best-effort.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AdPopupTrigger::CreateAdPopupReport,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(start_report_delay_ms_));
}

void AdPopupTrigger::SetTaskRunnerForTest(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AdPopupTrigger)

}  // namespace safe_browsing
