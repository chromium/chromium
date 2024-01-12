// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_MITM_SOFTWARE_BLOCKING_PAGE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_MITM_SOFTWARE_BLOCKING_PAGE_H_

#include <memory>
#include <string>

#include "components/security_interstitials/content/ssl_blocking_page_base.h"
#include "components/security_interstitials/core/mitm_software_ui.h"
#include "components/ssl_errors/error_classification.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "net/ssl/ssl_info.h"

class GURL;

// This class is responsible for showing/hiding the interstitial page that
// occurs when an SSL error is caused by any sort of MITM software. MITM
// software includes antiviruses, firewalls, proxies or any other non-malicious
// software that intercepts and rewrites the user's connection. This class
// creates the interstitial UI using security_interstitials::MITMSoftwareUI and
// then displays it. It deletes itself when the interstitial page is closed.
class MITMSoftwareBlockingPage : public SSLBlockingPageBase {
 public:
  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  // If the blocking page isn't shown, the caller is responsible for cleaning
  // up the blocking page. Otherwise, the interstitial takes ownership when
  // shown.
  MITMSoftwareBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const GURL& request_url,
      bool can_show_enhanced_protection_message,
      const net::SSLInfo& ssl_info,
      const std::string& mitm_software_name,
      bool is_enterprise_managed,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client);

  MITMSoftwareBlockingPage(const MITMSoftwareBlockingPage&) = delete;
  MITMSoftwareBlockingPage& operator=(const MITMSoftwareBlockingPage&) = delete;

  ~MITMSoftwareBlockingPage() override;

  // SecurityInterstitialPage method:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

 protected:
  // SecurityInterstitialPage implementation:
  void CommandReceived(const std::string& command) override;
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;

 private:
  const net::SSLInfo ssl_info_;

  const std::unique_ptr<security_interstitials::MITMSoftwareUI>
      mitm_software_ui_;
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_MITM_SOFTWARE_BLOCKING_PAGE_H_
