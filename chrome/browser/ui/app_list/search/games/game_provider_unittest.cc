// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/games/game_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_util.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/apps/app_discovery_service/result.h"
#include "chrome/browser/ui/app_list/search/search_features.h"
#include "chrome/browser/ui/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace app_list {
namespace {

using ::base::test::ScopedFeatureList;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

MATCHER_P(Title, title, "") {
  return arg->title() == title;
}

apps::Result MakeAppsResult(const std::u16string& title,
                            const std::u16string& source) {
  return apps::Result(
      apps::AppSource::kGames, "12345", title,
      std::make_unique<apps::GameExtras>(
          absl::make_optional(std::vector<std::u16string>({u"A", u"B", u"C"})),
          source, u"TestGamePublisher", base::FilePath("/icons/test.png"),
          /*is_icon_masking_allowed=*/false, GURL("https://game.com/game")));
}

apps::Result MakeAppsResult(const std::u16string& title) {
  return MakeAppsResult(title, u"SourceName");
}

// Checks that the result's details text vector contains exactly one text item
// with the given text.
bool DetailsEquals(const std::unique_ptr<ChromeSearchResult>& result,
                   const std::u16string& details) {
  const auto& details_vector = result->details_text_vector();

  if (details_vector.size() != 1u)
    return false;

  if (details_vector[0].GetType() != ash::SearchResultTextItemType::kString)
    return false;

  return details_vector[0].GetText() == details;
}

}  // namespace

// Parameterized by the ItemSuggest "enabled_override" parameter.
class GameProviderTest : public testing::Test,
                         public testing::WithParamInterface<bool> {
 public:
  GameProviderTest() {
    bool enabled_override = GetParam();
    std::vector<ScopedFeatureList::FeatureAndParams> enabled_features = {
        {ash::features::kProductivityLauncher, {}},
        {search_features::kLauncherGameSearch,
         {{"enabled_override", enabled_override ? "true" : "false"}}}};
    feature_list_.InitWithFeaturesAndParameters(
        enabled_features, std::vector<base::test::FeatureRef>());
  }

 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    auto provider =
        std::make_unique<GameProvider>(profile_.get(), &list_controller_);
    provider_ = provider.get();

    search_controller_ = std::make_unique<TestSearchController>();
    search_controller_->AddProvider(0, std::move(provider));
  }

  const SearchProvider::Results& LastResults() {
    if (app_list_features::IsCategoricalSearchEnabled()) {
      return search_controller_->last_results();
    } else {
      return provider_->results();
    }
  }

  void SetUpTestingIndex() {
    GameProvider::GameIndex index;
    index.push_back(MakeAppsResult(u"First Title"));
    index.push_back(MakeAppsResult(u"Second Title"));
    index.push_back(MakeAppsResult(u"Third Title"));
    provider_->SetGameIndexForTest(std::move(index));
  }

  void StartSearch(const std::u16string& query) {
    search_controller_->StartSearch(query);
    task_environment_.RunUntilIdle();
  }

  GameProvider* provider() const { return provider_; }

  ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  ::test::TestAppListControllerDelegate list_controller_;
  std::unique_ptr<TestSearchController> search_controller_;
  std::unique_ptr<Profile> profile_;

  GameProvider* provider_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(ProductivityLauncher,
                         GameProviderTest,
                         testing::Bool());

TEST_P(GameProviderTest, SearchResultsMatchQuery) {
  SetUpTestingIndex();

  StartSearch(u"first");
  EXPECT_THAT(LastResults(), ElementsAre(Title(u"First Title")));

  StartSearch(u"title");
  EXPECT_THAT(LastResults(), UnorderedElementsAre(Title(u"First Title"),
                                                  Title(u"Second Title"),
                                                  Title(u"Third Title")));
}

// Tests that scores are not greatly affected by characters such as apostrophe.
TEST_P(GameProviderTest, SpecialCharactersIgnored) {
  GameProvider::GameIndex index;
  index.push_back(MakeAppsResult(u"titles one"));
  index.push_back(MakeAppsResult(u"title's one"));
  provider()->SetGameIndexForTest(std::move(index));

  // Expect that the results have similar scores.
  StartSearch(u"titles");
  ASSERT_EQ(LastResults().size(), 2u);
  double score_diff =
      abs(LastResults()[0]->relevance() - LastResults()[1]->relevance());
  EXPECT_LT(score_diff, 0.01);

  StartSearch(u"title's");
  ASSERT_EQ(LastResults().size(), 2u);
  score_diff =
      abs(LastResults()[0]->relevance() - LastResults()[1]->relevance());
  EXPECT_LT(score_diff, 0.01);
}

TEST_P(GameProviderTest, Policy) {
  SetUpTestingIndex();

  // Results should exist if Suggested Content is enabled.
  profile_->GetPrefs()->SetBoolean(chromeos::prefs::kSuggestedContentEnabled,
                                   true);
  StartSearch(u"first");
  EXPECT_THAT(LastResults(), ElementsAre(Title(u"First Title")));

  // If Suggested Content is disabled, only show results if the override is on.
  profile_->GetPrefs()->SetBoolean(chromeos::prefs::kSuggestedContentEnabled,
                                   false);
  StartSearch(u"first");
  bool enabled_override = GetParam();
  if (enabled_override) {
    EXPECT_THAT(LastResults(), ElementsAre(Title(u"First Title")));
  } else {
    EXPECT_TRUE(LastResults().empty());
  }
}

// Tests that games with the same title but different sources appear in a random
// order across different queries.
TEST_P(GameProviderTest, RandomizeSourceOrder) {
  // Create two games with the same name but different sources.
  GameProvider::GameIndex index;
  index.push_back(MakeAppsResult(u"title", u"source_a"));
  index.push_back(MakeAppsResult(u"title", u"source_b"));
  provider()->SetGameIndexForTest(std::move(index));

  int a_first = 0;
  int b_first = 0;
  for (int i = 0; i < 1000; ++i) {
    StartSearch(u"title");
    ASSERT_EQ(LastResults().size(), 2u);

    // The source name is set into the result details, so use the result details
    // to identify which source it came from.
    if (DetailsEquals(LastResults()[0], u"source_a")) {
      a_first++;
    } else if (DetailsEquals(LastResults()[0], u"source_b")) {
      b_first++;
    }
  }
  ASSERT_EQ(a_first + b_first, 1000);

  // We expect a and b each to be first about ~half the time, but this will vary
  // randomly across test runs. To avoid flakiness, only expect here that they
  // each happen at least 10 times, which has a very high chance of being true.
  EXPECT_GE(a_first, 10);
  EXPECT_GE(b_first, 10);
}

}  // namespace app_list
