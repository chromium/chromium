// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the SafeBrowsingBlockingPage class.

#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"

#include <memory>

#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_report_util.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/content/browser/threat_details.h"
#include "components/safe_browsing/content/browser/triggers/trigger_manager.h"
#include "components/safe_browsing/content/browser/unsafe_resource_util.h"
#include "components/safe_browsing/content/browser/web_contents_key.h"
#include "components/safe_browsing/core/browser/safe_browsing_hats_delegate.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/safe_browsing_loud_error_ui.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

using content::BrowserThread;
using content::WebContents;
using security_interstitials::BaseSafeBrowsingErrorUI;
using security_interstitials::SecurityInterstitialControllerClient;

namespace safe_browsing {

// static
const security_interstitials::SecurityInterstitialPage::TypeID
    SafeBrowsingBlockingPage::kTypeForTesting =
        &SafeBrowsingBlockingPage::kTypeForTesting;

SafeBrowsingBlockingPage::SafeBrowsingBlockingPage(
    BaseUIManager* ui_manager,
    WebContents* web_contents,
    const GURL& main_frame_url,
    const UnsafeResourceList& unsafe_resources,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client,
    const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
    bool should_trigger_reporting,
    history::HistoryService* history_service,
    SafeBrowsingNavigationObserverManager* navigation_observer_manager,
    SafeBrowsingMetricsCollector* metrics_collector,
    TriggerManager* trigger_manager,
    bool is_proceed_anyway_disabled,
    bool is_safe_browsing_surveys_enabled,
    base::OnceCallback<void(bool, SBThreatType)>
        trust_safety_sentiment_service_trigger,
    base::OnceCallback<void(bool, SBThreatType)>
        ignore_auto_revocation_notifications_trigger,
    network::SharedURLLoaderFactory* url_loader_for_testing)
    : BaseBlockingPage(ui_manager,
                       web_contents,
                       main_frame_url,
                       unsafe_resources,
                       std::move(controller_client),
                       display_options),
      threat_details_in_progress_(false),
      threat_source_(unsafe_resources[0].threat_source),
      threat_type_(unsafe_resources[0].threat_type),
      history_service_(history_service),
      navigation_observer_manager_(navigation_observer_manager),
      metrics_collector_(metrics_collector),
      trigger_manager_(trigger_manager),
      is_proceed_anyway_disabled_(is_proceed_anyway_disabled),
      is_safe_browsing_surveys_enabled_(is_safe_browsing_surveys_enabled),
      trust_safety_sentiment_service_trigger_(
          std::move(trust_safety_sentiment_service_trigger)),
      ignore_auto_revocation_notifications_trigger_(
          std::move(ignore_auto_revocation_notifications_trigger)) {
  if (unsafe_resources.size() == 1) {
    UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.BlockingPage.ThreatType",
                              unsafe_resources[0].threat_type);
  }
  LogSafeBrowsingInterstitialShownUKM(web_contents);

  if (metrics_collector_) {
    metrics_collector_->AddSafeBrowsingEventToPref(
        SafeBrowsingMetricsCollector::EventType::
            SECURITY_SENSITIVE_SAFE_BROWSING_INTERSTITIAL);
  }

  if (!trigger_manager_) {
    return;
  }

  // Start computing threat details. Trigger Manager will decide if it's safe to
  // begin collecting data at this time. The report will be sent only if the
  // user opts-in on the blocking page later.
  // If there's more than one malicious resources, it means the user clicked
  // through the first warning, so we don't prepare additional reports.
  if (unsafe_resources.size() == 1 &&
      ShouldReportThreatDetails(unsafe_resources[0].threat_type)) {
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        url_loader_for_testing ? url_loader_for_testing
                               : web_contents->GetBrowserContext()
                                     ->GetDefaultStoragePartition()
                                     ->GetURLLoaderFactoryForBrowserProcess();
    if (should_trigger_reporting) {
      threat_details_in_progress_ =
          trigger_manager_->StartCollectingThreatDetails(
              TriggerType::SECURITY_INTERSTITIAL, web_contents,
              unsafe_resources[0], url_loader_factory, history_service_,
              navigation_observer_manager_,
              sb_error_ui()->get_error_display_options());
      warning_shown_ts_ = base::Time::Now().InMillisecondsSinceUnixEpoch();
    }
  }
}

SafeBrowsingBlockingPage::~SafeBrowsingBlockingPage() {}

security_interstitials::SecurityInterstitialPage::TypeID
SafeBrowsingBlockingPage::GetTypeForTesting() {
  return SafeBrowsingBlockingPage::kTypeForTesting;
}

