// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_run_on_os_login.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_win.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/installer/util/shell_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

namespace web_app {

namespace {

constexpr char kAppTitle[] = {"app"};
}  // namespace

class WebAppRunOnOsLoginWinTest : public WebAppTest {
 public:
  void TearDown() override {
    base::FilePath location = GetStartupFolder();
    std::vector<base::FilePath> shortcuts = GetShortcuts();
    for (const auto& shortcut_file : shortcuts) {
      base::DeleteFile(shortcut_file);
    }
    WebAppTest::TearDown();
  }

  std::unique_ptr<ShortcutInfo> GetShortcutInfo() {
    auto shortcut_info = std::make_unique<ShortcutInfo>();
    shortcut_info->extension_id = "app-id";
    shortcut_info->title = base::UTF8ToUTF16(kAppTitle);
    shortcut_info->profile_path = profile()->GetPath();

    gfx::ImageFamily image_family;
    SquareSizePx icon_size_in_px = GetDesiredIconSizesForShortcut().back();
    gfx::ImageSkia image_skia = CreateDefaultApplicationIcon(icon_size_in_px);
    image_family.Add(gfx::Image(image_skia));
    shortcut_info->favicon = std::move(image_family);

    return shortcut_info;
  }

  base::FilePath GetStartupFolder() {
    base::FilePath location;
    ShellUtil::GetShortcutPath(
        ShellUtil::ShortcutLocation::SHORTCUT_LOCATION_STARTUP,
        ShellUtil::ShellChange::CURRENT_USER, &location);
    return location;
  }

  std::vector<base::FilePath> GetShortcuts() {
    return internals::FindAppShortcutsByProfileAndTitle(
        GetStartupFolder(), profile()->GetPath(), base::UTF8ToUTF16(kAppTitle));
  }

  void VerifyShortcutCreated() {
    std::vector<base::FilePath> shortcuts = GetShortcuts();
    EXPECT_GT(shortcuts.size(), 0u);
  }

  void VerifyShortcutDeleted() {
    std::vector<base::FilePath> shortcuts = GetShortcuts();
    EXPECT_EQ(shortcuts.size(), 0u);
  }
};

TEST_F(WebAppRunOnOsLoginWinTest, Register) {
  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();
  bool result = internals::RegisterRunOnOsLogin(*shortcut_info);
  EXPECT_TRUE(result);
  VerifyShortcutCreated();
}

TEST_F(WebAppRunOnOsLoginWinTest, Unregister) {
  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();
  bool result = internals::RegisterRunOnOsLogin(*shortcut_info);
  EXPECT_TRUE(result);
  VerifyShortcutCreated();

  internals::UnregisterRunOnOsLogin(profile()->GetPath(),
                                    base::UTF8ToUTF16(kAppTitle));
  VerifyShortcutDeleted();
}

}  // namespace web_app
