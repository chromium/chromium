// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_CONTROLLER_CLIENT_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_CONTROLLER_CLIENT_H_

#include "components/security_interstitials/content/security_interstitial_controller_client.h"

namespace content {
class WebContents;
}

namespace security_interstitials {
class MetricsHelper;
class SettingsPageHelper;
}  // namespace security_interstitials

class PrefService;

namespace safe_browsing {

// Provides embedder-specific logic for the Safe Browsing interstitial page
// controller.
class SafeBrowsingControllerClient
    : public security_interstitials::SecurityInterstitialControllerClient {
 public:
  SafeBrowsingControllerClient(
      content::WebContents* web_contents,
      std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
      PrefService* prefs,
      const std::string& app_locale,
      const GURL& default_safe_page,
      std::unique_ptr<security_interstitials::SettingsPageHelper>
          settings_page_helper);

  SafeBrowsingControllerClient(const SafeBrowsingControllerClient&) = delete;
  SafeBrowsingControllerClient& operator=(const SafeBrowsingControllerClient&) =
      delete;

  ~SafeBrowsingControllerClient() override;

  void Proceed() override;

  void GoBack() override;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_CONTROLLER_CLIENT_H_
