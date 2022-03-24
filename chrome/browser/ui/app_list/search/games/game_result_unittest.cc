// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/games/game_result.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/common/search_result_util.h"
#include "chrome/browser/ui/app_list/search/games/stub_api.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  GameIndexManager index_manager;

  GameData game_data;
  game_data.title = u"Title";
  game_data.source = GameSource::kExampleSource;
  game_data.launch_url = GURL("https://test-url.com/");
  game_data.platforms = {u"A", u"B", u"C"};

  GameResult result(profile_.get(), &list_controller_, &index_manager,
                    game_data, 0.6, u"SomeGame");

  EXPECT_EQ(result.title(), u"Title");

  EXPECT_EQ(StringFromTextVector(result.details_text_vector()),
            base::StrCat({u"Example source - ",
                          l10n_util::GetStringUTF16(
                              IDS_APP_LIST_SEARCH_GAME_PLATFORMS_PREFIX),
                          u" A, B, C"}));

  EXPECT_EQ(result.accessible_name(),
            base::StrCat({u"Title, Example source, ",
                          l10n_util::GetStringUTF16(
                              IDS_APP_LIST_SEARCH_GAME_PLATFORMS_PREFIX),
                          u" A, B, C"}));
}

}  // namespace app_list
