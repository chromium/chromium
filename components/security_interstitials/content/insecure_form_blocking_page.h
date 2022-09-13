// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_INSECURE_FORM_BLOCKING_PAGE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_INSECURE_FORM_BLOCKING_PAGE_H_

#include "components/security_interstitials/content/security_interstitial_page.h"

class PrefRegistrySimple;

namespace security_interstitials {
class SecurityInterstitialControllerClient;

// Interstitial page object used for warnings shown when a form embedded on a
// secure (HTTPS) page is submitted over HTTP.
class InsecureFormBlockingPage : public SecurityInterstitialPage {
 public:
  InsecureFormBlockingPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      std::unique_ptr<SecurityInterstitialControllerClient> controller_client);

  static const SecurityInterstitialPage::TypeID kTypeForTesting;
  ~InsecureFormBlockingPage() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // SecurityInterstitialPage::
  void OnInterstitialClosing() override {}
  SecurityInterstitialPage::TypeID GetTypeForTesting() override;

 protected:
  // SecurityInterstitialPage::
  void CommandReceived(const std::string& command) override;
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;

 private:
  // Adds values required for shared interstitial HTML to |load_time_data|.
  void PopulateValuesForSharedHTML(base::Value::Dict& load_time_data);
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_INSECURE_FORM_BLOCKING_PAGE_H_
