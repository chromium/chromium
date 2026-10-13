// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/icon_table.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/omnibox/browser/location_bar_model_util.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/security_state/core/security_state.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/vector_icons.h"

using toolbar_ui_api::mojom::IconType;

namespace webui_toolbar {

namespace {

class IconTableTest : public testing::Test, public IconTable::Delegate {
 public:
  IconTableTest() : icon_table_(this) {
    icon_table_.PermitFallbackVectorRasterizationForTesting();
    color_provider_.SetColorForTesting(ui::kColorIcon, SK_ColorBLUE);
    color_provider_.SetColorForTesting(ui::kColorMenuIcon, SK_ColorGREEN);
    color_provider_.SetColorForTesting(ui::kColorSysPrimary, SK_ColorCYAN);
  }

  // IconTable::Delegate:
  const ui::ColorProvider* GetColorProvider() const override {
    return &color_provider_;
  }

  float GetScaleFactor() const override { return scale_factor_; }

 protected:
  // Setup the browser task environment so thread-checkers for particular
  // browser threads function properly.
  content::BrowserTaskEnvironment task_environment_;
  ui::ColorProvider color_provider_;
  float scale_factor_ = 1.5f;

  IconTable icon_table_;
};

TEST_F(IconTableTest, BasicOperation) {
  // Assuming this icon isn't mapped.
  std::optional<toolbar_ui_api::IconHandle> maybe_i0 =
      icon_table_.RegisterVectorIcon(vector_icons::kCardboardFilledIcon);
  ASSERT_FALSE(maybe_i0.has_value());

  std::optional<toolbar_ui_api::IconHandle> maybe_i1 =
      icon_table_.RegisterVectorIcon(vector_icons::kPasswordManagerIcon);
  ASSERT_TRUE(maybe_i1.has_value());
  auto i1 = std::move(maybe_i1.value());

  auto expected_i1 = toolbar_ui_api::mojom::IconUpdate::New(
      1u, "webui-toolbar:password_manager", IconType::kIconSet,
      /*color=*/std::nullopt);

  // Binding i1 in both full state and pending updates.
  EXPECT_THAT(
      icon_table_.TakePendingUpdates(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i1))));
  EXPECT_THAT(
      icon_table_.GetFullState(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i1))));

  // An additional call to TakePendingUpdates() will be empty, but
  // GetFullState() will be the same.
  EXPECT_THAT(icon_table_.TakePendingUpdates(),
              testing::UnorderedElementsAre());
  EXPECT_THAT(
      icon_table_.GetFullState(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i1))));

  // Release i1, bind i2.
  i1 = toolbar_ui_api::IconHandle();
  auto expected_delete_i1 = toolbar_ui_api::mojom::IconUpdate::New(
      1u, std::nullopt, IconType::kMaskUrl, /*color=*/std::nullopt);

  std::optional<toolbar_ui_api::IconHandle> maybe_i2 =
      icon_table_.RegisterVectorIcon(vector_icons::kHistoryIcon);
  ASSERT_TRUE(maybe_i2.has_value());
  auto i2 = maybe_i2.value();

  auto expected_i2 = toolbar_ui_api::mojom::IconUpdate::New(
      2u, "webui-toolbar:history", IconType::kIconSet, /*color=*/std::nullopt);

  // Updates has delete of i1, and addition of i2; full state just i2.
  EXPECT_THAT(icon_table_.TakePendingUpdates(),
              testing::UnorderedElementsAre(
                  MatchesIconUpdate(std::ref(expected_delete_i1)),
                  MatchesIconUpdate(std::ref(expected_i2))));
  EXPECT_THAT(
      icon_table_.GetFullState(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i2))));

  // Now also add i3.
  std::optional<toolbar_ui_api::IconHandle> maybe_i3 =
      icon_table_.RegisterVectorIcon(kMenuBookIcon);
  ASSERT_TRUE(maybe_i3.has_value());
  auto i3 = maybe_i3.value();

  auto expected_i3 = toolbar_ui_api::mojom::IconUpdate::New(
      3u, "webui-toolbar:menu_book", IconType::kIconSet,
      /*color=*/std::nullopt);

  // Update has addition of i3; full state i2 + i3.
  EXPECT_THAT(
      icon_table_.TakePendingUpdates(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i3))));
  EXPECT_THAT(
      icon_table_.GetFullState(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i2)),
                                    MatchesIconUpdate(std::ref(expected_i3))));
}

