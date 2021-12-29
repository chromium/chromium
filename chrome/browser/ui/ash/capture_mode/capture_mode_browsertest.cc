// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/callback_forward.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/window_util.h"

namespace {

// Defines a warning-level restriction type for screen captures.
const policy::DlpContentRestrictionSet kScreenCaptureWarned{
    policy::DlpContentRestriction::kScreenshot,
    policy::DlpRulesManager::Level::kWarn};

// Returns the native window of the given `browser`.
aura::Window* GetBrowserWindow(Browser* browser) {
  return browser->window()->GetNativeWindow();
}

void SetupLoopToWaitForCaptureFileToBeSaved(base::RunLoop* loop) {
  ash::CaptureModeTestApi().SetOnCaptureFileSavedCallback(
      base::BindLambdaForTesting([loop](const base::FilePath& path) {
        base::ScopedAllowBlockingForTesting allow_blocking;
        EXPECT_TRUE(base::PathExists(path));
        loop->Quit();
      }));
}

// Defines a waiter that waits for the DLP warning dialog to be added as a child
// of the system modal container window under the given `root`.
class DlpWarningDialogWaiter : public aura::WindowObserver {
 public:
  explicit DlpWarningDialogWaiter(aura::Window* root) {
    observation_.Observe(
        root->GetChildById(ash::kShellWindowId_SystemModalContainer));
  }
  DlpWarningDialogWaiter(const DlpWarningDialogWaiter&) = delete;
  DlpWarningDialogWaiter& operator=(const DlpWarningDialogWaiter&) = delete;
  ~DlpWarningDialogWaiter() override = default;

  void Wait() {
    base::RunLoop loop;
    on_window_added_callback_ = loop.QuitClosure();
    loop.Run();
  }

  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* new_window) override {
    if (on_window_added_callback_)
      std::move(on_window_added_callback_).Run();
  }

 private:
  base::ScopedObservation<aura::Window, aura::WindowObserver> observation_{
      this};
  base::OnceClosure on_window_added_callback_;
};

// Starts a fullscreen video recording.
void StartVideoRecording() {
  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  ASSERT_TRUE(test_api.IsSessionActive());
  test_api.PerformCapture();
  test_api.FlushRecordingServiceForTesting();
  EXPECT_TRUE(test_api.IsVideoRecordingInProgress());
}

// Marks the active web contents of the given `browser` as DLP restricted with a
// warning level.
void MarkActiveTabAsDlpWarnedForScreenCapture(Browser* browser) {
  auto* dlp_content_observer = policy::DlpContentObserver::Get();
  ASSERT_TRUE(dlp_content_observer);

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  dlp_content_observer->OnConfidentialityChanged(web_contents,
                                                 kScreenCaptureWarned);
}

// Stops the video recording and waits for the DLP warning dialog to be added.
void StopRecordingAndWaitForDlpWarningDialog(Browser* browser) {
  auto* root = GetBrowserWindow(browser)->GetRootWindow();
  ASSERT_TRUE(root);
  DlpWarningDialogWaiter waiter{root};
  ash::CaptureModeTestApi test_api;
  test_api.StopVideoRecording();
  waiter.Wait();
  EXPECT_FALSE(test_api.IsVideoRecordingInProgress());
}

