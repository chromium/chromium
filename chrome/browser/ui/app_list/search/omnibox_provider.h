// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_PROVIDER_H_

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AppListControllerDelegate;
class AutocompleteController;
class AutocompleteResult;
class Profile;

namespace app_list {

// OmniboxProvider wraps AutocompleteController to provide omnibox results.
class OmniboxProvider : public SearchProvider,
                        public AutocompleteController::Observer {
 public:
  explicit OmniboxProvider(Profile* profile,
                           AppListControllerDelegate* list_controller);

  OmniboxProvider(const OmniboxProvider&) = delete;
  OmniboxProvider& operator=(const OmniboxProvider&) = delete;

  ~OmniboxProvider() override;

  // SearchProvider overrides:
  void Start(const std::u16string& query) override;
  void StartZeroState() override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  // Populates result list from AutocompleteResult.
  void PopulateFromACResult(const AutocompleteResult& result);

  // AutocompleteController::Observer overrides:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  void RecordQueryLatencyHistogram();

  Profile* profile_;
  // True if the input is empty for zero state suggestion.
  bool is_zero_state_input_ = false;
  AppListControllerDelegate* list_controller_;

  std::u16string last_query_;
  absl::optional<chromeos::string_matching::TokenizedString>
      last_tokenized_query_;
  base::TimeTicks query_start_time_;
  AutocompleteInput input_;

  // The omnibox AutocompleteController that collects/sorts/dup-
  // eliminates the results as they come in.
  std::unique_ptr<AutocompleteController> controller_;

  // The AutocompleteController can sometimes update its results more than once
  // after reporting it is done. This flag is set to ensure we only update the
  // UI once.
  bool query_finished_ = false;

  FaviconCache favicon_cache_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_PROVIDER_H_