// Test accessing the state via the IconTableFetcher interface.
TEST_F(IconTableTest, MakeIconTableFetcher) {
  // Create a table on the heap to help test lifetime independence.
  auto icon_table = std::make_unique<IconTable>(this);
  auto icon_table_fetcher = icon_table->MakeIconTableFetcher();

  std::optional<toolbar_ui_api::IconHandle> maybe_i1 =
      icon_table->RegisterVectorIcon(vector_icons::kPasswordManagerIcon);
  ASSERT_TRUE(maybe_i1.has_value());
  auto i1 = maybe_i1.value();
  auto expected_i1 = toolbar_ui_api::mojom::IconUpdate::New(
      1u, "webui-toolbar:password_manager", IconType::kIconSet,
      /*color=*/std::nullopt);

  // Binding i1 in both full state and pending updates.
  EXPECT_THAT(
      icon_table_fetcher->TakePendingUpdates(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i1))));
  EXPECT_THAT(
      icon_table_fetcher->GetFullState(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i1))));

  // Now add i2.
  std::optional<toolbar_ui_api::IconHandle> maybe_i2 =
      icon_table->RegisterVectorIcon(vector_icons::kHistoryIcon);
  ASSERT_TRUE(maybe_i2.has_value());
  auto i2 = maybe_i2.value();

  auto expected_i2 = toolbar_ui_api::mojom::IconUpdate::New(
      2u, "webui-toolbar:history", IconType::kIconSet, /*color=*/std::nullopt);

  // Pending has i2, full state has both.
  EXPECT_THAT(
      icon_table_fetcher->TakePendingUpdates(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i2))));
  EXPECT_THAT(
      icon_table_fetcher->GetFullState(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i1)),
                                    MatchesIconUpdate(std::ref(expected_i2))));

  icon_table.reset();
  // With table destroyed, these methods are still safe to call, just return
  // empty things.
  EXPECT_TRUE(icon_table_fetcher->GetFullState().empty());
  EXPECT_TRUE(icon_table_fetcher->TakePendingUpdates().empty());
}

TEST_F(IconTableTest, RegisterImageModel) {
  std::optional<toolbar_ui_api::IconHandle> maybe_i0 =
      icon_table_.RegisterVectorIcon(vector_icons::kCardboardFilledIcon);
  ASSERT_FALSE(maybe_i0.has_value())
      << "This test assumes the kCardboardFilledIcon isn't mapped";

  // Despite the icon not being mapped, we can bind; it'll end up a
  // data: PNG.
  toolbar_ui_api::IconHandle i1 = icon_table_.RegisterImageModel(
      ui::ImageModel::FromVectorIcon(vector_icons::kCardboardFilledIcon));
  ASSERT_FALSE(i1.is_null());

  toolbar_ui_api::IconHandle i2 = icon_table_.RegisterImageModel(
      ui::ImageModel::FromVectorIcon(vector_icons::kPasswordManagerIcon));
  ASSERT_FALSE(i2.is_null());
  // kColorMenuIcon is default, and we set it to green.
  auto expected_i2 = toolbar_ui_api::mojom::IconUpdate::New(
      2u, "webui-toolbar:password_manager", IconType::kIconSet,
      /*color=*/SK_ColorGREEN);

  EXPECT_THAT(
      icon_table_.GetFullState(),
      testing::UnorderedElementsAre(MatchesBitmapIconUpdate(1u),
                                    MatchesIconUpdate(std::ref(expected_i2))));
}

