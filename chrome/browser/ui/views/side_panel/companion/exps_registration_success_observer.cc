// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/companion/exps_registration_success_observer.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/common/pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace companion {

ExpsRegistrationSuccessObserver::ExpsRegistrationSuccessObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ExpsRegistrationSuccessObserver>(
          *web_contents) {
  const auto& url_strings_to_match =
      base::SplitString(companion::GetExpsRegistrationSuccessPageURLs(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& url_string : url_strings_to_match) {
    exps_registration_success_url_patterns_.emplace_back(url_string);
  }

  const auto& blocklisted_url_strings_to_match =
      base::SplitString(companion::GetCompanionIPHBlocklistedPageURLs(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& url_string : blocklisted_url_strings_to_match) {
    blocklisted_iph_url_patterns_.emplace_back(url_string);
  }
}

ExpsRegistrationSuccessObserver::~ExpsRegistrationSuccessObserver() = default;

void ExpsRegistrationSuccessObserver::PrimaryPageChanged(content::Page& page) {
  if (!web_contents() || !pref_service()) {
    return;
  }

  MaybeShowIPH();

  if (pref_service()->GetBoolean(kHasNavigatedToExpsSuccessPage)) {
    return;
  }

  const GURL& url = page.GetMainDocument().GetLastCommittedURL();
  if (!DoesUrlMatchPatternsInList(url,
                                  exps_registration_success_url_patterns_)) {
    return;
  }

  // Save the status to a pref.
  pref_service()->SetBoolean(kHasNavigatedToExpsSuccessPage, true);
}

void ExpsRegistrationSuccessObserver::MaybeShowIPH() {
  if (web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
    return;
  }

  const auto& url = web_contents()->GetVisibleURL();
  if (!IsValidPageURLForCompanion(url)) {
    return;
  }

  if (!IsSearchInCompanionSidePanelSupported()) {
    return;
  }

  if (DoesUrlMatchPatternsInList(url, blocklisted_iph_url_patterns_)) {
    return;
  }

  bool has_pinned_entry = pref_service()->GetBoolean(
      prefs::kSidePanelCompanionEntryPinnedToToolbar);
  if (!has_pinned_entry) {
    return;
  }

  ShowIPH();
}

void ExpsRegistrationSuccessObserver::ShowIPH() {
  Browser* const browser = chrome::FindBrowserWithTab(web_contents());
  if (!browser || !browser->window()) {
    return;
  }
  browser->window()->MaybeShowFeaturePromo(
      feature_engagement::kIPHCompanionSidePanelFeature);
}

PrefService* ExpsRegistrationSuccessObserver::pref_service() {
  auto* profile =
      web_contents()
          ? Profile::FromBrowserContext(web_contents()->GetBrowserContext())
          : nullptr;
  return profile ? profile->GetPrefs() : nullptr;
}

bool ExpsRegistrationSuccessObserver::IsSearchInCompanionSidePanelSupported() {
  return companion::IsSearchInCompanionSidePanelSupported(
      chrome::FindBrowserWithTab(web_contents()));
}

bool ExpsRegistrationSuccessObserver::DoesUrlMatchPatternsInList(
    const GURL& url,
    const std::vector<std::string>& url_patterns) {
  if (!url.is_valid()) {
    return false;
  }

  for (const auto& url_pattern : url_patterns) {
    if (base::StartsWith(url.spec(), url_pattern)) {
      return true;
    }
  }

  return false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ExpsRegistrationSuccessObserver);

}  // namespace companion