void SendKeyEvent(Browser* browser,
                  ui::KeyboardCode key_code,
                  int flags = ui::EF_NONE) {
  auto* browser_window = GetBrowserWindow(browser);
  ui::test::EventGenerator event_generator{browser_window->GetRootWindow(),
                                           browser_window};
  event_generator.PressAndReleaseKey(key_code, flags);
}

}  // namespace

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

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpWarningDialogOnVideoEndDismissed) {
  ASSERT_TRUE(browser());
  StartVideoRecording();

  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  ash::CaptureModeTestApi test_api;
  // Video recording should not end as a result of adding a restriction of a
  // warning level type.
  EXPECT_TRUE(test_api.IsVideoRecordingInProgress());

  // Set up a waiter to wait for the file to be deleted.
  base::RunLoop loop;
  test_api.SetOnCaptureFileDeletedCallback(base::BindLambdaForTesting(
      [&loop](const base::FilePath& path, bool delete_successful) {
        EXPECT_TRUE(delete_successful);
        base::ScopedAllowBlockingForTesting allow_blocking;
        EXPECT_FALSE(base::PathExists(path));
        loop.Quit();
      }));
  StopRecordingAndWaitForDlpWarningDialog(browser());

  // Dismiss the dialog by hitting the ESCAPE key and wait for the file to be
  // deleted.
  SendKeyEvent(browser(), ui::VKEY_ESCAPE);
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpWarningDialogOnVideoEndAccepted) {
  ASSERT_TRUE(browser());
  StartVideoRecording();

  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  ash::CaptureModeTestApi test_api;
  // Video recording should not end as a result of adding a restriction of a
  // warning level type.
  EXPECT_TRUE(test_api.IsVideoRecordingInProgress());

  // Set up a waiter to wait for the file to be saved.
  base::RunLoop loop;
  SetupLoopToWaitForCaptureFileToBeSaved(&loop);
  StopRecordingAndWaitForDlpWarningDialog(browser());

  // Accept the dialog by hitting the ENTER key and wait for the file to be
  // saved.
  SendKeyEvent(browser(), ui::VKEY_RETURN);
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpWarningDialogOnSessionInitDismissed) {
  ASSERT_TRUE(browser());

  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  ash::CaptureModeTestApi test_api;
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  test_api.StartForFullscreen(/*for_video=*/false);
  // A capture mode session doesn't start immediately. The controller should be
  // in a pending state waiting for a reply from the DLP manager.
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_TRUE(test_api.IsPendingDlpCheck());

  // Dismiss the dialog by hitting the ESCAPE key. The session should be aborted
  // and the pending state should end.
  SendKeyEvent(browser(), ui::VKEY_ESCAPE);
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsPendingDlpCheck());
}

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpWarningDialogOnSessionInitAccepted) {
  ASSERT_TRUE(browser());

  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  ash::CaptureModeTestApi test_api;
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  test_api.StartForFullscreen(/*for_video=*/true);
  // A capture mode session doesn't start immediately. The controller should be
  // in a pending state waiting for a reply from the DLP manager.
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_TRUE(test_api.IsPendingDlpCheck());

  // Accept the dialog by hitting the ENTER key. The session should start and
  // the pending state should end.
  SendKeyEvent(browser(), ui::VKEY_RETURN);
  EXPECT_TRUE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsPendingDlpCheck());
}

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpWarningDialogOnPerformingCaptureDismissed) {
  ASSERT_TRUE(browser());

  // Start the session before a window becomes restricted.
  ash::CaptureModeTestApi test_api;
  EXPECT_FALSE(test_api.IsPendingDlpCheck());
  test_api.StartForFullscreen(/*for_video=*/false);
  ASSERT_TRUE(test_api.IsSessionActive());

  MarkActiveTabAsDlpWarnedForScreenCapture(browser());

  // Attempt performing the capture now, it won't be performed immediately,
  // rather the dialog will show instead.
  test_api.PerformCapture();

  // The session should remain active but in a pending state.
  EXPECT_TRUE(test_api.IsSessionActive());
  EXPECT_TRUE(test_api.IsPendingDlpCheck());
  EXPECT_TRUE(test_api.IsSessionWaitingForDlpConfirmation());

  // Dismiss the dialog by hitting the ESCAPE key. The session should be aborted
  // and the pending state should end.
  SendKeyEvent(browser(), ui::VKEY_ESCAPE);
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsPendingDlpCheck());
}

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpWarningDialogOnPerformingCaptureAccepted) {
  ASSERT_TRUE(browser());

  // Start the session before a window becomes restricted.
  ash::CaptureModeTestApi test_api;
  EXPECT_FALSE(test_api.IsPendingDlpCheck());
  test_api.StartForFullscreen(/*for_video=*/false);
  ASSERT_TRUE(test_api.IsSessionActive());

  MarkActiveTabAsDlpWarnedForScreenCapture(browser());

  // Attempt performing the capture now, it won't be performed immediately,
  // rather the dialog will show instead.
  test_api.PerformCapture();

  // The session should remain active but in a pending state.
  EXPECT_TRUE(test_api.IsSessionActive());
  EXPECT_TRUE(test_api.IsPendingDlpCheck());
  EXPECT_TRUE(test_api.IsSessionWaitingForDlpConfirmation());

  // Accept the dialog by hitting the ENTER key. The session should end, and the
  // screenshot should be taken.
  base::RunLoop loop;
  SetupLoopToWaitForCaptureFileToBeSaved(&loop);
  SendKeyEvent(browser(), ui::VKEY_RETURN);
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsPendingDlpCheck());
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpWarningDialogOnCountdownEndDismissed) {
  ASSERT_TRUE(browser());
  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  ASSERT_TRUE(test_api.IsSessionActive());
  test_api.PerformCapture(/*skip_count_down=*/false);
  EXPECT_TRUE(test_api.IsInCountDownAnimation());

  // While countdown is in progress, mark the window as restricted, and wait for
  // the dialog to show when the countdown ends.
  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  auto* root = GetBrowserWindow(browser())->GetRootWindow();
  DlpWarningDialogWaiter(root).Wait();
  EXPECT_FALSE(test_api.IsInCountDownAnimation());
  EXPECT_FALSE(test_api.IsVideoRecordingInProgress());
  EXPECT_TRUE(test_api.IsSessionActive());
  EXPECT_TRUE(test_api.IsPendingDlpCheck());
  EXPECT_TRUE(test_api.IsSessionWaitingForDlpConfirmation());

  // Dismiss the dialog by hitting the ESCAPE key and expect that recording
  // doesn't start.
  SendKeyEvent(browser(), ui::VKEY_ESCAPE);
  EXPECT_FALSE(test_api.IsInCountDownAnimation());
  EXPECT_FALSE(test_api.IsVideoRecordingInProgress());
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsPendingDlpCheck());
}