TEST_F(IconTableTest, RegisterColorUrl) {
  toolbar_ui_api::IconHandle i1 = icon_table_.RegisterColorUrl("rainbow.png");
  ASSERT_FALSE(i1.is_null());

  auto expected_i1 = toolbar_ui_api::mojom::IconUpdate::New(
      1u, "rainbow.png", IconType::kFullColorUrl, /*color=*/std::nullopt);

  EXPECT_THAT(
      icon_table_.GetFullState(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i1))));
}

TEST_F(IconTableTest, RegisterImageModelTryReuse) {
  std::optional<toolbar_ui_api::IconHandle> maybe_i0 =
      icon_table_.RegisterVectorIcon(vector_icons::kCardboardFilledIcon);
  ASSERT_FALSE(maybe_i0.has_value())
      << "This test assumes the kCardboardFilledIcon isn't mapped";

  // Start from an empty previous value.
  toolbar_ui_api::IconHandle i1 = icon_table_.RegisterImageModelTryReuse(
      ui::ImageModel::FromVectorIcon(vector_icons::kPasswordManagerIcon),
      toolbar_ui_api::IconHandle());
  ASSERT_FALSE(i1.is_null());
  auto expected_i1 = toolbar_ui_api::mojom::IconUpdate::New(
      1u, "webui-toolbar:password_manager", IconType::kIconSet,
      /*color=*/SK_ColorGREEN);

  // Now try to create the same one.
  toolbar_ui_api::IconHandle i2 = icon_table_.RegisterImageModelTryReuse(
      ui::ImageModel::FromVectorIcon(vector_icons::kPasswordManagerIcon), i1);
  ASSERT_FALSE(i2.is_null());
  // Should get the same handle.
  EXPECT_EQ(i1, i2);

  // Now a different one...
  toolbar_ui_api::IconHandle i3 = icon_table_.RegisterImageModelTryReuse(
      ui::ImageModel::FromVectorIcon(vector_icons::kCardboardFilledIcon), i2);
  ASSERT_FALSE(i3.is_null());
  EXPECT_NE(i2, i3);

  // And can reuse though it's rasterized.
  toolbar_ui_api::IconHandle i4 = icon_table_.RegisterImageModelTryReuse(
      ui::ImageModel::FromVectorIcon(vector_icons::kCardboardFilledIcon), i3);
  ASSERT_FALSE(i4.is_null());
  EXPECT_EQ(i3, i4);

  // Distinct gfx::Image instances with identical bitmap contents should be
  // reused via deep comparison.
  SkBitmap bitmap1;
  bitmap1.allocN32Pixels(16, 16);
  bitmap1.eraseColor(SK_ColorRED);

  SkBitmap bitmap2;
  bitmap2.allocN32Pixels(16, 16);
  bitmap2.eraseColor(SK_ColorRED);

  gfx::Image img1 = gfx::Image::CreateFrom1xBitmap(bitmap1);
  gfx::Image img2 = gfx::Image::CreateFrom1xBitmap(bitmap2);

  // Verify that operator== returns false for distinct gfx::Image instances.
  EXPECT_NE(img1, img2);
  EXPECT_NE(ui::ImageModel::FromImage(img1), ui::ImageModel::FromImage(img2));

  toolbar_ui_api::IconHandle img_handle1 =
      icon_table_.RegisterImageModelTryReuse(ui::ImageModel::FromImage(img1),
                                             toolbar_ui_api::IconHandle());
  ASSERT_FALSE(img_handle1.is_null());

  toolbar_ui_api::IconHandle img_handle2 =
      icon_table_.RegisterImageModelTryReuse(ui::ImageModel::FromImage(img2),
                                             img_handle1);
  ASSERT_FALSE(img_handle2.is_null());
  EXPECT_EQ(img_handle1, img_handle2);

  EXPECT_THAT(icon_table_.GetFullState(),
              testing::UnorderedElementsAre(
                  MatchesIconUpdate(std::ref(expected_i1)),
                  MatchesBitmapIconUpdate(2u), MatchesBitmapIconUpdate(3u)));
}

