// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/insecure_form_blocking_page.h"

#include <ostream>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {

// static
const SecurityInterstitialPage::TypeID
    InsecureFormBlockingPage::kTypeForTesting =
        &InsecureFormBlockingPage::kTypeForTesting;

InsecureFormBlockingPage::InsecureFormBlockingPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    std::unique_ptr<SecurityInterstitialControllerClient> controller_client)
    : SecurityInterstitialPage(web_contents,
                               request_url,
                               std::move(controller_client)) {}

InsecureFormBlockingPage::~InsecureFormBlockingPage() = default;

void InsecureFormBlockingPage::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kMixedFormsWarningsEnabled, true);
}

SecurityInterstitialPage::TypeID InsecureFormBlockingPage::GetTypeForTesting() {
  return InsecureFormBlockingPage::kTypeForTesting;
}

void InsecureFormBlockingPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }
  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);
  switch (cmd) {
    case security_interstitials::CMD_DONT_PROCEED:
      controller()->GoBack();
      break;
    case security_interstitials::CMD_PROCEED:
      controller()->Proceed();
      break;
    case security_interstitials::CMD_DO_REPORT:
    case security_interstitials::CMD_DONT_REPORT:
    case security_interstitials::CMD_SHOW_MORE_SECTION:
    case security_interstitials::CMD_OPEN_DATE_SETTINGS:
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY:
    case security_interstitials::CMD_OPEN_WHITEPAPER:
    case security_interstitials::CMD_OPEN_HELP_CENTER:
    case security_interstitials::CMD_RELOAD:
    case security_interstitials::CMD_OPEN_DIAGNOSTIC:
    case security_interstitials::CMD_OPEN_LOGIN:
    case security_interstitials::CMD_REPORT_PHISHING_ERROR:
      // Not supported by the insecure form blocking page.
      NOTREACHED_IN_MIGRATION() << "Unsupported command: " << command;
      break;
    case security_interstitials::CMD_ERROR:
    case security_interstitials::CMD_TEXT_FOUND:
    case security_interstitials::CMD_TEXT_NOT_FOUND:
      // Commands are for testing.
      break;
  }
}

void InsecureFormBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  PopulateValuesForSharedHTML(load_time_data);

  load_time_data.Set("tabTitle",
                     l10n_util::GetStringUTF16(IDS_INSECURE_FORM_TITLE));
  load_time_data.Set("heading",
                     l10n_util::GetStringUTF16(IDS_INSECURE_FORM_HEADING));
  load_time_data.Set(
      "primaryParagraph",
      l10n_util::GetStringUTF16(IDS_INSECURE_FORM_PRIMARY_PARAGRAPH));
  load_time_data.Set("proceedButtonText", l10n_util::GetStringUTF16(
                                              IDS_INSECURE_FORM_SUBMIT_BUTTON));
  load_time_data.Set("primaryButtonText",
                     l10n_util::GetStringUTF16(IDS_INSECURE_FORM_BACK_BUTTON));
  load_time_data.Set("optInLink", l10n_util::GetStringUTF16(
                                      IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE));
  load_time_data.Set(
      "enhancedProtectionMessage",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE));
}

void InsecureFormBlockingPage::PopulateValuesForSharedHTML(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("type", "INSECURE_FORM");
  load_time_data.Set("overridable", false);
  load_time_data.Set("hide_primary_button", false);
  load_time_data.Set("show_recurrent_error_paragraph", false);
  load_time_data.Set("recurrentErrorParagraph", "");
  load_time_data.Set("openDetails", "");
  load_time_data.Set("explanationParagraph", "");
  load_time_data.Set("finalParagraph", "");
}
}  // namespace security_interstitials