IN_PROC_BROWSER_TEST_F(CaptureModeBrowserTest,
                       DlpWarningDialogOnCountdownEndAccepted) {
  ASSERT_TRUE(browser());
  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  ASSERT_TRUE(test_api.IsSessionActive());
  test_api.PerformCapture(/*skip_count_down=*/false);
  EXPECT_TRUE(test_api.IsInCountDownAnimation());

  // While countdown is in progress, mark the window as restricted, and wait for
  // the dialog to show when the countdown ends.
  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  auto* root = GetBrowserWindow(browser())->GetRootWindow();
  DlpWarningDialogWaiter(root).Wait();
  EXPECT_FALSE(test_api.IsInCountDownAnimation());
  EXPECT_FALSE(test_api.IsVideoRecordingInProgress());
  EXPECT_TRUE(test_api.IsSessionActive());
  EXPECT_TRUE(test_api.IsPendingDlpCheck());
  EXPECT_TRUE(test_api.IsSessionWaitingForDlpConfirmation());

  // Accept the dialog by hitting the ENTER key, and expect recording to start.
  SendKeyEvent(browser(), ui::VKEY_RETURN);
  test_api.FlushRecordingServiceForTesting();
  EXPECT_FALSE(test_api.IsInCountDownAnimation());
  EXPECT_TRUE(test_api.IsVideoRecordingInProgress());
  EXPECT_FALSE(test_api.IsSessionActive());
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  // Stop recording and wait for file to be saved successfully.
  base::RunLoop loop;
  SetupLoopToWaitForCaptureFileToBeSaved(&loop);
  test_api.StopVideoRecording();
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    CaptureModeBrowserTest,
    DlpWarningDialogOnCaptureScreenshotsOfAllDisplaysDismissed) {
  ASSERT_TRUE(browser());

  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  ash::CaptureModeTestApi test_api;
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  // The screenshots should not be taken immediately through the keyboard
  // shortcut. The controller should be in a pending state waiting for a reply
  // from the DLP manager.
  SendKeyEvent(browser(), ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(test_api.IsPendingDlpCheck());

  // Dismiss the dialog by hitting the ESCAPE key. The screenshot should be
  // aborted and the pending state should end.
  SendKeyEvent(browser(), ui::VKEY_ESCAPE);
  EXPECT_FALSE(test_api.IsPendingDlpCheck());
}

IN_PROC_BROWSER_TEST_F(
    CaptureModeBrowserTest,
    DlpWarningDialogOnFullscreenScreenCaptureShortcutAccepted) {
  ASSERT_TRUE(browser());

  MarkActiveTabAsDlpWarnedForScreenCapture(browser());
  ash::CaptureModeTestApi test_api;
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  // The screenshots should not be taken immediately through the keyboard
  // shortcut. The controller should be in a pending state waiting for a reply
  // from the DLP manager.
  SendKeyEvent(browser(), ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(test_api.IsPendingDlpCheck());

  // Set up a waiter to wait for the file to be saved.
  base::RunLoop loop;
  test_api.SetOnCaptureFileSavedCallback(
      base::BindLambdaForTesting([&loop](const base::FilePath& path) {
        base::ScopedAllowBlockingForTesting allow_blocking;
        EXPECT_TRUE(base::PathExists(path));
        loop.Quit();
      }));

  // Accept the dialog by hitting the ENTER key. The screenshot should be taken
  // and the pending state should end.
  SendKeyEvent(browser(), ui::VKEY_RETURN);
  EXPECT_FALSE(test_api.IsPendingDlpCheck());

  // Wait for the file to be saved.
  loop.Run();
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
