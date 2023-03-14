// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_HTTPS_ONLY_MODE_BLOCKING_PAGE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_HTTPS_ONLY_MODE_BLOCKING_PAGE_H_

#include "components/security_interstitials/content/security_interstitial_page.h"

namespace security_interstitials {

// Interstitial page object used for warnings shown when HTTPS-Only Mode fails
// to upgrade a navigation to HTTPS.
class HttpsOnlyModeBlockingPage : public SecurityInterstitialPage {
 public:
  HttpsOnlyModeBlockingPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      std::unique_ptr<SecurityInterstitialControllerClient> controller_client,
      bool is_under_advanced_protection);

  static const SecurityInterstitialPage::TypeID kTypeForTesting;
  ~HttpsOnlyModeBlockingPage() override;

  // SecurityInterstitialPage:
  void OnInterstitialClosing() override;
  SecurityInterstitialPage::TypeID GetTypeForTesting() override;

 protected:
  // SecurityInterstitialPage:
  void CommandReceived(const std::string& command) override;
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;

 private:
  bool user_made_decision_ = false;
  // True if the interstitial is shown because the user is under Advanced
  // Protection which automatically enables HTTPS-First Mode.
  bool is_under_advanced_protection_ = false;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_HTTPS_ONLY_MODE_BLOCKING_PAGE_H_
