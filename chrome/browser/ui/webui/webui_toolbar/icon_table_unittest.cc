// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/icon_table.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/vector_icons.h"

using toolbar_ui_api::mojom::IconType;

namespace webui_toolbar {

namespace {

class IconTableTest : public testing::Test, public IconTable::Delegate {
 public:
  IconTableTest() : icon_table_(this) {
    icon_table_.PermitFallbackVectorRasterizationForTesting();
  }

  // IconTable::Delegate:
  const ui::ColorProvider* GetColorProvider() const override {
    return &color_provider_;
  }

  float GetScaleFactor() const override { return scale_factor_; }

 protected:
  ui::ColorProvider color_provider_;
  float scale_factor_ = 1.5f;

  IconTable icon_table_;
};

TEST_F(IconTableTest, BasicOperation) {
  // Assuming this icon isn't mapped.
  toolbar_ui_api::IconHandle i0 =
      icon_table_.RegisterVectorIcon(vector_icons::kVrHeadsetOldIcon);
  ASSERT_TRUE(i0.is_null());

  toolbar_ui_api::IconHandle i1 =
      icon_table_.RegisterVectorIcon(vector_icons::kPasswordManagerOldIcon);
  ASSERT_FALSE(i1.is_null());

  auto expected_i1 = toolbar_ui_api::mojom::IconUpdate::New(
      1u, "rhs_icons/password_manager.svg", IconType::kMaskUrl);

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
      1u, std::nullopt, IconType::kMaskUrl);

  toolbar_ui_api::IconHandle i2 = icon_table_.RegisterVectorIcon(
      vector_icons::kHistoryChromeRefreshOldIcon);
  ASSERT_FALSE(i2.is_null());

  auto expected_i2 = toolbar_ui_api::mojom::IconUpdate::New(
      2u, "pinned-toolbar-action:SidePanelShowHistoryCluster",
      IconType::kIconSet);

  // Updates has delete of i1, and addition of i2; full state just i2.
  EXPECT_THAT(icon_table_.TakePendingUpdates(),
              testing::UnorderedElementsAre(
                  MatchesIconUpdate(std::ref(expected_delete_i1)),
                  MatchesIconUpdate(std::ref(expected_i2))));
  EXPECT_THAT(
      icon_table_.GetFullState(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i2))));

  // Now also add i3.
  toolbar_ui_api::IconHandle i3 =
      icon_table_.RegisterVectorIcon(kMenuBookChromeRefreshOldIcon);
  ASSERT_FALSE(i3.is_null());

  auto expected_i3 = toolbar_ui_api::mojom::IconUpdate::New(
      3u, "pinned-toolbar-action:SidePanelShowReadAnything",
      IconType::kIconSet);

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

  toolbar_ui_api::IconHandle i1 =
      icon_table->RegisterVectorIcon(vector_icons::kPasswordManagerOldIcon);
  ASSERT_FALSE(i1.is_null());
  auto expected_i1 = toolbar_ui_api::mojom::IconUpdate::New(
      1u, "rhs_icons/password_manager.svg", IconType::kMaskUrl);

  // Binding i1 in both full state and pending updates.
  EXPECT_THAT(
      icon_table_fetcher->TakePendingUpdates(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i1))));
  EXPECT_THAT(
      icon_table_fetcher->GetFullState(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i1))));

  // Now add i2.
  toolbar_ui_api::IconHandle i2 = icon_table->RegisterVectorIcon(
      vector_icons::kHistoryChromeRefreshOldIcon);
  ASSERT_FALSE(i2.is_null());

  auto expected_i2 = toolbar_ui_api::mojom::IconUpdate::New(
      2u, "pinned-toolbar-action:SidePanelShowHistoryCluster",
      IconType::kIconSet);

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
  toolbar_ui_api::IconHandle i0 =
      icon_table_.RegisterVectorIcon(vector_icons::kVrHeadsetOldIcon);
  ASSERT_TRUE(i0.is_null())
      << "This test assumes the kVrHeadsetOldIcon isn't mapped";

  // Despite the icon not being mapped, we can bind; it'll end up a
  // data: PNG.
  toolbar_ui_api::IconHandle i1 = icon_table_.RegisterImageModel(
      ui::ImageModel::FromVectorIcon(vector_icons::kVrHeadsetOldIcon));
  ASSERT_FALSE(i1.is_null());

  toolbar_ui_api::IconHandle i2 = icon_table_.RegisterImageModel(
      ui::ImageModel::FromVectorIcon(vector_icons::kPasswordManagerOldIcon));
  ASSERT_FALSE(i2.is_null());
  auto expected_i2 = toolbar_ui_api::mojom::IconUpdate::New(
      2u, "rhs_icons/password_manager.svg", IconType::kMaskUrl);

  EXPECT_THAT(
      icon_table_.GetFullState(),
      testing::UnorderedElementsAre(MatchesBitmapIconUpdate(1u),
                                    MatchesIconUpdate(std::ref(expected_i2))));
}

