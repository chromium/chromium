// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}
class Browser;

// Helper class which watches `web_contents` to determine whether there is an
// appropriate opportunity to show the SearchEngineChoiceDialogView.
class SearchEngineChoiceTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SearchEngineChoiceTabHelper> {
 public:
  SearchEngineChoiceTabHelper(const SearchEngineChoiceTabHelper&) = delete;
  SearchEngineChoiceTabHelper& operator=(const SearchEngineChoiceTabHelper&) =
      delete;
  ~SearchEngineChoiceTabHelper() override;

 private:
  friend class content::WebContentsUserData<SearchEngineChoiceTabHelper>;

  explicit SearchEngineChoiceTabHelper(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Implemented in
// `chrome/browser/ui/views/search_engine_choice/search_engine_choice_dialog_view.cc`
// because there isn't a dependency between `chrome/browser/ui/` and
// `chrome/browser/ui/views/`.
// `boundary_dimensions` can be set to specify an upper bound for dialog's width
// and height. Leaving it empty will make the dialog use the window size
// as upper bound.
void ShowSearchEngineChoiceDialog(
    Browser& browser,
    absl::optional<gfx::Size> boundary_dimensions = absl::nullopt);

#endif  // CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TAB_HELPER_H_
