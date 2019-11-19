// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_RESULT_H_

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "url/gurl.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {

// Result of AnswerCardSearchProvider.
class AnswerCardResult : public ChromeSearchResult {
 public:
  AnswerCardResult(Profile* profile,
                   AppListControllerDelegate* list_controller,
                   const GURL& potential_card_url,
                   const GURL& search_result_url,
                   const GURL& stripped_search_result_url);

  ~AnswerCardResult() override;

  void Open(int event_flags) override;

  ash::SearchResultType GetSearchResultType() const override;

  const GURL& search_result_url() const { return search_result_url_; }

 private:
  Profile* const profile_;                            // Unowned
  AppListControllerDelegate* const list_controller_;  // Unowned
  GURL search_result_url_;

  DISALLOW_COPY_AND_ASSIGN(AnswerCardResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_RESULT_H_