TEST_F(IconTableTest, ScaleFactorChange) {
  std::optional<toolbar_ui_api::IconHandle> maybe_i0 =
      icon_table_.RegisterVectorIcon(vector_icons::kCardboardFilledIcon);
  ASSERT_FALSE(maybe_i0.has_value())
      << "This test assumes the kCardboardFilledIcon isn't mapped";

  toolbar_ui_api::IconHandle i1 = icon_table_.RegisterImageModel(
      ui::ImageModel::FromVectorIcon(vector_icons::kCardboardFilledIcon));
  ASSERT_FALSE(i1.is_null());

  std::optional<toolbar_ui_api::IconHandle> maybe_i2 =
      icon_table_.RegisterVectorIcon(vector_icons::kPasswordManagerIcon);
  ASSERT_TRUE(maybe_i2.has_value());
  auto i2 = maybe_i2.value();
  auto expected_i2 = toolbar_ui_api::mojom::IconUpdate::New(
      2u, "webui-toolbar:password_manager", IconType::kIconSet,
      /*color=*/std::nullopt);

  EXPECT_THAT(
      icon_table_.TakePendingUpdates(),
      testing::UnorderedElementsAre(MatchesBitmapIconUpdate(1u),
                                    MatchesIconUpdate(std::ref(expected_i2))));

  scale_factor_ = 2.0f;

  // Changing scale factor should re-upload the bitmap icon, but not the vector
  // one.
  EXPECT_THAT(icon_table_.TakePendingUpdates(),
              testing::UnorderedElementsAre(MatchesBitmapIconUpdate(1u)));

  // Changing scale factor should not re-upload new things twice.
  toolbar_ui_api::IconHandle i3 = icon_table_.RegisterImageModel(
      ui::ImageModel::FromVectorIcon(vector_icons::kCardboardFilledIcon));
  ASSERT_FALSE(i3.is_null());

  scale_factor_ = 3.0f;
  EXPECT_THAT(icon_table_.TakePendingUpdates(),
              testing::UnorderedElementsAre(MatchesBitmapIconUpdate(1u),
                                            MatchesBitmapIconUpdate(3u)));
}

TEST_F(IconTableTest, Colors) {
  toolbar_ui_api::IconHandle i1 =
      icon_table_.RegisterImageModel(ui::ImageModel::FromVectorIcon(
          vector_icons::kPasswordManagerIcon, ui::kColorIcon));
  ASSERT_FALSE(i1.is_null());
  auto expected_i1 = toolbar_ui_api::mojom::IconUpdate::New(
      1u, "webui-toolbar:password_manager", IconType::kIconSet,
      /*color=*/SK_ColorBLUE);

  toolbar_ui_api::IconHandle i2 =
      icon_table_.RegisterImageModel(ui::ImageModel::FromVectorIcon(
          vector_icons::kPasswordManagerIcon, ui::kColorMenuIcon));
  ASSERT_FALSE(i2.is_null());
  auto expected_i2 = toolbar_ui_api::mojom::IconUpdate::New(
      2u, "webui-toolbar:password_manager", IconType::kIconSet,
      /*color=*/SK_ColorGREEN);

  toolbar_ui_api::IconHandle i3 =
      icon_table_.RegisterImageModel(ui::ImageModel::FromVectorIcon(
          omnibox::kStarFilledIcon, ui::kColorSysPrimary));
  ASSERT_FALSE(i3.is_null());
  auto expected_i3 = toolbar_ui_api::mojom::IconUpdate::New(
      3u, "webui-toolbar:star_filled", IconType::kIconSet,
      /*color=*/SK_ColorCYAN);

  EXPECT_THAT(
      icon_table_.GetFullState(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i1)),
                                    MatchesIconUpdate(std::ref(expected_i2)),
                                    MatchesIconUpdate(std::ref(expected_i3))));
}

