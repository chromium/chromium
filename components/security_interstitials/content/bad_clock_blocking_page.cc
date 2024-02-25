// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/bad_clock_blocking_page.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/core/bad_clock_ui.h"
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
    BadClockBlockingPage::kTypeForTesting =
        &BadClockBlockingPage::kTypeForTesting;

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
// Creating an interstitial without showing (e.g. from chrome://interstitials)
// it leaks memory, so don't create it here.
BadClockBlockingPage::BadClockBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    const base::Time& time_triggered,
    bool can_show_enhanced_protection_message,
    ssl_errors::ClockState clock_state,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : SSLBlockingPageBase(web_contents,
                          ssl_info,
                          request_url,
                          false /* overridable */,
                          time_triggered,
                          can_show_enhanced_protection_message,
                          std::move(controller_client)),
      ssl_info_(ssl_info),
      bad_clock_ui_(new security_interstitials::BadClockUI(request_url,
                                                           cert_error,
                                                           ssl_info,
                                                           time_triggered,
                                                           clock_state,
                                                           controller())) {}

BadClockBlockingPage::~BadClockBlockingPage() = default;

security_interstitials::SecurityInterstitialPage::TypeID
BadClockBlockingPage::GetTypeForTesting() {
  return BadClockBlockingPage::kTypeForTesting;
}

void BadClockBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  bad_clock_ui_->PopulateStringsForHTML(load_time_data);

  PopulateEnhancedProtectionMessage(load_time_data);
}

// This handles the commands sent from the interstitial JavaScript.
void BadClockBlockingPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);

  bad_clock_ui_->HandleCommand(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd));
}
