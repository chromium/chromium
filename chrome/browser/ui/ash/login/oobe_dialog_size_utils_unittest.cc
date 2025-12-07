// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/oobe_dialog_size_utils.h"

#include <stddef.h>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

constexpr int kShelfHeight = 56;
constexpr int kVirtualKeyboardHeight = 280;
constexpr int kDockedMagnifierHeight = 235;

}  // namespace

class OobeDialogSizeUtilsTest : public testing::Test {
 public:
  OobeDialogSizeUtilsTest() = default;

  OobeDialogSizeUtilsTest(const OobeDialogSizeUtilsTest&) = delete;
  OobeDialogSizeUtilsTest& operator=(const OobeDialogSizeUtilsTest&) = delete;

  ~OobeDialogSizeUtilsTest() override = default;

  void ValidateDialog(const gfx::Rect& display,
                      const gfx::Rect& area,
                      const gfx::Rect& dialog) {
    // Dialog should fully fit into the area.
    EXPECT_TRUE(area.Contains(dialog));

    // Dialog is centered in area.
    EXPECT_EQ(area.CenterPoint(), dialog.CenterPoint());

    const gfx::Size min_dialog_size = GetMinDialogSize(display);
    const gfx::Size max_dialog_size = GetMaxDialogSize(display);
    EXPECT_LE(dialog.width(), max_dialog_size.width());
    EXPECT_LE(dialog.height(), max_dialog_size.height());

    // If there is at least some space, we should have margins.
    const gfx::Size margins = ExpectedMargins(display.size());
    if (dialog.width() > min_dialog_size.width()) {
      EXPECT_EQ(dialog.x() + (area.right() - dialog.right()), margins.width());
    }
    if (dialog.height() > min_dialog_size.height()) {
      EXPECT_EQ(dialog.y() + (area.bottom() - dialog.bottom()),
                margins.height());
    }
    // If dialog size is lesser than minimum size, there should be no margins
    if (dialog.width() < min_dialog_size.width()) {
      EXPECT_EQ(dialog.x(), area.x());
      EXPECT_EQ(dialog.right(), area.right());
    }
    if (dialog.height() < min_dialog_size.height()) {
      EXPECT_EQ(dialog.y(), area.y());
      EXPECT_EQ(dialog.bottom(), area.bottom());
    }
  }

  gfx::Size GetMinDialogSize(const gfx::Rect& display) {
    if (IsHorizontal(display)) {
      return kMinLandscapeDialogSize;
    }
    return kMinPortraitDialogSize;
  }

  gfx::Size GetMaxDialogSize(const gfx::Rect& display) {
    if (IsHorizontal(display)) {
      return kMaxLandscapeDialogSize;
    }
    return kMaxPortraitDialogSize;
  }

  gfx::Size ExpectedMargins(const gfx::Size& display_size) {
    gfx::Size margin = ScaleToCeiledSize(display_size, 0.08);
    gfx::Size margins = margin + margin;
    margins.SetToMax(kMinMargins.size());
    return margins;
  }

  gfx::Rect SizeWithoutShelf(const gfx::Rect& area) const {
    return gfx::Rect(area.width(), area.height() - kShelfHeight);
  }

  gfx::Rect SizeWithoutKeyboard(const gfx::Rect& area) const {
    return gfx::Rect(area.width(), area.height() - kVirtualKeyboardHeight);
  }

  gfx::Rect SizeWithoutDockedMagnifier(const gfx::Rect& area) const {
    return gfx::Rect(area.width(), area.height() - kDockedMagnifierHeight);
  }

  bool IsHorizontal(const gfx::Rect& area) const {
    return area.width() > area.height();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// We have plenty of space on the screen.
TEST_F(OobeDialogSizeUtilsTest, Chromebook) {
  gfx::Rect usual_device(1200, 800);
  gfx::Rect dialog;

  CalculateOobeDialogBounds(usual_device, kShelfHeight,
                            IsHorizontal(usual_device), &dialog);
  ValidateDialog(usual_device, SizeWithoutShelf(usual_device), dialog);
}

// We have plenty of space on the screen, but virtual keyboard takes some space.
TEST_F(OobeDialogSizeUtilsTest, ChromebookVirtualKeyboard) {
  gfx::Rect usual_device(1200, 800);
  gfx::Rect dialog;

  CalculateOobeDialogBounds(SizeWithoutKeyboard(usual_device), 0,
                            IsHorizontal(usual_device), &dialog);
  ValidateDialog(usual_device, SizeWithoutKeyboard(usual_device), dialog);
}

// Tablet device can have smaller screen size.
TEST_F(OobeDialogSizeUtilsTest, TabletHorizontal) {
  gfx::Rect tablet_device(1080, 675);
  gfx::Rect dialog;

  CalculateOobeDialogBounds(tablet_device, kShelfHeight,
                            IsHorizontal(tablet_device), &dialog);
  ValidateDialog(tablet_device, SizeWithoutShelf(tablet_device), dialog);
}

// Tablet device in horizontal mode with virtual keyboard have restricted
// vertical space.
TEST_F(OobeDialogSizeUtilsTest, TabletHorizontalVirtualKeyboard) {
  gfx::Rect tablet_device(1080, 675);
  gfx::Rect dialog;

  CalculateOobeDialogBounds(SizeWithoutKeyboard(tablet_device), 0,
                            IsHorizontal(tablet_device), &dialog);
  ValidateDialog(tablet_device, SizeWithoutKeyboard(tablet_device), dialog);
}

// Tablet device in horizontal mode with docked magnifier have restricted
// vertical space.
TEST_F(OobeDialogSizeUtilsTest, TabletHorizontalDockedMagnifier) {
  gfx::Rect tablet_device(1080, 675);
  gfx::Rect dialog;

  CalculateOobeDialogBounds(SizeWithoutDockedMagnifier(tablet_device), 0,
                            IsHorizontal(tablet_device), &dialog);
  ValidateDialog(tablet_device, SizeWithoutDockedMagnifier(tablet_device),
                 dialog);
}

// Tablet device in horizontal mode with virtual keyboard and docked
// magnifier results in very few vertical space.
TEST_F(OobeDialogSizeUtilsTest, TabletHorizontalVirtualKeyboardMagnifier) {
  gfx::Rect tablet_device(1080, 675);
  gfx::Rect dialog;

  CalculateOobeDialogBounds(
      SizeWithoutDockedMagnifier(SizeWithoutKeyboard(tablet_device)), 0,
      IsHorizontal(tablet_device), &dialog);
  ValidateDialog(tablet_device,
                 SizeWithoutDockedMagnifier(SizeWithoutKeyboard(tablet_device)),
                 dialog);
}

// Tablet in vertical mode puts some strain on dialog width.
TEST_F(OobeDialogSizeUtilsTest, ChromeTabVertical) {
  gfx::Rect tablet_device(461, 738);
  gfx::Rect dialog;

  CalculateOobeDialogBounds(tablet_device, kShelfHeight,
                            IsHorizontal(tablet_device), &dialog);
  ValidateDialog(tablet_device, SizeWithoutShelf(tablet_device), dialog);
}

// Tablet in horizontal mode puts some strain on dialog width.
TEST_F(OobeDialogSizeUtilsTest, ChromeTabHorizontal) {
  gfx::Rect tablet_device(738, 461);
  gfx::Rect dialog;

  CalculateOobeDialogBounds(tablet_device, kShelfHeight,
                            IsHorizontal(tablet_device), &dialog);
  ValidateDialog(tablet_device, SizeWithoutShelf(tablet_device), dialog);
}

}  // namespace ash
