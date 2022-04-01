// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/games/game_provider.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service_factory.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/apps/app_discovery_service/result.h"
#include "chrome/browser/ui/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace app_list {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

MATCHER_P(Title, title, "") {
  return base::UTF16ToUTF8(arg->title()) == title;
}

apps::Result MakeAppsResult(const std::u16string& title) {
  return apps::Result(
      apps::AppSource::kGames, "12345", title,
      std::make_unique<apps::GameExtras>(
          absl::make_optional(std::vector<std::u16string>({u"A", u"B", u"C"})),
          apps::GameExtras::Source::kTestSource,
          GURL("https://icon-url.com/")));
}

}  // namespace

class GameProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    provider_ =
        std::make_unique<GameProvider>(profile_.get(), &list_controller_);

    search_controller_ = std::make_unique<TestSearchController>();
    provider_->set_controller(search_controller_.get());
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

  content::BrowserTaskEnvironment task_environment_;
  ::test::TestAppListControllerDelegate list_controller_;
  std::unique_ptr<TestSearchController> search_controller_;
  std::unique_ptr<Profile> profile_;

  std::unique_ptr<GameProvider> provider_;
};

// TODO(crbug.com/1305880): Enable this test once the app discovery service
// backend has been implemented.
TEST_F(GameProviderTest, DISABLED_SearchResultsMatchQuery) {
  SetUpTestingIndex();

  provider_->Start(u"first");
  Wait();
  EXPECT_THAT(LastResults(), ElementsAre(Title("First Title")));

  provider_->Start(u"title");
  Wait();
  EXPECT_THAT(LastResults(),
              UnorderedElementsAre(Title("First Title"), Title("Second Title"),
                                   Title("Third Title")));
}

}  // namespace app_list
