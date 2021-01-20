// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_

#include <memory>

#include "ash/shell_delegate.h"
#include "base/macros.h"

class ChromeShellDelegate : public ash::ShellDelegate {
 public:
  ChromeShellDelegate();
  ~ChromeShellDelegate() override;

  // ash::ShellDelegate:
  bool CanShowWindowForUser(const aura::Window* window) const override;
  std::unique_ptr<ash::CaptureModeDelegate> CreateCaptureModeDelegate()
      const override;
  std::unique_ptr<ash::ScreenshotDelegate> CreateScreenshotDelegate() override;
  ash::AccessibilityDelegate* CreateAccessibilityDelegate() override;
  std::unique_ptr<ash::BackGestureContextualNudgeDelegate>
  CreateBackGestureContextualNudgeDelegate(
      ash::BackGestureContextualNudgeController* controller) override;
  void OpenKeyboardShortcutHelpPage() const override;
  void DesksStateChanged(int num_desks) const override;
  bool CanGoBack(gfx::NativeWindow window) const override;
  void SetTabScrubberEnabled(bool enabled) override;
  bool AllowDefaultTouchActions(gfx::NativeWindow window) override;
  bool ShouldWaitForTouchPressAck(gfx::NativeWindow window) override;
  bool IsTabDrag(const ui::OSExchangeData& drop_data) override;
  aura::Window* CreateBrowserForTabDrop(
      aura::Window* source_window,
      const ui::OSExchangeData& drop_data) override;
  void BindBluetoothSystemFactory(
      mojo::PendingReceiver<device::mojom::BluetoothSystemFactory> receiver)
      override;
  void BindFingerprint(
      mojo::PendingReceiver<device::mojom::Fingerprint> receiver) override;
  void BindMultiDeviceSetup(
      mojo::PendingReceiver<
          chromeos::multidevice_setup::mojom::MultiDeviceSetup> receiver)
      override;
  media_session::MediaSessionService* GetMediaSessionService() override;
  std::unique_ptr<ash::NearbyShareDelegate> CreateNearbyShareDelegate(
      ash::NearbyShareController* controller) const override;
  bool IsSessionRestoreInProgress() const override;
  bool IsUiDevToolsStarted() const override;
  void StartUiDevTools() override;
  void StopUiDevTools() override;
  int GetUiDevToolsPort() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeShellDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_
