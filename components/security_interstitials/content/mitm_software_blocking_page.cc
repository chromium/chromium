// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/mitm_software_blocking_page.h"

#include <utility>

#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
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

namespace {}  // namespace

// static
const security_interstitials::SecurityInterstitialPage::TypeID
    MITMSoftwareBlockingPage::kTypeForTesting =
        &MITMSoftwareBlockingPage::kTypeForTesting;

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
// Creating an interstitial without showing (e.g. from chrome://interstitials)
// it leaks memory, so don't create it here.
MITMSoftwareBlockingPage::MITMSoftwareBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    bool can_show_enhanced_protection_message,
    const net::SSLInfo& ssl_info,
    const std::string& mitm_software_name,
    bool is_enterprise_managed,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : SSLBlockingPageBase(web_contents,
                          CertificateErrorReport::INTERSTITIAL_MITM_SOFTWARE,
                          ssl_info,
                          request_url,
                          std::move(ssl_cert_reporter),
                          false /* overridable */,
                          base::Time::Now(),
                          can_show_enhanced_protection_message,
                          std::move(controller_client)),

      ssl_info_(ssl_info),
      mitm_software_ui_(
          new security_interstitials::MITMSoftwareUI(request_url,
                                                     cert_error,
                                                     ssl_info,
                                                     mitm_software_name,
                                                     is_enterprise_managed,
                                                     controller())) {}

MITMSoftwareBlockingPage::~MITMSoftwareBlockingPage() = default;

security_interstitials::SecurityInterstitialPage::TypeID
MITMSoftwareBlockingPage::GetTypeForTesting() {
  return MITMSoftwareBlockingPage::kTypeForTesting;
}

void MITMSoftwareBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  mitm_software_ui_->PopulateStringsForHTML(load_time_data);
  cert_report_helper()->PopulateExtendedReportingOption(load_time_data);
  cert_report_helper()->PopulateEnhancedProtectionMessage(load_time_data);
}

// This handles the commands sent from the interstitial JavaScript.
void MITMSoftwareBlockingPage::CommandReceived(const std::string& command) {
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
  // MITMSoftwareUI's command handling triggers a report to be sent.
  cert_report_helper()->HandleReportingCommands(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd),
      controller()->GetPrefService());
  mitm_software_ui_->HandleCommand(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd));
}
