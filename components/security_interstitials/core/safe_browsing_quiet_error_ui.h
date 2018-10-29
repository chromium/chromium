// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_SAFE_BROWSING_QUIET_ERROR_UI_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_SAFE_BROWSING_QUIET_ERROR_UI_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "components/security_interstitials/core/controller_client.h"
#include "url/gurl.h"

namespace security_interstitials {

// Quiet version of the safe browsing interstitial. This is the small screen
// version of the interstitial selectively used in parts of WebView.
// This class displays a quiet UI for Safe Browsing errors that block page loads
// specifically for WebView. This class is purely about visual display; it does
// not do any error-handling logic to determine what type of error should be
// displayed when.
class SafeBrowsingQuietErrorUI
    : public security_interstitials::BaseSafeBrowsingErrorUI {
 public:
  SafeBrowsingQuietErrorUI(
      const GURL& request_url,
      const GURL& main_frame_url,
      BaseSafeBrowsingErrorUI::SBInterstitialReason reason,
      const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
      const std::string& app_locale,
      const base::Time& time_triggered,
      ControllerClient* controller,
      const bool is_giant_webview);
  ~SafeBrowsingQuietErrorUI() override;

  // Fills the passed dictionary with the values to be passed to the template
  // when creating the HTML.
  void PopulateStringsForHtml(base::DictionaryValue* load_time_data) override;

  void HandleCommand(SecurityInterstitialCommand command) override;

  // Manually set whether displaying in a giant WebView. Specifially used in
  // tests.
  void SetGiantWebViewForTesting(bool is_giant_webview);

  int GetHTMLTemplateId() const override;

 private:
  void PopulateMalwareLoadTimeData(base::DictionaryValue* load_time_data);
  void PopulateHarmfulLoadTimeData(base::DictionaryValue* load_time_data);
  void PopulatePhishingLoadTimeData(base::DictionaryValue* load_time_data);
  void PopulateBillingLoadTimeData(base::DictionaryValue* load_time_data);

  bool is_giant_webview_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingQuietErrorUI);
};

}  // security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_SAFE_BROWSING_QUIET_ERROR_UI_H_
