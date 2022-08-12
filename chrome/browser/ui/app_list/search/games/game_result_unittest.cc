// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/games/game_result.h"

#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/apps/app_discovery_service/result.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/common/icon_constants.h"
#include "chrome/browser/ui/app_list/search/common/search_result_util.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"

namespace app_list {
namespace {

// Creates a 50x50 yellow test icon.
gfx::ImageSkia GetTestIcon() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(50, 50);
  bitmap.eraseColor(SK_ColorYELLOW);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Creates the resized non-maskable variant of the test icon, which is a 22x22
// yellow square inside a 32x32 circle.
gfx::ImageSkia GetExpectedNonMaskableIcon() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(22, 22);
  bitmap.eraseColor(SK_ColorYELLOW);
  return gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
      /*radius=*/16, SK_ColorWHITE, gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
}

// A mock app discovery service that can produce fake icons.
class TestAppDiscoveryService : public apps::AppDiscoveryService {
 public:
  explicit TestAppDiscoveryService(Profile* profile)
      : apps::AppDiscoveryService(profile) {}
  ~TestAppDiscoveryService() override = default;
  TestAppDiscoveryService(const TestAppDiscoveryService&) = delete;
  TestAppDiscoveryService& operator=(const TestAppDiscoveryService&) = delete;

  void GetIcon(const std::string& app_id,
               int32_t size_hint_in_dip,
               apps::ResultType result_type,
               apps::GetIconCallback callback) override {
    std::move(callback).Run(GetTestIcon(), apps::DiscoveryError::kSuccess);
  }
};

apps::Result MakeAppsResult(bool masking_allowed) {
  return apps::Result(
      apps::AppSource::kGames, "12345", u"Title",
      std::make_unique<apps::GameExtras>(
          absl::make_optional(std::vector<std::u16string>({u"A", u"B", u"C"})),
          u"SourceName", u"TestGamePublisher",
          base::FilePath("/icons/test.png"), masking_allowed,
          GURL("https://game.com/game")));
}

}  // namespace

class GameResultTest : public testing::Test {
 public:
  GameResultTest() {
    profile_ = std::make_unique<TestingProfile>();
    app_discovery_service_ =
        std::make_unique<TestAppDiscoveryService>(profile_.get());
  }

  ~GameResultTest() override = default;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<Profile> profile_;
  test::TestAppListControllerDelegate list_controller_;
  std::unique_ptr<TestAppDiscoveryService> app_discovery_service_;
};

TEST_F(GameResultTest, Basic) {
  apps::Result apps_result = MakeAppsResult(/*masking_allowed=*/false);
  GameResult result(profile_.get(), &list_controller_,
                    app_discovery_service_.get(), apps_result, 0.6,
                    u"SomeGame");

  EXPECT_EQ(result.title(), u"Title");
  EXPECT_EQ(StringFromTextVector(result.details_text_vector()), u"SourceName");
  EXPECT_EQ(result.accessible_name(), u"Title, SourceName");
}

TEST_F(GameResultTest, Icons) {
  apps::Result maskable_app = MakeAppsResult(/*masking_allowed=*/true);
  GameResult maskable_result(profile_.get(), &list_controller_,
                             app_discovery_service_.get(), maskable_app, 0.6,
                             u"SomeGame");

  EXPECT_EQ(maskable_result.icon().dimension, GetAppIconDimension());
  EXPECT_EQ(maskable_result.icon().shape, ash::SearchResultIconShape::kCircle);
  // The maskable icon should not be modified from its original form.
  EXPECT_TRUE(gfx::BitmapsAreEqual(*maskable_result.icon().icon.bitmap(),
                                   *GetTestIcon().bitmap()));

  apps::Result non_maskable_app = MakeAppsResult(/*masking_allowed=*/false);
  GameResult non_maskable_result(profile_.get(), &list_controller_,
                                 app_discovery_service_.get(), non_maskable_app,
                                 0.6, u"SomeGame");

  EXPECT_EQ(non_maskable_result.icon().dimension, GetAppIconDimension());
  EXPECT_EQ(non_maskable_result.icon().shape,
            ash::SearchResultIconShape::kCircle);
  // The non-maskable icon must be resized and placed on a white circle.
  EXPECT_TRUE(gfx::BitmapsAreEqual(*non_maskable_result.icon().icon.bitmap(),
                                   *GetExpectedNonMaskableIcon().bitmap()));
}

}  // namespace app_list