TEST_F(IconTableTest, OnThemeChanged) {
  // Register a rasterized bitmap icon (need_rasterize = true)
  toolbar_ui_api::IconHandle i1 = icon_table_.RegisterImageModel(
      ui::ImageModel::FromVectorIcon(vector_icons::kCardboardFilledIcon));
  ASSERT_FALSE(i1.is_null());

  // Register a mapped vector icon with a color provider color
  toolbar_ui_api::IconHandle i2 =
      icon_table_.RegisterImageModel(ui::ImageModel::FromVectorIcon(
          vector_icons::kPasswordManagerIcon, ui::kColorIcon));
  ASSERT_FALSE(i2.is_null());

  // Consume initial pending updates
  auto expected_i2_initial = toolbar_ui_api::mojom::IconUpdate::New(
      2u, "webui-toolbar:password_manager", IconType::kIconSet,
      /*color=*/SK_ColorBLUE);

  EXPECT_THAT(icon_table_.TakePendingUpdates(),
              testing::UnorderedElementsAre(
                  MatchesBitmapIconUpdate(1u),
                  MatchesIconUpdate(std::ref(expected_i2_initial))));
  EXPECT_TRUE(icon_table_.TakePendingUpdates().empty());

  // Update color provider colors to simulate a theme change (e.g. dark mode)
  color_provider_.SetColorForTesting(ui::kColorIcon, SK_ColorRED);

  // Notify icon table that the theme changed
  icon_table_.OnThemeChanged();

  // Verify TakePendingUpdates() re-evaluates all registered icons and returns
  // updated rasterization / color data for both bitmap and vector icons.
  auto expected_i2_updated = toolbar_ui_api::mojom::IconUpdate::New(
      2u, "webui-toolbar:password_manager", IconType::kIconSet,
      /*color=*/SK_ColorRED);

  EXPECT_THAT(icon_table_.TakePendingUpdates(),
              testing::UnorderedElementsAre(
                  MatchesBitmapIconUpdate(1u),
                  MatchesIconUpdate(std::ref(expected_i2_updated))));

  // A subsequent call to TakePendingUpdates() without changes must be empty,
  // confirming rasterized_scale_ was maintained and pending updates cleared.
  EXPECT_TRUE(icon_table_.TakePendingUpdates().empty());
}

TEST_F(IconTableTest, ForceRerasterize) {
  // Register a rasterized bitmap icon whose rasterization depends on
  // GetColorProvider().
  toolbar_ui_api::IconHandle i1 =
      icon_table_.RegisterImageModel(ui::ImageModel::FromVectorIcon(
          vector_icons::kCardboardFilledIcon, ui::kColorIcon));
  ASSERT_FALSE(i1.is_null());

  // First call to TakePendingUpdates() invokes ToMojom(), rasterizing with
  // initial colors and setting rasterized_scale_.
  auto updates1 = icon_table_.TakePendingUpdates();
  ASSERT_EQ(1u, updates1.size());
  std::string initial_png_data_url = updates1[0]->icon_url_or_name.value();
  EXPECT_TRUE(base::StartsWith(initial_png_data_url, "data:image/png;base64"));

  // Change ColorProvider colors to simulate theme switch.
  color_provider_.SetColorForTesting(ui::kColorIcon, SK_ColorGREEN);

  // Calling ToMojom() when rasterized_scale_ matches scale_factor_ must reuse
  // initial_png_data_url without re-rasterizing.
  auto full_state_before_reraster = icon_table_.GetFullState();
  ASSERT_EQ(1u, full_state_before_reraster.size());
  EXPECT_EQ(initial_png_data_url,
            full_state_before_reraster[0]->icon_url_or_name.value());

  // Trigger OnThemeChanged() which calls ForceRerasterize(), resetting
  // rasterized_scale_.
  icon_table_.OnThemeChanged();

  // TakePendingUpdates() now re-rasterizes with the updated ColorProvider
  // color.
  auto updates2 = icon_table_.TakePendingUpdates();
  ASSERT_EQ(1u, updates2.size());
  std::string rerasterized_png_data_url = updates2[0]->icon_url_or_name.value();
  EXPECT_TRUE(
      base::StartsWith(rerasterized_png_data_url, "data:image/png;base64"));

  // Verify that re-rasterization generated a new PNG data URL matching the new
  // color.
  EXPECT_NE(initial_png_data_url, rerasterized_png_data_url);
}

