// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waffle/waffle_tab_helper.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/webui_url_constants.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif

WaffleTabHelper::~WaffleTabHelper() = default;

WaffleTabHelper::WaffleTabHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<WaffleTabHelper>(*web_contents) {
  CHECK(base::FeatureList::IsEnabled(switches::kWaffle));
}

void WaffleTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle) {
    return;
  }

  // Only valid top frame and committed navigations are considered.
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Don't show the Waffle dialog on top of any sub page of the settings page.
  if (navigation_handle->GetURL().host() == chrome::kChromeUISettingsHost) {
    return;
  }

  auto* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  CHECK(profile);
  bool is_regular_profile = profile->IsRegularProfile();

#if BUILDFLAG(IS_CHROMEOS)
  is_regular_profile &= !profiles::IsPublicSession() &&
                        !chromeos::IsKioskSession() &&
                        !profiles::IsChromeAppKioskSession();
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  is_regular_profile &= !profiles::IsGuestSession();
#endif

  if (!search_engines::ShouldShowChoiceScreen(
          *g_browser_process->policy_service(),
          /*profile_properties=*/{.is_regular_profile = is_regular_profile})) {
    return;
  }

  if (auto* browser = chrome::FindBrowserWithWebContents(
          navigation_handle->GetWebContents())) {
    ShowWaffleDialog(*browser);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WaffleTabHelper);
