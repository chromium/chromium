// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/triggers/trigger_manager.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/safe_browsing/content/browser/threat_details.h"
#include "components/safe_browsing/content/browser/web_contents_key.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/core/unsafe_resource.h"
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
    case TriggerType::PHISHY_SITE_INTERACTION:
      // For phishy site interactions reporting, the user must already be
      // opted in before the trigger is allowed to run.
      return true;
    case TriggerType::DEPRECATED_AD_POPUP:
    case TriggerType::DEPRECATED_AD_REDIRECT:
      NOTREACHED_IN_MIGRATION() << "These triggers have been handled in "
                                   "CanStartDataCollectionWithReason()";
      return true;
  }
  // By default, require opt-in for all triggers.
  return true;
}

bool CanSendReport(const SBErrorOptions& error_display_options,
                   const TriggerType trigger_type) {
  // SafeBrowsingExtendedReportingOptInAllowed policy was deprecated.
  // trigger_manager will not depend on the is_extended_reporting_opt_in_allowed
  // value when the extended reporting is deprecated. We will remove the feature
  // flag check when the feature is fully rolled out.
  bool is_extended_reporting_opt_in_allowed =
      base::FeatureList::IsEnabled(kExtendedReportingRemovePrefDependency)
          ? true
          : error_display_options.is_extended_reporting_opt_in_allowed;
  // Reports are only sent for non-incoginito users who are allowed to modify
  // the Extended Reporting setting and have opted-in to Extended Reporting.
  return !error_display_options.is_off_the_record &&
         is_extended_reporting_opt_in_allowed &&
         error_display_options.is_extended_reporting_enabled;
}

}  // namespace

DataCollectorsContainer::DataCollectorsContainer() {}
DataCollectorsContainer::~DataCollectorsContainer() {}

TriggerManager::FinishCollectingThreatDetailsResult::
    FinishCollectingThreatDetailsResult(bool should_send_report,
                                        bool are_threat_details_available)
    : should_send_report(should_send_report),
      are_threat_details_available(are_threat_details_available) {}

bool TriggerManager::FinishCollectingThreatDetailsResult::IsReportSent() {
  return should_send_report && are_threat_details_available;
}

TriggerManager::TriggerManager(BaseUIManager* ui_manager,
                               PrefService* local_state_prefs)
    : ui_manager_(ui_manager),
      trigger_throttler_(new TriggerThrottler(local_state_prefs)) {}

TriggerManager::~TriggerManager() {}

void TriggerManager::set_trigger_throttler(TriggerThrottler* throttler) {
  trigger_throttler_.reset(throttler);
}

// static
SBErrorOptions TriggerManager::GetSBErrorDisplayOptions(
    const PrefService& pref_service,
    content::WebContents* web_contents) {
  return SBErrorOptions(
      /*is_main_frame_load_pending=*/false,
      IsExtendedReportingOptInAllowed(pref_service),
      web_contents->GetBrowserContext()->IsOffTheRecord(),
      IsExtendedReportingEnabledBypassDeprecationFlag(pref_service),
      IsExtendedReportingPolicyManaged(pref_service),
      IsEnhancedProtectionEnabled(pref_service),
      /*is_proceed_anyway_disabled=*/false,
      /*should_open_links_in_new_tab=*/false,
      /*always_show_back_to_safety=*/true,
      /*is_enhanced_protection_message_enabled=*/true,
      IsSafeBrowsingPolicyManaged(pref_service),
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
  if (trigger_type == TriggerType::DEPRECATED_AD_POPUP ||
      trigger_type == TriggerType::DEPRECATED_AD_REDIRECT) {
    *out_reason = TriggerManagerReason::REPORT_TYPE_DEPRECATED;
    return false;
  }

  *out_reason = TriggerManagerReason::NO_REASON;

  // Some triggers require that the user be opted-in to extended reporting in
  // order to run, while others can run without opt-in (eg: because users are
  // prompted for opt-in as part of the trigger).
  bool optin_required_check_ok =
      !TriggerNeedsOptInForCollection(trigger_type) ||
      error_display_options.is_extended_reporting_enabled;

  // SafeBrowsingExtendedReportingOptInAllowed policy was deprecated.
  // trigger_manager will not depend on the is_extended_reporting_opt_in_allowed
  // value when the extended reporting is deprecated. We will remove the feature
  // flag check when the feature is fully rolled out.
  bool is_extended_reporting_opt_in_allowed =
      base::FeatureList::IsEnabled(kExtendedReportingRemovePrefDependency)
          ? true
          : error_display_options.is_extended_reporting_opt_in_allowed;

  // We start data collection as long as user is not incognito, and the
  // |trigger_type| has available quota. For some triggers we also require
  // extended reporting opt-in in order to start data collection.
  if (!error_display_options.is_off_the_record &&
      is_extended_reporting_opt_in_allowed && optin_required_check_ok) {
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
    ReferrerChainProvider* referrer_chain_provider,
    const SBErrorOptions& error_display_options) {
  TriggerManagerReason unused_reason;
  return StartCollectingThreatDetailsWithReason(
      trigger_type, web_contents, resource, url_loader_factory, history_service,
      referrer_chain_provider, error_display_options, &unused_reason);
}

bool TriggerManager::StartCollectingThreatDetailsWithReason(
    const TriggerType trigger_type,
    content::WebContents* web_contents,
    const security_interstitials::UnsafeResource& resource,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service,
    ReferrerChainProvider* referrer_chain_provider,
    const SBErrorOptions& error_display_options,
    TriggerManagerReason* reason) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!CanStartDataCollectionWithReason(error_display_options, trigger_type,
                                        reason))
    return false;

  // Ensure we're not already collecting ThreatDetails on this tab. Create an
  // entry in the map for this |web_contents| if it's not there already.
  DataCollectorsContainer* collectors =
      &data_collectors_map_[GetWebContentsKey(web_contents)];
  if (collectors->threat_details) {
    return false;
  }

  bool should_trim_threat_details = trigger_type == TriggerType::AD_SAMPLE;
  collectors->threat_details = ThreatDetails::NewThreatDetails(
      ui_manager_, web_contents, resource, url_loader_factory, history_service,
      referrer_chain_provider, should_trim_threat_details,
      base::BindOnce(&TriggerManager::ThreatDetailsDone,
                     weak_factory_.GetWeakPtr()));
  return true;
}