TEST_F(IconTableTest, AllSecurityIconsAreMapped) {
  const security_state::SecurityLevel levels[] = {
      security_state::NONE,
      security_state::WARNING,
      security_state::SECURE,
      security_state::DANGEROUS,
  };

  for (auto level : levels) {
    // Standard icon
    {
      security_state::VisibleSecurityState state;
      const gfx::VectorIcon& standard_icon =
          location_bar_model::GetSecurityVectorIcon(level, &state);
      EXPECT_TRUE(icon_table_.RegisterVectorIcon(standard_icon).has_value())
          << "Missing standard icon for SecurityLevel: " << level;
    }

    // HTTPS Upgraded (WARNING triggers no_encryption)
    {
      security_state::VisibleSecurityState state;
      state.is_https_only_mode_upgraded = true;
      const gfx::VectorIcon& https_upgraded_icon =
          location_bar_model::GetSecurityVectorIcon(level, &state);
      EXPECT_TRUE(
          icon_table_.RegisterVectorIcon(https_upgraded_icon).has_value())
          << "Missing HTTPS upgraded icon for SecurityLevel: " << level;
    }

    // Enterprise block (DANGEROUS triggers domain/business)
    {
      security_state::VisibleSecurityState state;
      state.malicious_content_status =
          security_state::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_BLOCK;
      const gfx::VectorIcon& enterprise_icon =
          location_bar_model::GetSecurityVectorIcon(level, &state);
      EXPECT_TRUE(icon_table_.RegisterVectorIcon(enterprise_icon).has_value())
          << "Missing enterprise block icon for SecurityLevel: " << level;
    }

    // Suspicious site (DANGEROUS triggers shield_question)
    {
      security_state::VisibleSecurityState state;
      state.malicious_content_status =
          security_state::MALICIOUS_CONTENT_STATUS_WARNABLE_SUSPICIOUS_SITE;
      const gfx::VectorIcon& suspicious_site_icon =
          location_bar_model::GetSecurityVectorIcon(level, &state);
      EXPECT_TRUE(
          icon_table_.RegisterVectorIcon(suspicious_site_icon).has_value())
          << "Missing suspicious site icon for SecurityLevel: " << level;
    }
  }
}

TEST_F(IconTableTest, ExtensionIconsAreMapped) {
  const gfx::VectorIcon* extension_icons[] = {
      &omnibox::kExtensionFilledIcon,
      &omnibox::kExtensionAppOldIcon,
      &vector_icons::kExtensionFilledIcon,
      &vector_icons::kExtensionOldIcon,
      &vector_icons::kExtensionChromeRefreshOldIcon,
      &vector_icons::kChromeExtensionIcon,
      &vector_icons::kChromeExtensionCheckIcon,
      &vector_icons::kChromeExtensionOffIcon,
      &vector_icons::kExtensionOffOldIcon,
      &vector_icons::kExtensionOnOldIcon,
  };

  for (const gfx::VectorIcon* icon : extension_icons) {
    EXPECT_TRUE(icon_table_.RegisterVectorIcon(*icon).has_value())
        << "Missing mapping for extension icon: "
        << (icon->name ? icon->name : "(null)");
  }
}

}  // namespace

}  // namespace webui_toolbar
