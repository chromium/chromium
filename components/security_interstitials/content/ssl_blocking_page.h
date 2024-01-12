// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_BLOCKING_PAGE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_BLOCKING_PAGE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/ssl_blocking_page_base.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace policy {
class PolicyTest_SSLErrorOverridingDisallowed_Test;
}

namespace security_interstitials {
class SSLErrorUI;
}

// URL to use as the 'Learn More' link when the interstitial is caused by
// a "ERR_CERT_SYMANTEC_LEGACY" error, -202 fragment is included so
// chrome://connection-help expands the right section if the user can't reach
// the help center.
const char kSymantecSupportUrl[] =
    "https://support.google.com/chrome?p=symantec#-202";

// This class is responsible for showing/hiding the interstitial page that is
// shown when a certificate error happens.
// It deletes itself when the interstitial page is closed.
class SSLBlockingPage : public SSLBlockingPageBase {
 public:
  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  SSLBlockingPage(const SSLBlockingPage&) = delete;
  SSLBlockingPage& operator=(const SSLBlockingPage&) = delete;

  ~SSLBlockingPage() override;

  // InterstitialPageDelegate method:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

  // Returns true if |options_mask| refers to a soft-overridable SSL error.
  static bool IsOverridable(int options_mask);

  SSLBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      int options_mask,
      const base::Time& time_triggered,
      const GURL& support_url,
      bool overrideable,
      bool can_show_enhanced_protection_message,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client);

 protected:
  // SecurityInterstitialPage implementation:
  void CommandReceived(const std::string& command) override;
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;

 private:
  friend class policy::PolicyTest_SSLErrorOverridingDisallowed_Test;
  friend class SSLUITestBase;
  friend class InterstitialAccessibilityBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(SSLBlockingPageTest,
                           VerifySecurityInterstitialExtensionEvents);
  void NotifyDenyCertificate();

  const net::SSLInfo ssl_info_;
  const bool overridable_;  // The UI allows the user to override the error.

  const std::unique_ptr<security_interstitials::SSLErrorUI> ssl_error_ui_;
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_BLOCKING_PAGE_H_
