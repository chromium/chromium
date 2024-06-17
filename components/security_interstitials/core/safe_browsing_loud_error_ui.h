// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_SAFE_BROWSING_LOUD_ERROR_UI_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_SAFE_BROWSING_LOUD_ERROR_UI_H_

#include "base/time/time.h"
#include "base/values.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "components/security_interstitials/core/controller_client.h"
#include "url/gurl.h"

namespace security_interstitials {

// Loud version of the safe browsing interstitial. This is the full screen
// version of the interstitial used on Desktop, Android and iOS. It is
// selectively used in parts of WebView.
// This class displays UI for Safe Browsing errors that block page loads. This
// class is purely about visual display; it does not do any error-handling logic
// to determine what type of error should be displayed when.
// |created_prior_to_navigation| should be set to true if this UI was created
// prior to navigating to the error page (e.g. from WillFailResponse in a
// navigation throttle), false otherwise.
class SafeBrowsingLoudErrorUI
    : public security_interstitials::BaseSafeBrowsingErrorUI {
 public:
  SafeBrowsingLoudErrorUI(
      const GURL& request_url,
      BaseSafeBrowsingErrorUI::SBInterstitialReason reason,
      const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
      const std::string& app_locale,
      const base::Time& time_triggered,
      ControllerClient* controller,
      bool created_prior_to_navigation);

  SafeBrowsingLoudErrorUI(const SafeBrowsingLoudErrorUI&) = delete;
  SafeBrowsingLoudErrorUI& operator=(const SafeBrowsingLoudErrorUI&) = delete;

  ~SafeBrowsingLoudErrorUI() override;

  // Implement BaseSafeBrowsingErrorUI.
  void PopulateStringsForHtml(base::Value::Dict& load_time_data) override;
  void HandleCommand(SecurityInterstitialCommand command) override;

  int GetHTMLTemplateId() const override;

 private:
  // Fills the passed dictionary with the values to be passed to the template
  // when creating the HTML.
  void PopulateExtendedReportingOption(base::Value::Dict& load_time_data);
  void PopulateMalwareLoadTimeData(base::Value::Dict& load_time_data);
  void PopulateHarmfulLoadTimeData(base::Value::Dict& load_time_data);
  void PopulatePhishingLoadTimeData(base::Value::Dict& load_time_data);
  void PopulateBillingLoadTimeData(base::Value::Dict& load_time_data);
  void PopulateEnhancedProtectionMessage(base::Value::Dict& load_time_data);

  // Handle update to interstitial_interaction_data_ when a command occurs.
  void UpdateInterstitialInteractionData(SecurityInterstitialCommand command);

  const bool created_prior_to_navigation_;
};

}  // security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_SAFE_BROWSING_LOUD_ERROR_UI_H_
