// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_CONTROLLER_CLIENT_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_CONTROLLER_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "components/security_interstitials/core/controller_client.h"
#include "url/gurl.h"

namespace content {
  class WebContents;
}

namespace security_interstitials {

class MetricsHelper;
class SettingsPageHelper;

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
      const GURL& default_safe_page,
      std::unique_ptr<SettingsPageHelper> settings_page_helper);

  SecurityInterstitialControllerClient(
      const SecurityInterstitialControllerClient&) = delete;
  SecurityInterstitialControllerClient& operator=(
      const SecurityInterstitialControllerClient&) = delete;

  ~SecurityInterstitialControllerClient() override;

  // security_interstitials::ControllerClient overrides.
  void GoBack() override;
  bool CanGoBack() override;
  void GoBackAfterNavigationCommitted() override;
  void Proceed() override;
  void Reload() override;
  void OpenUrlInCurrentTab(const GURL& url) override;
  void OpenUrlInNewForegroundTab(const GURL& url) override;
  void OpenEnhancedProtectionSettings() override;
  PrefService* GetPrefService() override;
  const std::string& GetApplicationLocale() const override;
  bool CanLaunchDateAndTimeSettings() override;
  void LaunchDateAndTimeSettings() override;
  bool CanGoBackBeforeNavigation() override;

 protected:
  // security_interstitials::ControllerClient overrides.
  const std::string GetExtendedReportingPrefName() const override;
  raw_ptr<content::WebContents> web_contents_;

 private:
  raw_ptr<PrefService> prefs_;
  const std::string app_locale_;
  // The default safe page we should go to if there is no previous page to go
  // back to, e.g. chrome:kChromeUINewTabURL.
  const GURL default_safe_page_;
  std::unique_ptr<SettingsPageHelper> settings_page_helper_;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_CONTROLLER_CLIENT_H_
