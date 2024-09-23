// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELL_DELEGATE_CHROME_SHELL_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_SHELL_DELEGATE_CHROME_SHELL_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/public/cpp/tab_strip_delegate.h"
#include "ash/shell_delegate.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "url/gurl.h"

namespace ash {
class WindowState;
}

class ChromeShellDelegate : public ash::ShellDelegate {
 public:
  ChromeShellDelegate();

  ChromeShellDelegate(const ChromeShellDelegate&) = delete;
  ChromeShellDelegate& operator=(const ChromeShellDelegate&) = delete;

  ~ChromeShellDelegate() override;

  // ash::ShellDelegate:
  bool CanShowWindowForUser(const aura::Window* window) const override;
  std::unique_ptr<ash::CaptureModeDelegate> CreateCaptureModeDelegate()
      const override;
  std::unique_ptr<ash::ClipboardHistoryControllerDelegate>
  CreateClipboardHistoryControllerDelegate() const override;
  std::unique_ptr<ash::CoralDelegate> CreateCoralDelegate() const override;
  std::unique_ptr<ash::GameDashboardDelegate> CreateGameDashboardDelegate()
      const override;
  std::unique_ptr<ash::AcceleratorPrefsDelegate>
  CreateAcceleratorPrefsDelegate() const override;
  ash::AccessibilityDelegate* CreateAccessibilityDelegate() override;
  std::unique_ptr<ash::BackGestureContextualNudgeDelegate>
  CreateBackGestureContextualNudgeDelegate(
      ash::BackGestureContextualNudgeController* controller) override;
  std::unique_ptr<ash::MediaNotificationProvider>
  CreateMediaNotificationProvider() override;
  std::unique_ptr<ash::NearbyShareDelegate> CreateNearbyShareDelegate(
      ash::NearbyShareController* controller) const override;
  std::unique_ptr<ash::SavedDeskDelegate> CreateSavedDeskDelegate()
      const override;
  std::unique_ptr<ash::SystemSoundsDelegate> CreateSystemSoundsDelegate()
      const override;
  std::unique_ptr<ash::TabStripDelegate> CreateTabStripDelegate()
      const override;
  std::unique_ptr<ash::api::TasksDelegate> CreateTasksDelegate() const override;
  std::unique_ptr<ash::FocusModeDelegate> CreateFocusModeDelegate()
      const override;
  std::unique_ptr<ash::UserEducationDelegate> CreateUserEducationDelegate()
      const override;
  std::unique_ptr<ash::ScannerDelegate> CreateScannerDelegate() const override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetBrowserProcessUrlLoaderFactory() const override;
  void OpenKeyboardShortcutHelpPage() const override;
  bool CanGoBack(gfx::NativeWindow window) const override;
  void SetTabScrubberChromeOSEnabled(bool enabled) override;
  bool AllowDefaultTouchActions(gfx::NativeWindow window) override;
  bool ShouldWaitForTouchPressAck(gfx::NativeWindow window) override;
  bool IsTabDrag(const ui::OSExchangeData& drop_data) override;
  int GetBrowserWebUITabStripHeight() override;
  void BindFingerprint(
      mojo::PendingReceiver<device::mojom::Fingerprint> receiver) override;
  void BindMultiDeviceSetup(
      mojo::PendingReceiver<ash::multidevice_setup::mojom::MultiDeviceSetup>
          receiver) override;
  void BindMultiCaptureService(
      mojo::PendingReceiver<video_capture::mojom::MultiCaptureService> receiver)
      override;
  media_session::MediaSessionService* GetMediaSessionService() override;
  bool IsSessionRestoreInProgress() const override;
  void SetUpEnvironmentForLockedFullscreen(
      const ash::WindowState& window_state) override;
  bool IsUiDevToolsStarted() const override;
  void StartUiDevTools() override;
  void StopUiDevTools() override;
  int GetUiDevToolsPort() const override;
  bool IsLoggingRedirectDisabled() const override;
  base::FilePath GetPrimaryUserDownloadsFolder() const override;
  void OpenFeedbackDialog(ShellDelegate::FeedbackSource source,
                          const std::string& description_template,
                          const std::string& category_tag) override;
  void OpenProfileManager() override;
  static void SetDisableLoggingRedirectForTesting(bool value);
  static void ResetDisableLoggingRedirectForTesting();
  const GURL& GetLastCommittedURLForWindowIfAny(aura::Window* window) override;
  version_info::Channel GetChannel() override;
  void ForceSkipWarningUserOnClose(
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows)
      override;
  std::string GetVersionString() override;
  void ShouldExitFullscreenBeforeLock(
      ShouldExitFullscreenCallback callback) override;
  ash::DeskProfilesDelegate* GetDeskProfilesDelegate() override;
  void OpenMultitaskingSettings() override;
  bool IsNoFirstRunSwitchOn() const override;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELL_DELEGATE_CHROME_SHELL_DELEGATE_H_
