// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/filtering_ranker.h"

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

namespace {

using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

class TestDriveIdResult : public ChromeSearchResult {
 public:
  TestDriveIdResult(const std::string& id,
                    ResultType type,
                    const absl::optional<std::string>& drive_id)
      : drive_id_(drive_id) {
    set_id(id);
    SetResultType(type);
  }

  ~TestDriveIdResult() override {}

  // ChromeSearchResult:
  void Open(int event_flags) override {}

  absl::optional<std::string> DriveId() const override { return drive_id_; }

 private:
  absl::optional<std::string> drive_id_;
};

Results MakeDriveIdResults(
    const std::vector<std::string>& ids,
    const std::vector<ResultType>& types,
    const std::vector<absl::optional<std::string>>& drive_ids) {
  CHECK_EQ(ids.size(), types.size());
  CHECK_EQ(ids.size(), drive_ids.size());

  Results res;
  for (size_t i = 0; i < ids.size(); ++i) {
    res.push_back(
        std::make_unique<TestDriveIdResult>(ids[i], types[i], drive_ids[i]));
  }
  return res;
}

}  // namespace

class FilteringRankerTest : public testing::Test {};

TEST_F(FilteringRankerTest, DeduplicateDriveFilesAndTabs) {
  auto drive = ResultType::kDriveSearch;
  auto web = ResultType::kOmnibox;
  auto tab = ResultType::kOpenTab;

  ResultsMap results;
  results[web] = MakeDriveIdResults({"a", "b", "c", "d"}, {web, tab, tab, tab},
                                    {absl::nullopt, "B", "C", absl::nullopt});
  results[drive] = MakeDriveIdResults({"a", "b", "d", "e", "f"},
                                      {drive, drive, drive, drive, drive},
                                      {"A", "B", "D", "E", absl::nullopt});

  FilteringRanker ranker;
  CategoriesList categories;
  ranker.Start(u"query", results, categories);
  ranker.UpdateResultRanks(results, ProviderType::kKeyboardShortcut);

  EXPECT_FALSE(results[drive][0]->scoring().filter);
  EXPECT_TRUE(results[drive][1]->scoring().filter);
  EXPECT_FALSE(results[drive][2]->scoring().filter);
  EXPECT_FALSE(results[drive][3]->scoring().filter);
  EXPECT_FALSE(results[drive][4]->scoring().filter);
}

}  // namespace app_list
