// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_ORIGIN_POLICY_INTERSTITIAL_PAGE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_ORIGIN_POLICY_INTERSTITIAL_PAGE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "services/network/public/cpp/origin_policy.h"

#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace security_interstitials {
class SecurityInterstitialControllerClient;

class OriginPolicyInterstitialPage : public SecurityInterstitialPage {
 public:
  OriginPolicyInterstitialPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      std::unique_ptr<SecurityInterstitialControllerClient> controller,
      network::OriginPolicyState error_reason);

  ~OriginPolicyInterstitialPage() override;

  void OnInterstitialClosing() override;

  void CommandReceived(const std::string& command) override;
  void OnProceed() override;
  void OnDontProceed() override;

 protected:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(base::DictionaryValue*) override;

 private:
  network::OriginPolicyState error_reason_;

  DISALLOW_COPY_AND_ASSIGN(OriginPolicyInterstitialPage);
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_ORIGIN_POLICY_INTERSTITIAL_PAGE_H_
