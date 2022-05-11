// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/games/game_result.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service_factory.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/apps/app_discovery_service/result.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/common/search_result_util.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace app_list {

class GameResultTest : public testing::Test {
 public:
  GameResultTest() { profile_ = std::make_unique<TestingProfile>(); }

  ~GameResultTest() override = default;

  content::BrowserTaskEnvironment task_environment_;
  test::TestAppListControllerDelegate list_controller_;
  std::unique_ptr<Profile> profile_;
};

TEST_F(GameResultTest, Basic) {
  auto* app_discovery_service =
      apps::AppDiscoveryServiceFactory::GetForProfile(profile_.get());

  apps::Result apps_result(
      apps::AppSource::kGames, "12345", u"Title",
      std::make_unique<apps::GameExtras>(
          absl::make_optional(std::vector<std::u16string>({u"A", u"B", u"C"})),
          u"SourceName", u"TestGamePublisher",
          base::FilePath("/icons/test.png"),
          /*is_icon_masking_allowed=*/false, GURL("https://game.com/game")));

  GameResult result(profile_.get(), &list_controller_, app_discovery_service,
                    apps_result, 0.6, u"SomeGame");

  EXPECT_EQ(result.title(), u"Title");
  EXPECT_EQ(StringFromTextVector(result.details_text_vector()), u"SourceName");
  EXPECT_EQ(result.accessible_name(), u"Title, SourceName");
}

}  // namespace app_list
