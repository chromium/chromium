// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/origin_policy_interstitial_page.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/origin_policy_commands.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace security_interstitials {

OriginPolicyInterstitialPage::OriginPolicyInterstitialPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    std::unique_ptr<SecurityInterstitialControllerClient> controller,
    network::OriginPolicyState error_reason)
    : SecurityInterstitialPage(web_contents,
                               request_url,
                               std::move(controller)),
      error_reason_(error_reason) {}

OriginPolicyInterstitialPage::~OriginPolicyInterstitialPage() {}

void OriginPolicyInterstitialPage::OnInterstitialClosing() {}

bool OriginPolicyInterstitialPage::ShouldCreateNewNavigation() const {
  return false;
}

void OriginPolicyInterstitialPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  load_time_data->SetString("type", "ORIGIN_POLICY");

  // User may choose to ignore the warning & proceed to the site.
  load_time_data->SetBoolean("overridable", true);

  // Custom messages depending on the OriginPolicyState:
  int explanation_paragraph_id = IDS_ORIGIN_POLICY_EXPLANATION_OTHER;
  switch (error_reason_) {
    case network::OriginPolicyState::kCannotLoadPolicy:
      explanation_paragraph_id = IDS_ORIGIN_POLICY_EXPLANATION_CANNOT_LOAD;
      break;
    case network::OriginPolicyState::kInvalidRedirect:
      explanation_paragraph_id =
          IDS_ORIGIN_POLICY_EXPLANATION_SHOULD_NOT_REDIRECT;
      break;
    default:
      NOTREACHED();
      break;
  }

  // Variables in IDR_SECURITY_INTERSTITIAL_HTML / interstitial_large.html,
  // resources defined in security_interstitials_strings.grdp.
  const struct {
    const char* name;
    int id;
  } messages[] = {
      {"closeDetails", IDS_ORIGIN_POLICY_CLOSE},
      {"explanationParagraph", explanation_paragraph_id},
      {"finalParagraph", IDS_ORIGIN_POLICY_FINAL_PARAGRAPH},
      {"heading", IDS_ORIGIN_POLICY_HEADING},
      {"openDetails", IDS_ORIGIN_POLICY_DETAILS},
      {"primaryButtonText", IDS_ORIGIN_POLICY_BUTTON},
      {"primaryParagraph", IDS_ORIGIN_POLICY_INFO},
      {"recurrentErrorParagraph", IDS_ORIGIN_POLICY_INFO2},
      {"tabTitle", IDS_ORIGIN_POLICY_TITLE},
  };
  // We interpolate _all_ strings with URL ($1) and origin ($2).
  const std::vector<base::string16> message_params = {
      base::ASCIIToUTF16(request_url().spec()),
      base::ASCIIToUTF16(url::Origin::Create(request_url()).Serialize()),
  };
  for (const auto& message : messages) {
    load_time_data->SetString(
        message.name,
        base::ReplaceStringPlaceholders(l10n_util::GetStringUTF16(message.id),
                                        message_params, nullptr));
  };

  // Selectively enable certain UI elements.
  load_time_data->SetBoolean(
      "hide_primary_button",
      !web_contents()->GetController().CanGoBack() ||
          load_time_data->FindStringKey("primaryButtonText")->empty());
  load_time_data->SetBoolean(
      "show_recurrent_error_paragraph",
      !load_time_data->FindStringKey("recurrentErrorParagraph")->empty());
}

void OriginPolicyInterstitialPage::CommandReceived(const std::string& command) {
  // The command string we get passed in is interstitial_commands.mojom turned
  // into a number turned into a string.
  //
  // TODO(carlosil): After non-committed insterstitials have been removed this
  //                 should be cleaned up to use enum values (or somesuch).
  if (command == "0") {
    OnDontProceed();
  } else if (command == "1") {
    OnProceed();
  } else if (command == "2") {
    // "Advanced" button, which shows extra text. This is handled within
    // the page.
  } else {
    NOTREACHED();
  }
}

void OriginPolicyInterstitialPage::OnProceed() {
  content::OriginPolicyAddExceptionFor(web_contents()->GetBrowserContext(),
                                       request_url());
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

void OriginPolicyInterstitialPage::OnDontProceed() {
  // "Go Back" / "Don't Proceed" button should be disabled if we can't go back.
  DCHECK(web_contents()->GetController().CanGoBack());
  web_contents()->GetController().GoBack();
}

}  // namespace security_interstitials
