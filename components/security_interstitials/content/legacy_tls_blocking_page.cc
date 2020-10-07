// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/legacy_tls_blocking_page.h"

#include "components/security_interstitials/content/cert_report_helper.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"

using content::NavigationController;
using content::NavigationEntry;

// static
const security_interstitials::SecurityInterstitialPage::TypeID
    LegacyTLSBlockingPage::kTypeForTesting =
        &LegacyTLSBlockingPage::kTypeForTesting;

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
// Creating an interstitial without showing (e.g. from chrome://interstitials)
// it leaks memory, so don't create it here.
LegacyTLSBlockingPage::LegacyTLSBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const net::SSLInfo& ssl_info,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : SSLBlockingPageBase(web_contents,
                          CertificateErrorReport::INTERSTITIAL_LEGACY_TLS,
                          ssl_info,
                          request_url,
                          std::move(ssl_cert_reporter),
                          true /* overridable */,
                          base::Time::Now(),
                          std::move(controller_client)),
      ssl_info_(ssl_info),
      legacy_tls_ui_(new security_interstitials::LegacyTLSUI(request_url,
                                                             cert_error,
                                                             ssl_info,
                                                             controller())) {}

LegacyTLSBlockingPage::~LegacyTLSBlockingPage() = default;

security_interstitials::SecurityInterstitialPage::TypeID
LegacyTLSBlockingPage::GetTypeForTesting() {
  return LegacyTLSBlockingPage::kTypeForTesting;
}

void LegacyTLSBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  legacy_tls_ui_->PopulateStringsForHTML(load_time_data);
  cert_report_helper()->PopulateExtendedReportingOption(load_time_data);
  cert_report_helper()->PopulateEnhancedProtectionMessage(load_time_data);
}

// This handles the commands sent from the interstitial JavaScript.
void LegacyTLSBlockingPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);

  // Let the CertReportHelper handle commands first, This allows it to get set
  // up to send reports, so that the report is populated properly if
  // LegacyTLSUI's command handling triggers a report to be sent.
  cert_report_helper()->HandleReportingCommands(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd),
      controller()->GetPrefService());
  legacy_tls_ui_->HandleCommand(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd));
}
