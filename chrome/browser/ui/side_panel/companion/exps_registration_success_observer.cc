// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/companion/exps_registration_success_observer.h"

#include "base/strings/string_split.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/utils.h"
#include "chrome/browser/profiles/profile.h"
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
    urls_to_match_against_.emplace_back(url_string);
  }
}

ExpsRegistrationSuccessObserver::~ExpsRegistrationSuccessObserver() = default;

void ExpsRegistrationSuccessObserver::PrimaryPageChanged(content::Page& page) {
  PrefService* pref_service =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())
          ->GetPrefs();
  if (pref_service->GetBoolean(kHasNavigatedToExpsSuccessPage)) {
    return;
  }
  bool matches_exps_url = false;
  const GURL& url = page.GetMainDocument().GetLastCommittedURL();
  for (const auto& url_to_match : urls_to_match_against_) {
    if (url == url_to_match) {
      matches_exps_url = true;
      break;
    }
  }

  if (!matches_exps_url) {
    return;
  }

  // Save the status to a pref.
  pref_service->SetBoolean(kHasNavigatedToExpsSuccessPage, true);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ExpsRegistrationSuccessObserver);

}  // namespace companion
