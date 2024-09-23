// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_run_on_os_login.h"

#include <vector>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_linux.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/auto_start_linux.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

namespace web_app {

namespace {

constexpr char16_t kAppTitle[] = u"app";
constexpr char kAppId[] = "app-id";

}  // namespace

class WebAppRunOnOsLoginLinuxTest : public WebAppTest {
 public:
  void TearDown() override {
    WebAppTest::TearDown();
  }

  std::unique_ptr<ShortcutInfo> GetShortcutInfo() {
    auto shortcut_info = std::make_unique<ShortcutInfo>();
    shortcut_info->app_id = kAppId;
    shortcut_info->title = kAppTitle;
    shortcut_info->profile_path = profile()->GetPath();

    gfx::ImageFamily image_family;
    SquareSizePx icon_size_in_px = GetDesiredIconSizesForShortcut().back();
    gfx::ImageSkia image_skia = CreateDefaultApplicationIcon(icon_size_in_px);
    image_family.Add(gfx::Image(image_skia));
    shortcut_info->favicon = std::move(image_family);

    return shortcut_info;
  }

  base::FilePath GetPathToAutoStartFile() {
    base::FilePath autostart_path =
        OsIntegrationTestOverrideImpl::Get()->startup();

    base::FilePath shortcut_filename =
        GetAppDesktopShortcutFilename(profile()->GetPath(), kAppId);
    EXPECT_FALSE(shortcut_filename.empty());

    return autostart_path.Append(shortcut_filename);
  }

  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      os_integration_override_{
          OsIntegrationTestOverrideImpl::OverrideForTesting()};
};

TEST_F(WebAppRunOnOsLoginLinuxTest, Register) {
  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();
  base::test::TestFuture<Result> result;
  internals::RegisterRunOnOsLogin(*shortcut_info, result.GetCallback());
  EXPECT_EQ(result.Get(), Result::kOk);
  EXPECT_TRUE(base::PathExists(GetPathToAutoStartFile()));
}

TEST_F(WebAppRunOnOsLoginLinuxTest, Unregister) {
  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();
  base::test::TestFuture<Result> result;
  internals::RegisterRunOnOsLogin(*shortcut_info, result.GetCallback());
  EXPECT_EQ(result.Get(), Result::kOk);
  EXPECT_TRUE(base::PathExists(GetPathToAutoStartFile()));

  EXPECT_EQ(Result::kOk,
            internals::UnregisterRunOnOsLogin(shortcut_info->app_id,
                                              profile()->GetPath(), kAppTitle));
  EXPECT_FALSE(base::PathExists(GetPathToAutoStartFile()));
}

}  // namespace web_app
