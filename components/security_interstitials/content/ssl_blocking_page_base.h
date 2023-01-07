// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_BLOCKING_PAGE_BASE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_BLOCKING_PAGE_BASE_H_

#include "components/security_interstitials/content/cert_report_helper.h"
#include "components/security_interstitials/content/certificate_error_report.h"
#include "components/security_interstitials/content/security_interstitial_page.h"

namespace base {
class Time;
}  // namespace base

namespace net {
class SSLInfo;
}  // namespace net

class SSLCertReporter;

// This is the base class for blocking pages representing SSL certificate
// errors.
class SSLBlockingPageBase
    : public security_interstitials::SecurityInterstitialPage {
 public:
  SSLBlockingPageBase(
      content::WebContents* web_contents,
      CertificateErrorReport::InterstitialReason interstitial_reason,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      bool overridable,
      const base::Time& time_triggered,
      bool can_show_enhanced_protection_message,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client);

  SSLBlockingPageBase(const SSLBlockingPageBase&) = delete;
  SSLBlockingPageBase& operator=(const SSLBlockingPageBase&) = delete;

  ~SSLBlockingPageBase() override;

  // security_interstitials::SecurityInterstitialPage:
  void OnInterstitialClosing() override;

  CertReportHelper* cert_report_helper() { return cert_report_helper_.get(); }

  void SetSSLCertReporterForTesting(
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter);

 private:
  const std::unique_ptr<CertReportHelper> cert_report_helper_;
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SSL_BLOCKING_PAGE_BASE_H_
