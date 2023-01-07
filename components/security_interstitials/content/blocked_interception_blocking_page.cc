// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/blocked_interception_blocking_page.h"

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

using content::NavigationController;
using content::NavigationEntry;

// static
const security_interstitials::SecurityInterstitialPage::TypeID
    BlockedInterceptionBlockingPage::kTypeForTesting =
        &BlockedInterceptionBlockingPage::kTypeForTesting;

namespace {}  // namespace

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
// Creating an interstitial without showing (e.g. from chrome://interstitials)
// it leaks memory, so don't create it here.
BlockedInterceptionBlockingPage::BlockedInterceptionBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    bool can_show_enhanced_protection_message,
    const net::SSLInfo& ssl_info,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : SSLBlockingPageBase(
          web_contents,
          CertificateErrorReport::INTERSTITIAL_BLOCKED_INTERCEPTION,
          ssl_info,
          request_url,
          std::move(ssl_cert_reporter),
          true /* overridable */,
          base::Time::Now(),
          can_show_enhanced_protection_message,
          std::move(controller_client)),
      ssl_info_(ssl_info),
      blocked_interception_ui_(
          new security_interstitials::BlockedInterceptionUI(request_url,
                                                            cert_error,
                                                            ssl_info,
                                                            controller())) {}

BlockedInterceptionBlockingPage::~BlockedInterceptionBlockingPage() = default;

security_interstitials::SecurityInterstitialPage::TypeID
BlockedInterceptionBlockingPage::GetTypeForTesting() {
  return BlockedInterceptionBlockingPage::kTypeForTesting;
}

void BlockedInterceptionBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  blocked_interception_ui_->PopulateStringsForHTML(load_time_data);
  cert_report_helper()->PopulateExtendedReportingOption(load_time_data);
  cert_report_helper()->PopulateEnhancedProtectionMessage(load_time_data);
}

// This handles the commands sent from the interstitial JavaScript.
void BlockedInterceptionBlockingPage::CommandReceived(
    const std::string& command) {
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
  // BlockedInterceptionUI's command handling triggers a report to be sent.
  cert_report_helper()->HandleReportingCommands(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd),
      controller()->GetPrefService());
  blocked_interception_ui_->HandleCommand(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd));
}
