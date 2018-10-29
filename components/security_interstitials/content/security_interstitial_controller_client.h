// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_CONTROLLER_CLIENT_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_CONTROLLER_CLIENT_H_

#include "base/macros.h"
#include "components/security_interstitials/core/controller_client.h"
#include "url/gurl.h"

namespace content {
  class InterstitialPage;
  class WebContents;
}

namespace security_interstitials {

class MetricsHelper;

// Handle commands from security interstitial pages. This class should only be
// instantiated by SafeBrowsingBlockingPage for the time being.
class SecurityInterstitialControllerClient
    : public security_interstitials::ControllerClient {
 public:
  SecurityInterstitialControllerClient(
      content::WebContents* web_contents,
      std::unique_ptr<MetricsHelper> metrics_helper,
      PrefService* prefs,
      const std::string& app_locale,
      const GURL& default_safe_page);

  ~SecurityInterstitialControllerClient() override;

  void set_interstitial_page(content::InterstitialPage* interstitial_page);
  content::InterstitialPage* interstitial_page();

  // security_interstitials::ControllerClient overrides.
  void GoBack() override;
  bool CanGoBack() override;
  void GoBackAfterNavigationCommitted() override;
  void Proceed() override;
  void Reload() override;
  void OpenUrlInCurrentTab(const GURL& url) override;
  void OpenUrlInNewForegroundTab(const GURL& url) override;
  PrefService* GetPrefService() override;
  const std::string& GetApplicationLocale() const override;
  bool CanLaunchDateAndTimeSettings() override;
  void LaunchDateAndTimeSettings() override;

 protected:
  // security_interstitials::ControllerClient overrides.
  const std::string GetExtendedReportingPrefName() const override;
  content::WebContents* web_contents_;

 private:
  content::InterstitialPage* interstitial_page_;
  PrefService* prefs_;
  const std::string app_locale_;
  // The default safe page we should go to if there is no previous page to go
  // back to, e.g. chrome:kChromeUINewTabURL.
  const GURL default_safe_page_;

  DISALLOW_COPY_AND_ASSIGN(SecurityInterstitialControllerClient);
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_CONTROLLER_CLIENT_H_
