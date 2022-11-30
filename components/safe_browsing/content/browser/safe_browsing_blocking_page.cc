// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the SafeBrowsingBlockingPage class.

#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"

#include <memory>

#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/content/browser/threat_details.h"
#include "components/safe_browsing/content/browser/triggers/trigger_manager.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

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
    network::SharedURLLoaderFactory* url_loader_for_testing)
    : BaseBlockingPage(ui_manager,
                       web_contents,
                       main_frame_url,
                       unsafe_resources,
                       std::move(controller_client),
                       display_options),
      threat_details_in_progress_(false),
      threat_source_(unsafe_resources[0].threat_source),
      history_service_(history_service),
      navigation_observer_manager_(navigation_observer_manager),
      metrics_collector_(metrics_collector),
      trigger_manager_(trigger_manager) {
  if (unsafe_resources.size() == 1) {
    UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.BlockingPage.RequestDestination",
                              unsafe_resources[0].request_destination);
  }

  if (metrics_collector_) {
    metrics_collector_->AddSafeBrowsingEventToPref(
        SafeBrowsingMetricsCollector::EventType::
            SECURITY_SENSITIVE_SAFE_BROWSING_INTERSTITIAL);
  }

  if (!trigger_manager_)
    return;

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
    }
  }
}

SafeBrowsingBlockingPage::~SafeBrowsingBlockingPage() {}

security_interstitials::SecurityInterstitialPage::TypeID
SafeBrowsingBlockingPage::GetTypeForTesting() {
  return SafeBrowsingBlockingPage::kTypeForTesting;
}

void SafeBrowsingBlockingPage::OnInterstitialClosing() {
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
  BaseBlockingPage::OnInterstitialClosing();
}

void SafeBrowsingBlockingPage::FinishThreatDetails(const base::TimeDelta& delay,
                                                   bool did_proceed,
                                                   int num_visits) {
  // Not all interstitials collect threat details (eg., incognito mode).
  if (!threat_details_in_progress_)
    return;

  if (!trigger_manager_)
    return;

  // Finish computing threat details. TriggerManager will decide if its safe to
  // send the report.
  bool report_sent = trigger_manager_->FinishCollectingThreatDetails(
      TriggerType::SECURITY_INTERSTITIAL, web_contents(), delay, did_proceed,
      num_visits, sb_error_ui()->get_error_display_options());

  if (report_sent) {
    controller()->metrics_helper()->RecordUserInteraction(
        security_interstitials::MetricsHelper::EXTENDED_REPORTING_IS_ENABLED);
  }
}

}  // namespace safe_browsing
