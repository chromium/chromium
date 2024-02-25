// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_BAD_CLOCK_BLOCKING_PAGE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_BAD_CLOCK_BLOCKING_PAGE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/security_interstitials/content/ssl_blocking_page_base.h"
#include "components/ssl_errors/error_classification.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "net/ssl/ssl_info.h"

class GURL;

namespace security_interstitials {
class BadClockUI;
}

// This class is responsible for showing/hiding the interstitial page that
// occurs when an SSL error is triggered by a clock misconfiguration. It
// creates the UI using security_interstitials::BadClockUI and then
// displays it. It deletes itself when the interstitial page is closed.
class BadClockBlockingPage : public SSLBlockingPageBase {
 public:
  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  // If the blocking page isn't shown, the caller is responsible for cleaning
  // up the blocking page. Otherwise, the interstitial takes ownership when
  // shown.
  BadClockBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      const base::Time& time_triggered,
      bool can_show_enhanced_protection_message,
      ssl_errors::ClockState clock_state,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client);

  BadClockBlockingPage(const BadClockBlockingPage&) = delete;
  BadClockBlockingPage& operator=(const BadClockBlockingPage&) = delete;

  ~BadClockBlockingPage() override;

  // SecurityInterstitialPage method:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

 protected:
  // SecurityInterstitialPage implementation:
  void CommandReceived(const std::string& command) override;
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;

 private:
  const net::SSLInfo ssl_info_;

  const std::unique_ptr<security_interstitials::BadClockUI> bad_clock_ui_;
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_BAD_CLOCK_BLOCKING_PAGE_H_
