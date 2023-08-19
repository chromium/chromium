// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LACROS_SCREEN_CAPTURE_NOTIFICATION_UI_LACROS_H_
#define CHROME_BROWSER_UI_LACROS_SCREEN_CAPTURE_NOTIFICATION_UI_LACROS_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/notifications/notification_platform_bridge_chromeos.h"
#include "chrome/browser/notifications/notification_platform_bridge_lacros.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"

constexpr char kLacrosScreenAccessNotificationId[] = "lacros-screen-access";

// Lacros implementation for ScreenCaptureNotificationUI. This UI displays a
// notification in system tray rather than a notification bar on screen.
class ScreenCaptureNotificationUILacros : public ScreenCaptureNotificationUI {
 public:
  explicit ScreenCaptureNotificationUILacros(const std::u16string& text);

  ScreenCaptureNotificationUILacros(const ScreenCaptureNotificationUILacros&) =
      delete;
  ScreenCaptureNotificationUILacros& operator=(
      const ScreenCaptureNotificationUILacros&) = delete;

  ~ScreenCaptureNotificationUILacros() override;

  // ScreenCaptureNotificationUI override.
  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::vector<content::DesktopMediaID>& media_ids) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ScreenCaptureNotificationUILacrosTest, OnStarted);

  void ProcessStopRequestFromNotification();

  // Body text of the notification.
  const std::u16string text_;

  std::unique_ptr<NotificationPlatformBridgeChromeOs> bridge_delegate_;
  base::OnceClosure stop_callback_;
  base::WeakPtrFactory<ScreenCaptureNotificationUILacros> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_LACROS_SCREEN_CAPTURE_NOTIFICATION_UI_LACROS_H_
