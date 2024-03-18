// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/wallpaper/sea_pen_wallpaper_manager.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "ash/webui/personalization_app/test/personalization_app_mojom_banned_mocha_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_mocha_test_base.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/manta/features.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_unittest_util.h"

// TODO(b/312208348) move this test to ash common sea_pen browsertest.

namespace ash::personalization_app {

namespace {

std::string CreateJpgBytes() {
  SkBitmap bitmap = gfx::test::CreateBitmap(1);
  bitmap.allocN32Pixels(1, 1);
  std::vector<unsigned char> data;
  gfx::JPEGCodec::Encode(bitmap, /*quality=*/100, &data);
  return std::string(data.begin(), data.end());
}

}  // namespace

// Tests state management and logic in SeaPen.
using SeaPenControllerTest = PersonalizationAppMojomBannedMochaTestBase;

IN_PROC_BROWSER_TEST_F(SeaPenControllerTest, All) {
  RunTest("chromeos/personalization_app/sea_pen_controller_test.js",
          "mocha.run()");
}

// Tests the SeaPen UI.
// TODO(b/329149811) Add screenplay id
class PersonalizationAppSeaPenBrowserTest
    : public PersonalizationAppMochaTestBase {
 public:
  PersonalizationAppSeaPenBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {
            ::manta::features::kMantaService,
            ::ash::features::kSeaPen,
            ::ash::features::kFeatureManagementSeaPen,
        },
        {});
  }
  PersonalizationAppSeaPenBrowserTest(
      const PersonalizationAppSeaPenBrowserTest&) = delete;
  PersonalizationAppSeaPenBrowserTest& operator=(
      const PersonalizationAppSeaPenBrowserTest&) = delete;

  ~PersonalizationAppSeaPenBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PersonalizationAppMochaTestBase::SetUpOnMainThread();

    //  Creates a fake SeaPen image and saves it to disk.
    auto* sea_pen_wallpaper_manager = SeaPenWallpaperManager::GetInstance();
    DCHECK(sea_pen_wallpaper_manager);
    const AccountId account_id = GetAccountId(browser()->profile());
    const SeaPenImage sea_pen_image = {CreateJpgBytes(), 323};
    const base::flat_map<mojom::SeaPenTemplateChip, mojom::SeaPenTemplateOption>
        options({{mojom::SeaPenTemplateChip::kFlowerColor,
                  mojom::SeaPenTemplateOption::kFlowerColorBlue},
                 {mojom::SeaPenTemplateChip::kFlowerType,
                  mojom::SeaPenTemplateOption::kFlowerTypeRose}});
    const mojom::SeaPenQueryPtr search_query =
        mojom::SeaPenQuery::NewTemplateQuery(mojom::SeaPenTemplateQuery::New(
            mojom::SeaPenTemplateId::kFlower, options,
            mojom::SeaPenUserVisibleQuery::New("test template query",
                                               "test template title")));
    ASSERT_TRUE(wallpaper_handlers::IsValidTemplateQuery(
        search_query->get_template_query()));
    base::test::TestFuture<const gfx::ImageSkia&> save_image_future;
    sea_pen_wallpaper_manager->DecodeAndSaveSeaPenImage(
        account_id, sea_pen_image, std::move(search_query),
        save_image_future.GetCallback());
    ASSERT_FALSE(save_image_future.Get().isNull());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PersonalizationAppSeaPenBrowserTest, SeaPen) {
  RunTestWithoutTestLoader(
      "chromeos/personalization_app/personalization_app_test.js",
      "runMochaSuite('sea pen')");
}

}  // namespace ash::personalization_app
