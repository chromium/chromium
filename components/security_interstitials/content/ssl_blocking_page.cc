// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/ssl_blocking_page.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/cert_report_helper.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/ssl_error_options_mask.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

using base::TimeTicks;
using content::NavigationEntry;
using security_interstitials::SSLErrorUI;

// static
const security_interstitials::SecurityInterstitialPage::TypeID
    SSLBlockingPage::kTypeForTesting = &SSLBlockingPage::kTypeForTesting;

security_interstitials::SecurityInterstitialPage::TypeID
SSLBlockingPage::GetTypeForTesting() {
  return SSLBlockingPage::kTypeForTesting;
}

SSLBlockingPage::~SSLBlockingPage() = default;

void SSLBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  ssl_error_ui_->PopulateStringsForHTML(load_time_data);
  cert_report_helper()->PopulateExtendedReportingOption(load_time_data);
  cert_report_helper()->PopulateEnhancedProtectionMessage(load_time_data);
}

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
SSLBlockingPage::SSLBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    const GURL& support_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    bool overridable,
    bool can_show_enhanced_protection_message,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : SSLBlockingPageBase(web_contents,
                          CertificateErrorReport::INTERSTITIAL_SSL,
                          ssl_info,
                          request_url,
                          std::move(ssl_cert_reporter),
                          overridable,
                          time_triggered,
                          can_show_enhanced_protection_message,
                          std::move(controller_client)),
      ssl_info_(ssl_info),
      overridable_(overridable),
      ssl_error_ui_(std::make_unique<SSLErrorUI>(request_url,
                                                 cert_error,
                                                 ssl_info,
                                                 options_mask,
                                                 time_triggered,
                                                 support_url,
                                                 controller())) {
  // Creating an interstitial without showing (e.g. from chrome://interstitials)
  // it leaks memory, so don't create it here.
}

// This handles the commands sent from the interstitial JavaScript.
void SSLBlockingPage::CommandReceived(const std::string& command) {
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
  // SSLErrorUI's command handling triggers a report to be sent.
  cert_report_helper()->HandleReportingCommands(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd),
      controller()->GetPrefService());
  ssl_error_ui_->HandleCommand(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd));
}

// static
bool SSLBlockingPage::IsOverridable(int options_mask) {
  const bool is_overridable =
      (options_mask &
       security_interstitials::SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED) &&
      !(options_mask &
        security_interstitials::SSLErrorOptionsMask::STRICT_ENFORCEMENT) &&
      !(options_mask &
        security_interstitials::SSLErrorOptionsMask::HARD_OVERRIDE_DISABLED);
  return is_overridable;
}
