// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_RESULT_H_

#include <memory>
#include <vector>

#include "ash/public/cpp/style/color_mode_observer.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "components/omnibox/browser/autocomplete_match.h"

class AppListControllerDelegate;
class AutocompleteController;
class FaviconCache;
class BitmapFetcher;
class Profile;

namespace app_list {

class OmniboxResult : public ChromeSearchResult,
                      public BitmapFetcherDelegate,
                      public ash::ColorModeObserver {
 public:
  OmniboxResult(Profile* profile,
                AppListControllerDelegate* list_controller,
                AutocompleteController* autocomplete_controller,
                FaviconCache* favicon_cache,
                const AutocompleteInput& input,
                const AutocompleteMatch& match,
                bool is_zero_suggestion);
  ~OmniboxResult() override;

  OmniboxResult(const OmniboxResult&) = delete;
  OmniboxResult& operator=(const OmniboxResult&) = delete;

  // ChromeSearchResult:
  void Open(int event_flags) override;
  void InvokeAction(ash::SearchResultActionType action) override;

  // BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  int dedup_priority() const { return dedup_priority_; }

 private:
  // ash::ColorModeObserver:
  void OnColorModeChanged(bool dark_mode_enabled) override;

  void UpdateIcon();
  // Creates a generic backup icon: used when rich icons are not available.
  void SetGenericIcon();
  void UpdateTitleAndDetails();

  void Remove();

  // Returns true if |match_| indicates a url search result with non-empty
  // description.
  bool IsUrlResultWithDescription() const;

  // Returns true if |match| has an image url.
  bool IsRichEntity() const;
  void FetchRichEntityImage(const GURL& url);

  void OnFaviconFetched(const gfx::Image& icon);

  void InitializeButtonActions(
      const std::vector<ash::SearchResultActionType>& button_actions);

  ash::SearchResultType GetSearchResultType() const;

  // Indicates the priority of a result for deduplicatin. Results with the same
  // ID but lower |dedup_priority| are removed.
  int dedup_priority_ = 0;

  Profile* profile_;
  AppListControllerDelegate* list_controller_;
  AutocompleteController* autocomplete_controller_;
  FaviconCache* favicon_cache_;
  AutocompleteMatch match_;
  const bool is_zero_suggestion_;
  std::unique_ptr<BitmapFetcher> bitmap_fetcher_;
  // Whether this omnibox result uses a generic backup icon.
  bool uses_generic_icon_ = false;

  base::WeakPtrFactory<OmniboxResult> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_RESULT_H_
