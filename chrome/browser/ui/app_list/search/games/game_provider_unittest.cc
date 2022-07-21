// Copyright 2022 The Chromium Authors. All rights reserved.
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

apps::Result MakeAppsResult(const std::u16string& title) {
  return apps::Result(
      apps::AppSource::kGames, "12345", title,
      std::make_unique<apps::GameExtras>(
          absl::make_optional(std::vector<std::u16string>({u"A", u"B", u"C"})),
          u"SourceName", u"TestGamePublisher",
          base::FilePath("/icons/test.png"), /*is_icon_masking_allowed=*/false,
          GURL("https://game.com/game")));
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
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                std::vector<base::Feature>());
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

  void Wait() { task_environment_.RunUntilIdle(); }

  void StartSearch(const std::u16string& query) {
    search_controller_->StartSearch(query);
  }

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
  Wait();
  EXPECT_THAT(LastResults(), ElementsAre(Title(u"First Title")));

  StartSearch(u"title");
  Wait();
  EXPECT_THAT(LastResults(), UnorderedElementsAre(Title(u"First Title"),
                                                  Title(u"Second Title"),
                                                  Title(u"Third Title")));
}

TEST_P(GameProviderTest, Policy) {
  SetUpTestingIndex();

  // Results should exist if Suggested Content is enabled.
  profile_->GetPrefs()->SetBoolean(chromeos::prefs::kSuggestedContentEnabled,
                                   true);
  StartSearch(u"first");
  Wait();
  EXPECT_THAT(LastResults(), ElementsAre(Title(u"First Title")));

  // If Suggested Content is disabled, only show results if the override is on.
  profile_->GetPrefs()->SetBoolean(chromeos::prefs::kSuggestedContentEnabled,
                                   false);
  StartSearch(u"first");
  Wait();
  bool enabled_override = GetParam();
  if (enabled_override) {
    EXPECT_THAT(LastResults(), ElementsAre(Title(u"First Title")));
  } else {
    EXPECT_TRUE(LastResults().empty());
  }
}

}  // namespace app_list
