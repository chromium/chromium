// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_BLOCKED_INTERCEPTION_BLOCKING_PAGE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_BLOCKED_INTERCEPTION_BLOCKING_PAGE_H_

#include "components/security_interstitials/content/ssl_blocking_page_base.h"
#include "components/security_interstitials/core/blocked_interception_ui.h"
#include "net/ssl/ssl_info.h"

class BlockedInterceptionBlockingPage : public SSLBlockingPageBase {
 public:
  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  BlockedInterceptionBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const GURL& request_url,
      bool can_show_enhanced_protection_message,
      const net::SSLInfo& ssl_info,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client);

  BlockedInterceptionBlockingPage(const BlockedInterceptionBlockingPage&) =
      delete;
  BlockedInterceptionBlockingPage& operator=(
      const BlockedInterceptionBlockingPage&) = delete;

  ~BlockedInterceptionBlockingPage() override;

  // SecurityInterstitialPage method:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

 protected:
  // SecurityInterstitialPage implementation:
  void CommandReceived(const std::string& command) override;
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;

 private:
  const net::SSLInfo ssl_info_;

  const std::unique_ptr<security_interstitials::BlockedInterceptionUI>
      blocked_interception_ui_;
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_BLOCKED_INTERCEPTION_BLOCKING_PAGE_H_
