// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_BASE_BLOCKING_PAGE_H_
#define COMPONENTS_SAFE_BROWSING_BASE_BLOCKING_PAGE_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "url/gurl.h"

namespace safe_browsing {

// Base class for managing the SafeBrowsing interstitial pages.
class BaseBlockingPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  typedef security_interstitials::UnsafeResource UnsafeResource;
  typedef security_interstitials::BaseSafeBrowsingErrorUI
      BaseSafeBrowsingErrorUI;
  typedef std::vector<UnsafeResource> UnsafeResourceList;
  typedef std::unordered_map<content::WebContents*, UnsafeResourceList>
      UnsafeResourceMap;

  ~BaseBlockingPage() override;

  static const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions
  CreateDefaultDisplayOptions(const UnsafeResourceList& unsafe_resources);

  // Shows a blocking page warning the user about phishing/malware for a
  // specific resource.
  // This can be called several times. If an interstitial is already showing
  // and the user decides to proceed, it will be discarded and a new one will be
  // displayed.
  static void ShowBlockingPage(BaseUIManager* ui_manager,
                               const UnsafeResource& resource);

  // Returns true if the passed |unsafe_resources| is blocking the load of
  // the main page.
  static bool IsMainPageLoadBlocked(const UnsafeResourceList& unsafe_resources);

  // InterstitialPageDelegate methods:
  void OnProceed() override;
  void OnDontProceed() override;
  void CommandReceived(const std::string& command) override;

  // Checks the threat type to decide if we should report ThreatDetails.
  static bool ShouldReportThreatDetails(SBThreatType threat_type);

 protected:
  // Don't instantiate this class directly, use ShowBlockingPage instead.
  BaseBlockingPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const UnsafeResourceList& unsafe_resources,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client,
      const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options);

  // SecurityInterstitialPage methods:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) override;
  void OnInterstitialClosing() override;

  // Called when the interstitial is going away. Intentionally do nothing in
  // this base class.
  virtual void FinishThreatDetails(const base::TimeDelta& delay,
                                   bool did_proceed,
                                   int num_visits);

  // A list of SafeBrowsingUIManager::UnsafeResource for a tab that the user
  // should be warned about. They are queued when displaying more than one
  // interstitial at a time.
  static UnsafeResourceMap* GetUnsafeResourcesMap();

  static std::string GetMetricPrefix(
      const UnsafeResourceList& unsafe_resources,
      BaseSafeBrowsingErrorUI::SBInterstitialReason interstitial_reason);

  static std::string GetExtraMetricsSuffix(
      const UnsafeResourceList& unsafe_resources);

  // Return the most severe interstitial reason from a list of unsafe resources.
  // Severity ranking: malware > UwS (harmful) > phishing.
  static BaseSafeBrowsingErrorUI::SBInterstitialReason GetInterstitialReason(
      const UnsafeResourceList& unsafe_resources);

  BaseUIManager* ui_manager() const;

  const GURL main_frame_url() const;

  UnsafeResourceList unsafe_resources() const;

  bool proceeded() const;

  int64_t threat_details_proceed_delay() const;

  BaseSafeBrowsingErrorUI* sb_error_ui() const;

  void set_proceeded(bool proceeded);

  static security_interstitials::MetricsHelper::ReportDetails GetReportingInfo(
      const UnsafeResourceList& unsafe_resources);

  // Called after OnProceed(). Does nothing in this class, but can be overridden
  // to handle malicious subresources.
  virtual void HandleSubresourcesAfterProceed();

  void SetThreatDetailsProceedDelayForTesting(int64_t delay);

  static std::unique_ptr<
      security_interstitials::SecurityInterstitialControllerClient>
  CreateControllerClient(content::WebContents* web_contents,
                         const UnsafeResourceList& unsafe_resources,
                         BaseUIManager* ui_manager,
                         PrefService* pref_service);

  int GetHTMLTemplateId() override;

  void set_sb_error_ui(std::unique_ptr<BaseSafeBrowsingErrorUI> sb_error_ui);

  void OnDontProceedDone();

 private:
  // For reporting back user actions.
  BaseUIManager* ui_manager_;

  // The URL of the main frame that caused the warning.
  GURL main_frame_url_;

  // The index of a navigation entry that should be removed when DontProceed()
  // is invoked, -1 if entry should not be removed.
  const int navigation_entry_index_to_remove_;

  // The list of unsafe resources this page is warning about.
  UnsafeResourceList unsafe_resources_;

  // Indicate whether user has proceeded this blocking page.
  bool proceeded_;

  // After a safe browsing interstitial where the user opted-in to the
  // report but clicked "proceed anyway", we delay the call to
  // ThreatDetails::FinishCollection() by this much time (in
  // milliseconds), in order to get data from the blocked resource itself.
  int64_t threat_details_proceed_delay_ms_;

  // For displaying safe browsing interstitial.
  std::unique_ptr<BaseSafeBrowsingErrorUI> sb_error_ui_;

  DISALLOW_COPY_AND_ASSIGN(BaseBlockingPage);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_BASE_BLOCKING_PAGE_H_
