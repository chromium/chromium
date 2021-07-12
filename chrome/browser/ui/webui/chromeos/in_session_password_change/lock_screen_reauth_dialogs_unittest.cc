// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_reauth_dialogs.h"

#include "chrome/browser/ash/login/ui/oobe_dialog_size_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {

namespace {

constexpr int kShelfHeight = 56;

}  // namespace

class LockScreenStartReauthDialogTest : public testing::Test {
 public:
  LockScreenStartReauthDialogTest() = default;

  ~LockScreenStartReauthDialogTest() override = default;

  void ValidateDialog(const gfx::Size& area, const gfx::Size& dialog) {
    EXPECT_LE(dialog.width(), area.width());
    EXPECT_LE(dialog.height(), area.height());

    bool is_horizontal = dialog.width() >= dialog.height();
    if (is_horizontal) {
      EXPECT_LE(dialog.width(), kMaxLandscapeDialogSize.width());
      EXPECT_LE(dialog.height(), kMaxLandscapeDialogSize.height());
      EXPECT_GE(dialog.width(), kMinLandscapeDialogSize.width());
      EXPECT_GE(dialog.height(), kMinLandscapeDialogSize.height());
    } else {
      EXPECT_LE(dialog.width(), kMaxPortraitDialogSize.width());
      EXPECT_LE(dialog.height(), kMaxPortraitDialogSize.height());
      EXPECT_GE(dialog.width(), kMinPortraitDialogSize.width());
      EXPECT_GE(dialog.height(), kMinPortraitDialogSize.height());
    }
  }

  gfx::Size SizeWithoutShelf(const gfx::Size& area) const {
    return gfx::Size(area.width(), area.height() - kShelfHeight);
  }

 private:
  LockScreenStartReauthDialogTest(const LockScreenStartReauthDialogTest&) =
      delete;
  LockScreenStartReauthDialogTest& operator=(
      const LockScreenStartReauthDialogTest&) = delete;
};

// We have plenty of space on the screen.
TEST_F(LockScreenStartReauthDialogTest, Chromebook) {
  gfx::Size usual_device(1200, 800);
  gfx::Size dialog =
      LockScreenStartReauthDialog::CalculateLockScreenReauthDialogSize(
          usual_device, /* is_new_layout_enabled = */ true);

  ValidateDialog(SizeWithoutShelf(usual_device), dialog);
}

// Tablet device can have smaller screen size.
TEST_F(LockScreenStartReauthDialogTest, TabletHorizontal) {
  gfx::Size tablet_device(1080, 675);
  gfx::Size dialog =
      LockScreenStartReauthDialog::CalculateLockScreenReauthDialogSize(
          tablet_device, /* is_new_layout_enabled = */ true);

  ValidateDialog(SizeWithoutShelf(tablet_device), dialog);
}

}  // namespace chromeos
