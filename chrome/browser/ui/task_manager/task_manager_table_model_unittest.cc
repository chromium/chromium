// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/task_manager/task_manager_table_model.h"

#include <string>
#include <string_view>
#include <vector>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia.h"

namespace task_manager {

namespace {

bool ContainsSubstringsInOrder(std::u16string_view text,
                               const std::vector<std::u16string>& substrings) {
  for (const auto& substring : substrings) {
    size_t found_pos = text.find(substring);
    if (found_pos == std::u16string_view::npos) {
      return false;
    }
    text.remove_prefix(found_pos + substring.length());
  }
  return true;
}

// Returns a favicon-sized icon filled with |color|.
gfx::ImageSkia CreateSolidIcon(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

constexpr SkColor kDarkGray = SkColorSetRGB(0x20, 0x20, 0x20);

}  // namespace

class TaskManagerTableModelTest : public testing::Test {
 protected:
  using TaskIconThemeColors = TaskManagerTableModel::TaskIconThemeColors;

  static gfx::ImageSkia RasterizeThemedIcon(const gfx::ImageSkia& icon,
                                            SkColor icon_color,
                                            SkColor icon_background_color,
                                            SkColor row_background_color) {
    ui::ColorProvider color_provider;
    color_provider.SetColorForTesting(ui::kColorIcon, icon_color);
    color_provider.SetColorForTesting(ui::kColorTableIconBackground,
                                      icon_background_color);
    color_provider.SetColorForTesting(kColorTaskManagerTableBackground,
                                      row_background_color);
    return TaskManagerTableModel::RasterizeThemedIcon(
        /*model=*/nullptr, /*task_id=*/0, icon, &color_provider);
  }

  static gfx::ImageSkia RasterizeThemedIconWithoutColorProvider(
      const gfx::ImageSkia& icon) {
    return TaskManagerTableModel::RasterizeThemedIcon(
        /*model=*/nullptr, /*task_id=*/0, icon, /*color_provider=*/nullptr);
  }
};

TEST_F(TaskManagerTableModelTest, FormatListToString) {
  std::vector<std::u16string> tasks;

  EXPECT_EQ(TaskManagerTableModel::FormatListToString(tasks), std::u16string());

  tasks.push_back(u"task1");
  EXPECT_TRUE(ContainsSubstringsInOrder(
      TaskManagerTableModel::FormatListToString(tasks), tasks));
  tasks.push_back(u"task2");
  EXPECT_TRUE(ContainsSubstringsInOrder(
      TaskManagerTableModel::FormatListToString(tasks), tasks));
  tasks.push_back(u"task3");
  EXPECT_TRUE(ContainsSubstringsInOrder(
      TaskManagerTableModel::FormatListToString(tasks), tasks));
}

TEST_F(TaskManagerTableModelTest, RasterizeThemedIconRecolorsIllegibleIcon) {
  const gfx::ImageSkia icon = CreateSolidIcon(SK_ColorBLACK);
  const gfx::ImageSkia themed =
      RasterizeThemedIcon(icon, /*icon_color=*/SK_ColorWHITE,
                          /*icon_background_color=*/SK_ColorBLACK,
                          /*row_background_color=*/kDarkGray);
  EXPECT_FALSE(themed.BackedBySameObjectAs(icon));
  EXPECT_EQ(SK_ColorWHITE, themed.bitmap()->getColor(8, 8));
}

TEST_F(TaskManagerTableModelTest, RasterizeThemedIconKeepsLegibleIcon) {
  const gfx::ImageSkia icon = CreateSolidIcon(SK_ColorBLACK);
  const gfx::ImageSkia themed =
      RasterizeThemedIcon(icon, /*icon_color=*/kDarkGray,
                          /*icon_background_color=*/SK_ColorWHITE,
                          /*row_background_color=*/SK_ColorWHITE);
  EXPECT_TRUE(themed.BackedBySameObjectAs(icon));
}

TEST_F(TaskManagerTableModelTest, RasterizeThemedIconConsidersBothBackgrounds) {
  const gfx::ImageSkia icon = CreateSolidIcon(SK_ColorBLACK);
  // Legible on the icon background, lost on the row.
  gfx::ImageSkia themed =
      RasterizeThemedIcon(icon, /*icon_color=*/SK_ColorGRAY,
                          /*icon_background_color=*/SK_ColorWHITE,
                          /*row_background_color=*/SK_ColorBLACK);
  EXPECT_FALSE(themed.BackedBySameObjectAs(icon));
  EXPECT_EQ(SK_ColorGRAY, themed.bitmap()->getColor(8, 8));
  // Legible on the row, lost on the icon background.
  themed = RasterizeThemedIcon(icon, /*icon_color=*/SK_ColorGRAY,
                               /*icon_background_color=*/SK_ColorBLACK,
                               /*row_background_color=*/SK_ColorWHITE);
  EXPECT_FALSE(themed.BackedBySameObjectAs(icon));
  EXPECT_EQ(SK_ColorGRAY, themed.bitmap()->getColor(8, 8));
}

TEST_F(TaskManagerTableModelTest, RasterizeThemedIconWithoutColorProvider) {
  const gfx::ImageSkia icon = CreateSolidIcon(SK_ColorBLACK);
  const gfx::ImageSkia themed = RasterizeThemedIconWithoutColorProvider(icon);
  EXPECT_TRUE(themed.BackedBySameObjectAs(icon));
}

TEST_F(TaskManagerTableModelTest, TaskIconThemeColorsFromColorProvider) {
  ui::ColorProvider color_provider;
  color_provider.SetColorForTesting(ui::kColorIcon, SK_ColorRED);
  color_provider.SetColorForTesting(ui::kColorTableIconBackground,
                                    SK_ColorGREEN);
  color_provider.SetColorForTesting(kColorTaskManagerTableBackground,
                                    SK_ColorBLUE);
  const TaskIconThemeColors colors =
      TaskIconThemeColors::FromColorProvider(color_provider);
  EXPECT_EQ(SK_ColorRED, colors.icon_color);
  EXPECT_EQ(SK_ColorGREEN, colors.icon_background_color);
  EXPECT_EQ(SK_ColorBLUE, colors.row_background_color);
}

TEST_F(TaskManagerTableModelTest, DefaultCategory) {
  // Prevents a situation where a developer creates a new category, and sets the
  // new kDefaultCategory to the new category, without updating kMax.
  EXPECT_LE(static_cast<int>(TaskManagerTableModel::kDefaultCategory),
            static_cast<int>(DisplayCategory::kMax));
}

}  // namespace task_manager
