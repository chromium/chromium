// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TAB_HELPER_H_

#include <optional>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/geometry/size.h"

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

  // Static method that tells us if the helper is needed. This is to be
  // checked before creating the helper so we don't unnecessarily create one
  // for every `WebContents`.
  // TODO(crbug.com/347223092): Improve this approximative check, that
  // currently only depends on feature flags.
  static bool IsHelperNeeded();

 private:
  friend class content::WebContentsUserData<SearchEngineChoiceTabHelper>;

  explicit SearchEngineChoiceTabHelper(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Shows the dialog if the user is eligible and if the tab is in compatible
  // state (e.g. visible, loaded, suitable URL).
  void MaybeShowDialog();

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Implemented in
// `chrome/browser/ui/views/search_engine_choice/search_engine_choice_dialog_view.cc`
// because there isn't a dependency between `chrome/browser/ui/` and
// `chrome/browser/ui/views/`.
// `boundary_dimensions_for_test` can be set to specify an upper bound for
// dialog's width and height. Leaving it empty will make the dialog use the
// window size as upper bound.
// `zoom_factor_for_test` can be set to specify the zoom factor needed. This is
// used to be able to display the full content of the dialog in screenshot
// tests. Leaving it empty will make the dialog use a zoom of 1.;
void ShowSearchEngineChoiceDialog(
    Browser& browser,
    std::optional<gfx::Size> boundary_dimensions_for_test = std::nullopt,
    std::optional<double> zoom_factor_for_test_ = std::nullopt);

// Implemented in
// `chrome/browser/ui/views/search_engine_choice/search_engine_choice_dialog_view.cc`
// because there isn't a dependency between `chrome/browser/ui/` and
// `chrome/browser/ui/views/`.
// Returns whether the smallest height variant of the search engine choice
// dialog can fit in the browser window or not.
bool CanWindowHeightFitSearchEngineChoiceDialog(Browser& browser);

#endif  // CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TAB_HELPER_H_