void TriggerManager::SetInterstitialInteractions(
    std::unique_ptr<security_interstitials::InterstitialInteractionMap>
        interstitial_interactions) {
  interstitial_interactions_ = std::move(interstitial_interactions);
}

TriggerManager::FinishCollectingThreatDetailsResult
TriggerManager::FinishCollectingThreatDetails(
    const TriggerType trigger_type,
    WebContentsKey web_contents_key,
    const base::TimeDelta& delay,
    bool did_proceed,
    int num_visits,
    const SBErrorOptions& error_display_options,
    std::optional<int64_t> warning_shown_ts,
    bool is_hats_candidate) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Determine whether a report should be sent.
  bool should_send_report = CanSendReport(error_display_options, trigger_type);
  bool has_threat_details_in_map =
      base::Contains(data_collectors_map_, web_contents_key);

  if (should_send_report &&
      trigger_type == TriggerType::SECURITY_INTERSTITIAL) {
    base::UmaHistogramBoolean(
        "SafeBrowsing.ClientSafeBrowsingReport.HasThreatDetailsForTab."
        "SecurityInterstitial",
        has_threat_details_in_map);
  }

  // Make sure there's a ThreatDetails collector running on this tab.
  if (!has_threat_details_in_map)
    return FinishCollectingThreatDetailsResult(
        should_send_report,
        /*are_threat_details_available=*/false);
  DataCollectorsContainer* collectors = &data_collectors_map_[web_contents_key];
  bool has_threat_details = !!collectors->threat_details;

  if (!has_threat_details) {
    return FinishCollectingThreatDetailsResult(
        should_send_report,
        /*are_threat_details_available=*/false);
  }

  // Trigger finishing the ThreatDetails collection if we should send the
  // report to SB or if the user may see a HaTS survey.
  if (should_send_report || is_hats_candidate) {
    // Find the data collector and tell it to finish collecting data. We expect
    // it to notify us when it's finished so we can clean up references to it.
    collectors->threat_details->SetIsHatsCandidate(is_hats_candidate);
    collectors->threat_details->SetShouldSendReport(should_send_report);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ThreatDetails::FinishCollection,
                       collectors->threat_details->GetWeakPtr(), did_proceed,
                       num_visits, std::move(interstitial_interactions_),
                       warning_shown_ts),
        delay);

    // Record that this trigger fired and collected data.
    trigger_throttler_->TriggerFired(trigger_type);
  } else {
    // We aren't telling ThreatDetails to finish the report so we should clean
    // up our map ourselves.
    ThreatDetailsDone(web_contents_key);
  }

  return FinishCollectingThreatDetailsResult(
      should_send_report,
      /*are_threat_details_available=*/true);
}

void TriggerManager::ThreatDetailsDone(WebContentsKey web_contents_key) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Clean up the ThreatDetailsdata collector on the specified tab.
  if (!base::Contains(data_collectors_map_, web_contents_key)) {
    return;
  }

  DataCollectorsContainer* collectors = &data_collectors_map_[web_contents_key];
  collectors->threat_details = nullptr;
}

void TriggerManager::WebContentsDestroyed(content::WebContents* web_contents) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  WebContentsKey key = GetWebContentsKey(web_contents);
  if (!base::Contains(data_collectors_map_, key)) {
    return;
  }
  data_collectors_map_.erase(key);
}

TriggerManagerWebContentsHelper::TriggerManagerWebContentsHelper(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<TriggerManagerWebContentsHelper>(
          *web_contents),
      trigger_manager_(trigger_manager) {}

TriggerManagerWebContentsHelper::~TriggerManagerWebContentsHelper() {}

void TriggerManagerWebContentsHelper::WebContentsDestroyed() {
  trigger_manager_->WebContentsDestroyed(web_contents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TriggerManagerWebContentsHelper);

}  // namespace safe_browsing
