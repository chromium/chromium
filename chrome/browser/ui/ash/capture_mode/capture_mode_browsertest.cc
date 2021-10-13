// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/window_util.h"

// Testing class to test CrOS capture mode, which is a feature to take
// screenshots and record video.
using CaptureModeBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest, ContextMenuStaysOpen) {
  // Right click the desktop to open a context menu.
  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  const gfx::Point point_on_desktop(1, 1);
  ASSERT_FALSE(browser_window->bounds().Contains(point_on_desktop));

  ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                           point_on_desktop);
  event_generator.ClickRightButton();

  ash::ShellTestApi shell_test_api;
  ASSERT_TRUE(shell_test_api.IsContextMenuShown());

  ash::CaptureModeTestApi().StartForWindow(/*for_video=*/false);
  EXPECT_TRUE(shell_test_api.IsContextMenuShown());
}

class AdvancedSettingsCaptureModeBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  AdvancedSettingsCaptureModeBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kImprovedScreenCaptureSettings);
  }

  ~AdvancedSettingsCaptureModeBrowserTest() override = default;

  // extensions::ExtensionBrowserTest:
  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    CHECK(profile());
    file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the capture mode folder selection dialog window gets parented
// correctly when a browser window is available.
IN_PROC_BROWSER_TEST_F(AdvancedSettingsCaptureModeBrowserTest,
                       FolderSelectionDialogParentedCorrectly) {
  ASSERT_TRUE(browser());
  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/false);
  test_api.SimulateOpeningFolderSelectionDialog();
  auto* dialog_window = test_api.GetFolderSelectionDialogWindow();
  ASSERT_TRUE(dialog_window);
  auto* transient_root = wm::GetTransientRoot(dialog_window);
  ASSERT_TRUE(transient_root);
  EXPECT_EQ(transient_root->GetId(),
            ash::kShellWindowId_CaptureModeFolderSelectionDialogOwner);
  EXPECT_NE(transient_root, browser()->window()->GetNativeWindow());
}
