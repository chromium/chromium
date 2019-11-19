// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/trigger_manager.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing/browser/threat_details.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {

bool TriggerNeedsOptInForCollection(const TriggerType trigger_type) {
  switch (trigger_type) {
    case TriggerType::SECURITY_INTERSTITIAL:
      // For security interstitials, users can change the opt-in while the
      // trigger runs, so collection can begin without opt-in.
      return false;
    case TriggerType::AD_POPUP:
    case TriggerType::AD_REDIRECT:
    case TriggerType::AD_SAMPLE:
      // Ad samples happen in the background so the user must already be opted
      // in before the trigger is allowed to run.
      return true;
    case TriggerType::GAIA_PASSWORD_REUSE:
      // For Gaia password reuses, it is unlikely for users to change opt-in
      // while the trigger runs, so we require opt-in for collection to avoid
      // overheads.
      return true;
    case TriggerType::SUSPICIOUS_SITE:
      // Suspicious site collection happens in the background so the user must
      // already be opted in before the trigger is allowed to run.
      return true;
    case TriggerType::APK_DOWNLOAD:
      // APK download collection happens in the background so the user must
      // already be opted in before the trigger is allowed to run.
      return true;
  }
  // By default, require opt-in for all triggers.
  return true;
}

bool CanSendReport(const SBErrorOptions& error_display_options,
                   const TriggerType trigger_type) {
  // Reports are only sent for non-incoginito users who are allowed to modify
  // the Extended Reporting setting and have opted-in to Extended Reporting.
  return !error_display_options.is_off_the_record &&
         error_display_options.is_extended_reporting_opt_in_allowed &&
         error_display_options.is_extended_reporting_enabled;
}

}  // namespace

DataCollectorsContainer::DataCollectorsContainer() {}
DataCollectorsContainer::~DataCollectorsContainer() {}

TriggerManager::TriggerManager(BaseUIManager* ui_manager,
                               ReferrerChainProvider* referrer_chain_provider,
                               PrefService* local_state_prefs)
    : ui_manager_(ui_manager),
      referrer_chain_provider_(referrer_chain_provider),
      trigger_throttler_(new TriggerThrottler(local_state_prefs)) {}

TriggerManager::~TriggerManager() {}

void TriggerManager::set_trigger_throttler(TriggerThrottler* throttler) {
  trigger_throttler_.reset(throttler);
}

// static
SBErrorOptions TriggerManager::GetSBErrorDisplayOptions(
    const PrefService& pref_service,
    content::WebContents* web_contents) {
  return SBErrorOptions(/*is_main_frame_load_blocked=*/false,
                        IsExtendedReportingOptInAllowed(pref_service),
                        web_contents->GetBrowserContext()->IsOffTheRecord(),
                        IsExtendedReportingEnabled(pref_service),
                        IsExtendedReportingPolicyManaged(pref_service),
                        /*is_proceed_anyway_disabled=*/false,
                        /*should_open_links_in_new_tab=*/false,
                        /*show_back_to_safety_button=*/true,
                        /*help_center_article_link=*/std::string());
}

bool TriggerManager::CanStartDataCollection(
    const SBErrorOptions& error_display_options,
    const TriggerType trigger_type) {
  TriggerManagerReason unused_reason;
  return CanStartDataCollectionWithReason(error_display_options, trigger_type,
                                          &unused_reason);
}

bool TriggerManager::CanStartDataCollectionWithReason(
    const SBErrorOptions& error_display_options,
    const TriggerType trigger_type,
    TriggerManagerReason* out_reason) {
  *out_reason = TriggerManagerReason::NO_REASON;

  // Some triggers require that the user be opted-in to extended reporting in
  // order to run, while others can run without opt-in (eg: because users are
  // prompted for opt-in as part of the trigger).
  bool optin_required_check_ok =
      !TriggerNeedsOptInForCollection(trigger_type) ||
      error_display_options.is_extended_reporting_enabled;
  // We start data collection as long as user is not incognito and is able to
  // change the Extended Reporting opt-in, and the |trigger_type| has available
  // quota. For some triggers we also require extended reporting opt-in in
  // order to start data collection.
  if (!error_display_options.is_off_the_record &&
      error_display_options.is_extended_reporting_opt_in_allowed &&
      optin_required_check_ok) {
    bool quota_ok = trigger_throttler_->TriggerCanFire(trigger_type);
    if (!quota_ok)
      *out_reason = TriggerManagerReason::DAILY_QUOTA_EXCEEDED;
    return quota_ok;
  } else {
    *out_reason = TriggerManagerReason::USER_PREFERENCES;
    return false;
  }
}

