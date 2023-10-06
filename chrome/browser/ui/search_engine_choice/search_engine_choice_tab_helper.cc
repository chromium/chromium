// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engine_choice/search_engine_choice_tab_helper.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif

SearchEngineChoiceTabHelper::~SearchEngineChoiceTabHelper() = default;

SearchEngineChoiceTabHelper::SearchEngineChoiceTabHelper(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<SearchEngineChoiceTabHelper>(*web_contents) {
  CHECK(search_engines::IsChoiceScreenFlagEnabled(
      search_engines::ChoicePromo::kDialog));
}

void SearchEngineChoiceTabHelper::DidFinishNavigation(
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

  Browser& browser = CHECK_DEREF(
      chrome::FindBrowserWithWebContents(navigation_handle->GetWebContents()));
  SearchEngineChoiceService* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(browser.profile());
  if (!search_engine_choice_service ||
      !search_engine_choice_service->CanShowDialog(browser) ||
      !search_engine_choice_service->IsUrlSuitableForDialog(
          navigation_handle->GetURL())) {
    return;
  }
  ShowSearchEngineChoiceDialog(browser);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchEngineChoiceTabHelper);
