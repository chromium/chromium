// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_CHIP_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_CHIP_RANKER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_data.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

namespace app_list {

// A ChipRanker provides a method for ranking suggestion chips in the Chrome OS
// Launcher. Given a list of SortedResults from the Mixer, the ChipRanker will
// rescore the chip items so that they are appropriately ranked, while
// preserving the original ordering of all types of results.
//
// To combine the two app and file items, a type score is stored for the two
// categories 'apps' and 'files', tracking the user's overall usage of those
// categories. This is updated when results are launched. To produce a combined
// list of apps and files, we do the following:
//
// - Make a copy of the type scores: app_score and file_score.
// - Calculate delta = (app_score + file_score) / number_of_chips
// - Until we have number_of_chips results:
//   - Select the highest scoring unchosen app or file, depending on whether
//     app_score > file_score.
//   - Decrease the score of the selected type by delta.
//
// The types of the shown results reflect the proportion of the type scores and,
// as a type's score increases, its results appear closer to the front of the
// list. Note the implementation also handles the case of one type not having
// enough results.
class ChipRanker {
 public:
  explicit ChipRanker(Profile* profile);
  ~ChipRanker();

  ChipRanker(const ChipRanker&) = delete;
  ChipRanker& operator=(const ChipRanker&) = delete;

  // Train the ranker that compares the different result types.
  void Train(const AppLaunchData& app_launch_data);

  // Adjusts chip scores to fit in line with app scores using
  // ranking algorithm detailed above.
  void Rank(Mixer::SortedResults* results);

  // Get a pointer to the ranker for testing.
  RecurrenceRanker* GetRankerForTest();

  // Stores scores tracking a user's overall usage of apps or files.
  std::unique_ptr<RecurrenceRanker> type_ranker_;

 private:
  // Returns the number of chips available for ranked results. Accounts for the
  // release notes, continue reading, help app, assistant, and arc reinstall
  // chips.
  int NumAvailableChips(Mixer::SortedResults* results);

  Profile* profile_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_CHIP_RANKER_H_
