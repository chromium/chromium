// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/base_blocking_page.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/safe_browsing/content/browser/safe_browsing_controller_client.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/safe_browsing_loud_error_ui.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;
using security_interstitials::BaseSafeBrowsingErrorUI;
using security_interstitials::SafeBrowsingLoudErrorUI;
using security_interstitials::SecurityInterstitialControllerClient;

namespace safe_browsing {

namespace {

// After a safe browsing interstitial where the user opted-in to the report
// but clicked "proceed anyway", we delay the call to
// ThreatDetails::FinishCollection() by this much time (in
// milliseconds).
const int64_t kThreatDetailsProceedDelayMilliSeconds = 3000;

base::LazyInstance<BaseBlockingPage::UnsafeResourceMap>::Leaky
    g_unsafe_resource_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

BaseBlockingPage::BaseBlockingPage(
    BaseUIManager* ui_manager,
    WebContents* web_contents,
    const GURL& main_frame_url,
    const UnsafeResourceList& unsafe_resources,
    std::unique_ptr<SecurityInterstitialControllerClient> controller_client,
    const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options)
    : SecurityInterstitialPage(web_contents,
                               unsafe_resources[0].url,
                               std::move(controller_client)),
      ui_manager_(ui_manager),
      main_frame_url_(main_frame_url),
      navigation_entry_index_to_remove_(-1),
      unsafe_resources_(unsafe_resources),
      proceeded_(false),
      threat_details_proceed_delay_ms_(kThreatDetailsProceedDelayMilliSeconds),
      sb_error_ui_(std::make_unique<SafeBrowsingLoudErrorUI>(
          unsafe_resources_[0].url,
          main_frame_url_,
          GetInterstitialReason(unsafe_resources_),
          display_options,
          ui_manager->app_locale(),
          base::Time::NowFromSystemTime(),
          controller(),
          /* created_prior_to_navigation */
          IsMainPageLoadBlocked(unsafe_resources))) {}

BaseBlockingPage::~BaseBlockingPage() {}

// static
const security_interstitials::BaseSafeBrowsingErrorUI::SBErrorDisplayOptions
BaseBlockingPage::CreateDefaultDisplayOptions(
    const UnsafeResourceList& unsafe_resources) {
  return BaseSafeBrowsingErrorUI::SBErrorDisplayOptions(
      IsMainPageLoadBlocked(unsafe_resources),
      false,                 // kSafeBrowsingExtendedReportingOptInAllowed
      false,                 // is_off_the_record
      false,                 // is_extended_reporting
      false,                 // is_sber_policy_managed
      false,                 // is_enhanced_protection_enabled
      false,                 // kSafeBrowsingProceedAnywayDisabled
      false,                 // should_open_links_in_new_tab
      true,                  // always_show_back_to_safety
      false,                 // is_enhanced_protection_message_enabled
      false,                 // is_safe_browsing_managed
      "cpn_safe_browsing");  // help_center_article_link
}

// static
bool BaseBlockingPage::IsMainPageLoadBlocked(
    const UnsafeResourceList& unsafe_resources) {
  // If there is more than one unsafe resource, the main page load must not be
  // blocked. Otherwise, check if the one resource is.
  return unsafe_resources.size() == 1 &&
         unsafe_resources[0].IsMainPageLoadBlocked();
}

void BaseBlockingPage::SetThreatDetailsProceedDelayForTesting(int64_t delay) {
  threat_details_proceed_delay_ms_ = delay;
}

void BaseBlockingPage::CommandReceived(const std::string& page_cmd) {
  if (page_cmd == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int command = 0;
  bool retval = base::StringToInt(page_cmd, &command);
  DCHECK(retval) << page_cmd;
  auto interstitial_command =
      static_cast<security_interstitials::SecurityInterstitialCommand>(command);

  if (interstitial_command ==
      security_interstitials::SecurityInterstitialCommand::CMD_PROCEED) {
    // With committed interstitials, OnProceed() doesn't get called, so handle
    // adding to the allow list here.
    set_proceeded(true);
    ui_manager()->OnBlockingPageDone(unsafe_resources(), true /* proceed */,
                                     web_contents(), main_frame_url(),
                                     true /* showed_interstitial */);
  }

  sb_error_ui_->HandleCommand(interstitial_command);
}

void BaseBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  sb_error_ui_->PopulateStringsForHtml(load_time_data);
}

void BaseBlockingPage::FinishThreatDetails(const base::TimeDelta& delay,
                                           bool did_proceed,
                                           int num_visits) {}

// static
BaseBlockingPage::UnsafeResourceMap* BaseBlockingPage::GetUnsafeResourcesMap() {
  return g_unsafe_resource_map.Pointer();
}

// static
std::string BaseBlockingPage::GetMetricPrefix(
    const UnsafeResourceList& unsafe_resources,
    BaseSafeBrowsingErrorUI::SBInterstitialReason interstitial_reason) {
  bool primary_subresource = unsafe_resources[0].is_subresource;
  switch (interstitial_reason) {
    case BaseSafeBrowsingErrorUI::SB_REASON_MALWARE:
      return primary_subresource ? "malware_subresource" : "malware";
    case BaseSafeBrowsingErrorUI::SB_REASON_HARMFUL:
      return primary_subresource ? "harmful_subresource" : "harmful";
    case BaseSafeBrowsingErrorUI::SB_REASON_BILLING:
      return primary_subresource ? "billing_subresource" : "billing";
    case BaseSafeBrowsingErrorUI::SB_REASON_PHISHING:
      return primary_subresource ? "phishing_subresource" : "phishing";
  }
  NOTREACHED();
  return "unkown_metric_prefix";
}

// We populate a parallel set of metrics to differentiate some threat sources.
// static
std::string BaseBlockingPage::GetExtraMetricsSuffix(
    const UnsafeResourceList& unsafe_resources) {
  switch (unsafe_resources[0].threat_source) {
    case safe_browsing::ThreatSource::REMOTE:
      return "from_device";
    case safe_browsing::ThreatSource::LOCAL_PVER4:
      return "from_device_v4";
    case safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION:
      return "from_client_side_detection";
    case safe_browsing::ThreatSource::URL_REAL_TIME_CHECK:
      return "from_real_time_check";
    case safe_browsing::ThreatSource::NATIVE_PVER5_REAL_TIME:
      return "from_hash_prefix_real_time_check_v5";
    case safe_browsing::ThreatSource::ANDROID_SAFEBROWSING_REAL_TIME:
      return "from_android_safebrowsing_real_time";
    case safe_browsing::ThreatSource::ANDROID_SAFEBROWSING:
      return "from_android_safebrowsing";
    case safe_browsing::ThreatSource::UNKNOWN:
      break;
  }
  NOTREACHED();
  return std::string();
}

// static
security_interstitials::BaseSafeBrowsingErrorUI::SBInterstitialReason
BaseBlockingPage::GetInterstitialReason(
    const UnsafeResourceList& unsafe_resources) {
  bool harmful = false;
  for (auto iter = unsafe_resources.begin(); iter != unsafe_resources.end();
       ++iter) {
    const BaseUIManager::UnsafeResource& resource = *iter;
    safe_browsing::SBThreatType threat_type = resource.threat_type;
    if (threat_type == SB_THREAT_TYPE_BILLING)
      return BaseSafeBrowsingErrorUI::SB_REASON_BILLING;

    if (threat_type == SB_THREAT_TYPE_URL_MALWARE ||
        threat_type == SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE) {
      return BaseSafeBrowsingErrorUI::SB_REASON_MALWARE;
    }

    if (threat_type == SB_THREAT_TYPE_URL_UNWANTED) {
      harmful = true;
    } else {
      DCHECK(threat_type == SB_THREAT_TYPE_URL_PHISHING ||
             threat_type == SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING);
    }
  }

  if (harmful)
    return BaseSafeBrowsingErrorUI::SB_REASON_HARMFUL;
  return BaseSafeBrowsingErrorUI::SB_REASON_PHISHING;
}

BaseUIManager* BaseBlockingPage::ui_manager() const {
  return ui_manager_;
}

const GURL BaseBlockingPage::main_frame_url() const {
  return main_frame_url_;
}

BaseBlockingPage::UnsafeResourceList BaseBlockingPage::unsafe_resources()
    const {
  return unsafe_resources_;
}

bool BaseBlockingPage::proceeded() const {
  return proceeded_;
}

int64_t BaseBlockingPage::threat_details_proceed_delay() const {
  return threat_details_proceed_delay_ms_;
}

BaseSafeBrowsingErrorUI* BaseBlockingPage::sb_error_ui() const {
  return sb_error_ui_.get();
}

void BaseBlockingPage::set_proceeded(bool proceeded) {
  proceeded_ = proceeded;
}

// static
security_interstitials::MetricsHelper::ReportDetails
BaseBlockingPage::GetReportingInfo(const UnsafeResourceList& unsafe_resources) {
  BaseSafeBrowsingErrorUI::SBInterstitialReason interstitial_reason =
      GetInterstitialReason(unsafe_resources);

  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix =
      GetMetricPrefix(unsafe_resources, interstitial_reason);
  reporting_info.extra_suffix = GetExtraMetricsSuffix(unsafe_resources);
  return reporting_info;
}

// static
std::unique_ptr<SecurityInterstitialControllerClient>
BaseBlockingPage::CreateControllerClient(
    content::WebContents* web_contents,
    const UnsafeResourceList& unsafe_resources,
    BaseUIManager* ui_manager,
    PrefService* pref_service,
    std::unique_ptr<security_interstitials::SettingsPageHelper>
        settings_page_helper) {
  history::HistoryService* history_service =
      ui_manager->history_service(web_contents);

  std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper =
      std::make_unique<security_interstitials::MetricsHelper>(
          unsafe_resources[0].url, GetReportingInfo(unsafe_resources),
          history_service);

  return std::make_unique<SafeBrowsingControllerClient>(
      web_contents, std::move(metrics_helper), pref_service,
      ui_manager->app_locale(), ui_manager->default_safe_page(),
      std::move(settings_page_helper));
}

int BaseBlockingPage::GetHTMLTemplateId() {
  return sb_error_ui_->GetHTMLTemplateId();
}

void BaseBlockingPage::set_sb_error_ui(
    std::unique_ptr<BaseSafeBrowsingErrorUI> sb_error_ui) {
  sb_error_ui_ = std::move(sb_error_ui);
}

void BaseBlockingPage::OnDontProceedDone() {
  if (!sb_error_ui_->is_proceed_anyway_disabled()) {
    controller()->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::DONT_PROCEED);
  }