void SafeBrowsingBlockingPage::OnInterstitialClosing() {
  interstitial_interactions_ =
      sb_error_ui()->get_interstitial_interaction_data();
  // If this is a phishing interstitial and the user did not make a decision
  // through the UI, record that interaction in UMA
  if (!sb_error_ui()->did_user_make_decision()) {
    controller()->metrics_helper()->RecordUserInteraction(
        security_interstitials::MetricsHelper::CLOSE_INTERSTITIAL_WITHOUT_UI);

    // Add CMD_CLOSE_INTERSTITIAL_WITHOUT_UI interaction to interactions.
    if (interstitial_interactions_) {
      interstitial_interactions_->insert_or_assign(
          security_interstitials::SecurityInterstitialCommand::
              CMD_CLOSE_INTERSTITIAL_WITHOUT_UI,
          security_interstitials::InterstitialInteractionDetails(
              1, base::Time::Now().InMillisecondsSinceUnixEpoch(),
              base::Time::Now().InMillisecondsSinceUnixEpoch()));
    }
  }

  // Log UKM if the user bypassed the interstitial.
  if (proceeded()) {
    LogSafeBrowsingInterstitialBypassedUKM(web_contents());
  }

  // If the user proceeded past a social engineering threat interstitial,
  // ignore the origin in future auto-revocation of abusive notifications.
  if (ignore_auto_revocation_notifications_trigger_) {
    DCHECK(base::FeatureList::IsEnabled(
        safe_browsing::kSafetyHubAbusiveNotificationRevocation));
    std::move(ignore_auto_revocation_notifications_trigger_)
        .Run(proceeded(), threat_type_);
  }

  // With committed interstitials OnProceed and OnDontProceed don't get
  // called, so call FinishThreatDetails from here.
  FinishThreatDetails(
      (proceeded() ? base::Milliseconds(threat_details_proceed_delay())
                   : base::TimeDelta()),
      proceeded(), controller()->metrics_helper()->NumVisits());
  if (!proceeded()) {
    OnDontProceedDone();
  } else {
    if (metrics_collector_) {
      metrics_collector_->AddBypassEventToPref(threat_source_);
    }
  }

#if !BUILDFLAG(IS_ANDROID)
  if (trust_safety_sentiment_service_trigger_) {
    std::move(trust_safety_sentiment_service_trigger_)
        .Run(proceeded(), threat_type_);
  }
#endif

  BaseBlockingPage::OnInterstitialClosing();
}

void SafeBrowsingBlockingPage::SendFallbackReport(
    const security_interstitials::UnsafeResource resource,
    bool did_proceed,
    int num_visits,
    security_interstitials::InterstitialInteractionMap* interactions,
    bool is_hats_candidate) {
  auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
  client_report_utils::FillReportBasicResourceDetails(report.get(), resource);
  report->set_did_proceed(did_proceed);
  if (num_visits >= 0) {
    report->set_repeat_visit(num_visits > 0);
  }
  if (report->type() == ClientSafeBrowsingReportRequest::URL_PHISHING ||
      report->type() ==
          ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_PHISHING) {
    client_report_utils::FillInterstitialInteractionsHelper(report.get(),
                                                            interactions);
  }
  if (base::FeatureList::IsEnabled(
          safe_browsing::kAddWarningShownTSToClientSafeBrowsingReport)) {
    report->set_warning_shown_timestamp_msec(warning_shown_ts_);
  }
  ui_manager()->SendThreatDetails(web_contents()->GetBrowserContext(),
                                  std::move(report));
}

void SafeBrowsingBlockingPage::FinishThreatDetails(const base::TimeDelta& delay,
                                                   bool did_proceed,
                                                   int num_visits) {
  base::UmaHistogramBoolean(
      "SafeBrowsing.ClientSafeBrowsingReport.HasThreatDetailsAtFinish."
      "Mainframe",
      threat_details_in_progress_);
  // Not all interstitials collect threat details (eg., incognito mode).
  if (!threat_details_in_progress_) {
    return;
  }

  if (!trigger_manager_) {
    return;
  }

  // In case the report cannot get send through trigger manager, save the
  // interstitial interactions locally before moving unique ptr to trigger
  // manager.
  security_interstitials::InterstitialInteractionMap local_interactions;
  if (interstitial_interactions_) {
    local_interactions = *interstitial_interactions_;
  }

  // Finish computing threat details. TriggerManager will decide if its safe to
  // send the report.
  trigger_manager_->SetInterstitialInteractions(
      std::move(interstitial_interactions_));
  bool is_hats_candidate = false;
  if (base::FeatureList::IsEnabled(kRedWarningSurvey)) {
    is_hats_candidate =
        SafeBrowsingHatsDelegate::IsSurveyCandidate(
            threat_type_, kRedWarningSurveyReportTypeFilter.Get(), proceeded(),
            kRedWarningSurveyDidProceedFilter.Get()) &&
        !is_proceed_anyway_disabled_ && is_safe_browsing_surveys_enabled_;
  }
  auto report_sent_result = trigger_manager_->FinishCollectingThreatDetails(
      TriggerType::SECURITY_INTERSTITIAL, GetWebContentsKey(web_contents()),
      delay, did_proceed, num_visits,
      sb_error_ui()->get_error_display_options(), warning_shown_ts_,
      is_hats_candidate);
  if (!report_sent_result.are_threat_details_available &&
      report_sent_result.should_send_report && unsafe_resources().size() == 1) {
    // If reports are not sent because threat details are not available, send a
    // fallback report without information from threat details instead.
    SendFallbackReport(unsafe_resources()[0], did_proceed, num_visits,
                       &local_interactions, is_hats_candidate);
  }

  if (report_sent_result.should_send_report) {
    controller()->metrics_helper()->RecordUserInteraction(
        security_interstitials::MetricsHelper::EXTENDED_REPORTING_IS_ENABLED);
  }
}

void SafeBrowsingBlockingPage::LogSafeBrowsingInterstitialBypassedUKM(
    content::WebContents* web_contents) {
  ukm::SourceId source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  ukm::builders::SafeBrowsingInterstitial(source_id).SetBypassed(true).Record(
      ukm::UkmRecorder::Get());
}

void SafeBrowsingBlockingPage::LogSafeBrowsingInterstitialShownUKM(
    content::WebContents* web_contents) {
  ukm::SourceId source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  ukm::builders::SafeBrowsingInterstitial(source_id).SetShown(true).Record(
      ukm::UkmRecorder::Get());
}

}  // namespace safe_browsing