bool TriggerManager::StartCollectingThreatDetails(
    const TriggerType trigger_type,
    content::WebContents* web_contents,
    const security_interstitials::UnsafeResource& resource,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service,
    const SBErrorOptions& error_display_options) {
  TriggerManagerReason unused_reason;
  return StartCollectingThreatDetailsWithReason(
      trigger_type, web_contents, resource, url_loader_factory, history_service,
      error_display_options, &unused_reason);
}

bool TriggerManager::StartCollectingThreatDetailsWithReason(
    const TriggerType trigger_type,
    content::WebContents* web_contents,
    const security_interstitials::UnsafeResource& resource,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service,
    const SBErrorOptions& error_display_options,
    TriggerManagerReason* reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!CanStartDataCollectionWithReason(error_display_options, trigger_type,
                                        reason))
    return false;

  // Ensure we're not already collecting ThreatDetails on this tab. Create an
  // entry in the map for this |web_contents| if it's not there already.
  DataCollectorsContainer* collectors = &data_collectors_map_[web_contents];
  if (collectors->threat_details != nullptr)
    return false;

  bool should_trim_threat_details = (trigger_type == TriggerType::AD_POPUP ||
                                     trigger_type == TriggerType::AD_SAMPLE ||
                                     trigger_type == TriggerType::AD_REDIRECT);
  collectors->threat_details = ThreatDetails::NewThreatDetails(
      ui_manager_, web_contents, resource, url_loader_factory, history_service,
      referrer_chain_provider_, should_trim_threat_details,
      base::Bind(&TriggerManager::ThreatDetailsDone,
                 weak_factory_.GetWeakPtr()));
  return true;
}

bool TriggerManager::FinishCollectingThreatDetails(
    const TriggerType trigger_type,
    content::WebContents* web_contents,
    const base::TimeDelta& delay,
    bool did_proceed,
    int num_visits,
    const SBErrorOptions& error_display_options) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Make sure there's a ThreatDetails collector running on this tab.
  if (!base::Contains(data_collectors_map_, web_contents))
    return false;
  DataCollectorsContainer* collectors = &data_collectors_map_[web_contents];
  if (collectors->threat_details == nullptr)
    return false;

  // Determine whether a report should be sent.
  bool should_send_report = CanSendReport(error_display_options, trigger_type);

  if (should_send_report) {
    // Find the data collector and tell it to finish collecting data. We expect
    // it to notify us when it's finished so we can clean up references to it.

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ThreatDetails::FinishCollection,
                       collectors->threat_details->GetWeakPtr(), did_proceed,
                       num_visits),
        delay);

    // Record that this trigger fired and collected data.
    trigger_throttler_->TriggerFired(trigger_type);
  } else {
    // We aren't telling ThreatDetails to finish the report so we should clean
    // up our map ourselves.
    ThreatDetailsDone(web_contents);
  }

  return should_send_report;
}

void TriggerManager::ThreatDetailsDone(content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Clean up the ThreatDetailsdata collector on the specified tab.
  if (!base::Contains(data_collectors_map_, web_contents))
    return;

  DataCollectorsContainer* collectors = &data_collectors_map_[web_contents];
  collectors->threat_details = nullptr;
}

void TriggerManager::WebContentsDestroyed(content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!base::Contains(data_collectors_map_, web_contents))
    return;
  data_collectors_map_.erase(web_contents);
}

TriggerManagerWebContentsHelper::TriggerManagerWebContentsHelper(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager)
    : content::WebContentsObserver(web_contents),
      trigger_manager_(trigger_manager) {}

TriggerManagerWebContentsHelper::~TriggerManagerWebContentsHelper() {}

void TriggerManagerWebContentsHelper::CreateForWebContents(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager) {
  DCHECK(web_contents);
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(), base::WrapUnique(new TriggerManagerWebContentsHelper(
                           web_contents, trigger_manager)));
  }
}

void TriggerManagerWebContentsHelper::WebContentsDestroyed() {
  trigger_manager_->WebContentsDestroyed(web_contents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TriggerManagerWebContentsHelper)

}  // namespace safe_browsing