TEST_F(IconTableTest, RegisterColorUrl) {
  toolbar_ui_api::IconHandle i1 = icon_table_.RegisterColorUrl("rainbow.png");
  ASSERT_FALSE(i1.is_null());

  auto expected_i1 = toolbar_ui_api::mojom::IconUpdate::New(
      1u, "rainbow.png", IconType::kFullColorUrl);

  EXPECT_THAT(
      icon_table_.GetFullState(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i1))));
}

TEST_F(IconTableTest, RegisterImageModelTryReuse) {
  toolbar_ui_api::IconHandle i0 =
      icon_table_.RegisterVectorIcon(vector_icons::kVrHeadsetOldIcon);
  ASSERT_TRUE(i0.is_null())
      << "This test assumes the kVrHeadsetIcon isn't mapped";

  // Start from an empty previous value.
  toolbar_ui_api::IconHandle i1 = icon_table_.RegisterImageModelTryReuse(
      ui::ImageModel::FromVectorIcon(vector_icons::kPasswordManagerOldIcon),
      toolbar_ui_api::IconHandle());
  ASSERT_FALSE(i1.is_null());
  auto expected_i1 = toolbar_ui_api::mojom::IconUpdate::New(
      1u, "rhs_icons/password_manager.svg", IconType::kMaskUrl);

  // Now try to create the same one.
  toolbar_ui_api::IconHandle i2 = icon_table_.RegisterImageModelTryReuse(
      ui::ImageModel::FromVectorIcon(vector_icons::kPasswordManagerOldIcon),
      i1);
  ASSERT_FALSE(i2.is_null());
  // Should get the same handle.
  EXPECT_EQ(i1, i2);

  // Now a different one...
  toolbar_ui_api::IconHandle i3 = icon_table_.RegisterImageModelTryReuse(
      ui::ImageModel::FromVectorIcon(vector_icons::kVrHeadsetOldIcon), i2);
  ASSERT_FALSE(i3.is_null());
  EXPECT_NE(i2, i3);

  // And can reuse though it's rasterized.
  toolbar_ui_api::IconHandle i4 = icon_table_.RegisterImageModelTryReuse(
      ui::ImageModel::FromVectorIcon(vector_icons::kVrHeadsetOldIcon), i3);
  ASSERT_FALSE(i4.is_null());
  EXPECT_EQ(i3, i4);

  EXPECT_THAT(
      icon_table_.GetFullState(),
      testing::UnorderedElementsAre(MatchesIconUpdate(std::ref(expected_i1)),
                                    MatchesBitmapIconUpdate(2u)));
}

TEST_F(IconTableTest, ScaleFactorChange) {
  toolbar_ui_api::IconHandle i0 =
      icon_table_.RegisterVectorIcon(vector_icons::kVrHeadsetOldIcon);
  ASSERT_TRUE(i0.is_null())
      << "This test assumes the kVrHeadsetOldIcon isn't mapped";

  toolbar_ui_api::IconHandle i1 = icon_table_.RegisterImageModel(
      ui::ImageModel::FromVectorIcon(vector_icons::kVrHeadsetOldIcon));
  ASSERT_FALSE(i1.is_null());

  toolbar_ui_api::IconHandle i2 =
      icon_table_.RegisterVectorIcon(vector_icons::kPasswordManagerOldIcon);
  ASSERT_FALSE(i2.is_null());
  auto expected_i2 = toolbar_ui_api::mojom::IconUpdate::New(
      2u, "rhs_icons/password_manager.svg", IconType::kMaskUrl);

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
      ui::ImageModel::FromVectorIcon(vector_icons::kVrHeadsetOldIcon));
  ASSERT_FALSE(i3.is_null());

  scale_factor_ = 3.0f;
  EXPECT_THAT(icon_table_.TakePendingUpdates(),
              testing::UnorderedElementsAre(MatchesBitmapIconUpdate(1u),
                                            MatchesBitmapIconUpdate(3u)));
}

}  // namespace

}  // namespace webui_toolbar
