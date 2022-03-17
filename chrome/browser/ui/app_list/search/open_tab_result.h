// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_OPEN_TAB_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_OPEN_TAB_RESULT_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AppListControllerDelegate;
class FaviconCache;
class Profile;

namespace app_list {

// Open tab search results. This is produced by the OmniboxProvider.
class OpenTabResult : public ChromeSearchResult {
 public:
  OpenTabResult(Profile* profile,
                AppListControllerDelegate* list_controller,
                FaviconCache* favicon_cache,
                const chromeos::string_matching::TokenizedString& query,
                const AutocompleteMatch& match);
  ~OpenTabResult() override;

  OpenTabResult(const OpenTabResult&) = delete;
  OpenTabResult& operator=(const OpenTabResult&) = delete;

  // ChromeSearchResult:
  void Open(int event_flags) override;
  absl::optional<std::string> DriveId() const override;

 private:
  void UpdateText();
  void UpdateIcon();
  void OnFaviconFetched(const gfx::Image& icon);

  Profile* profile_;
  AppListControllerDelegate* list_controller_;
  FaviconCache* favicon_cache_;
  AutocompleteMatch match_;
  absl::optional<std::string> drive_id_;

  base::WeakPtrFactory<OpenTabResult> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_OPEN_TAB_RESULT_H_