  ui_manager_->OnBlockingPageDone(unsafe_resources_, false /* proceed */,
                                  web_contents(), main_frame_url_,
                                  true /* showed_interstitial */);

  // The user does not want to proceed, clear the queued unsafe resources
  // notifications we received while the interstitial was showing.
  UnsafeResourceMap* unsafe_resource_map = GetUnsafeResourcesMap();
  auto iter = unsafe_resource_map->find(web_contents());
  if (iter != unsafe_resource_map->end() && !iter->second.empty()) {
    ui_manager_->OnBlockingPageDone(iter->second, false, web_contents(),
                                    main_frame_url_,
                                    true /* showed_interstitial */);
    unsafe_resource_map->erase(iter);
  }

  // We don't remove the navigation entry if the tab is being destroyed as this
  // would trigger a navigation that would cause trouble as the render view host
  // for the tab has by then already been destroyed.  We also don't delete the
  // current entry if it has been committed again, which is possible on a page
  // that had a subresource warning.
  const int last_committed_index =
      web_contents()->GetController().GetLastCommittedEntryIndex();
  if (navigation_entry_index_to_remove_ != -1 &&
      navigation_entry_index_to_remove_ != last_committed_index &&
      !web_contents()->IsBeingDestroyed()) {
    CHECK(web_contents()->GetController().RemoveEntryAtIndex(
        navigation_entry_index_to_remove_));
  }
}

// static
bool BaseBlockingPage::ShouldReportThreatDetails(SBThreatType threat_type) {
  return threat_type == SB_THREAT_TYPE_BILLING ||
         threat_type == SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE ||
         threat_type == SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING ||
         threat_type == SB_THREAT_TYPE_URL_MALWARE ||
         threat_type == SB_THREAT_TYPE_URL_PHISHING ||
         threat_type == SB_THREAT_TYPE_URL_UNWANTED;
}

}  // namespace safe_browsing
