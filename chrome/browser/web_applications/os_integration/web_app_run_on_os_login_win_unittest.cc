// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_run_on_os_login.h"

#include <vector>

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/win/shortcut.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_win.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/shell_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

namespace web_app {

namespace {

constexpr char16_t kAppTitle[] = u"app";
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
    shortcut_info->app_id = "app-id";
    shortcut_info->title = kAppTitle;
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
    EXPECT_TRUE(ShellUtil::GetShortcutPath(
        ShellUtil::ShortcutLocation::SHORTCUT_LOCATION_STARTUP,
        ShellUtil::ShellChange::CURRENT_USER, &location));
    return location;
  }

  std::vector<base::FilePath> GetShortcuts() {
    return internals::FindAppShortcutsByProfileAndTitle(
        GetStartupFolder(), profile()->GetPath(), kAppTitle);
  }

  void VerifyShortcutCreated() {
    std::vector<base::FilePath> shortcuts = GetShortcuts();
    EXPECT_EQ(shortcuts.size(), 1u);

    for (const base::FilePath& shortcut : shortcuts) {
      std::wstring cmd_line_string;
      EXPECT_TRUE(
          base::win::ResolveShortcut(shortcut, nullptr, &cmd_line_string));
      base::CommandLine shortcut_cmd_line =
          base::CommandLine::FromString(L"program " + cmd_line_string);
      EXPECT_TRUE(shortcut_cmd_line.HasSwitch(switches::kAppRunOnOsLoginMode));
      EXPECT_EQ(
          shortcut_cmd_line.GetSwitchValueASCII(switches::kAppRunOnOsLoginMode),
          kRunOnOsLoginModeWindowed);
    }
  }

  void VerifyShortcutDeleted() {
    std::vector<base::FilePath> shortcuts = GetShortcuts();
    EXPECT_EQ(shortcuts.size(), 0u);
  }
};

TEST_F(WebAppRunOnOsLoginWinTest, Register) {
  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();
  base::test::TestFuture<Result> result;
  internals::RegisterRunOnOsLogin(*shortcut_info, result.GetCallback());
  EXPECT_EQ(result.Get(), Result::kOk);
  VerifyShortcutCreated();
}

TEST_F(WebAppRunOnOsLoginWinTest, RegisterMultipleTimes) {
  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();
  base::test::TestFuture<Result> result;
  internals::RegisterRunOnOsLogin(*shortcut_info, result.GetCallback());
  EXPECT_EQ(result.Get(), Result::kOk);
  VerifyShortcutCreated();

  result.Clear();
  // There should still only be one shortcut created.
  internals::RegisterRunOnOsLogin(*shortcut_info, result.GetCallback());
  VerifyShortcutCreated();
}

TEST_F(WebAppRunOnOsLoginWinTest, Unregister) {
  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();
  base::test::TestFuture<Result> result;
  internals::RegisterRunOnOsLogin(*shortcut_info, result.GetCallback());
  EXPECT_EQ(result.Get(), Result::kOk);
  VerifyShortcutCreated();

  internals::UnregisterRunOnOsLogin(shortcut_info->app_id, profile()->GetPath(),
                                    kAppTitle);
  VerifyShortcutDeleted();
}

}  // namespace web_app
