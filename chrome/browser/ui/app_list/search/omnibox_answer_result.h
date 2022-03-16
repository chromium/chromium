// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_ANSWER_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_ANSWER_RESULT_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "components/omnibox/browser/autocomplete_match.h"

class AppListControllerDelegate;
class AutocompleteController;
class BitmapFetcher;
class Profile;

namespace app_list {

// An answer result from the Omnibox provider. Answer results are displayed as
// cards at the top of the categorical search launcher, so are separated in the
// UI from other Omnibox results.
class OmniboxAnswerResult : public ChromeSearchResult,
                            public BitmapFetcherDelegate {
 public:
  OmniboxAnswerResult(Profile* profile,
                      AppListControllerDelegate* list_controller,
                      AutocompleteController* autocomplete_controller,
                      const AutocompleteMatch& match,
                      const std::u16string& query);
  ~OmniboxAnswerResult() override;

  OmniboxAnswerResult(const OmniboxAnswerResult&) = delete;
  OmniboxAnswerResult& operator=(const OmniboxAnswerResult&) = delete;

  // ChromeSearchResult:
  void Open(int event_flags) override;

  // BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

 private:
  void UpdateIcon();
  // Updates title and details for the productivity launcher.
  void UpdateTitleAndDetails();
  // Updates title and details for the classic launcher.
  void UpdateClassicTitleAndDetails();

  void FetchImage(const GURL& url);

  bool IsCalculatorResult() const;
  bool IsDictionaryResult() const;
  bool IsWeatherResult() const;

  Profile* profile_;
  AppListControllerDelegate* list_controller_;
  AutocompleteController* autocomplete_controller_;
  AutocompleteMatch match_;
  const std::u16string query_;
  std::unique_ptr<BitmapFetcher> bitmap_fetcher_;

  base::WeakPtrFactory<OmniboxAnswerResult> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_ANSWER_RESULT_H_
