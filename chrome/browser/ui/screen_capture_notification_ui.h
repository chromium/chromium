// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SCREEN_CAPTURE_NOTIFICATION_UI_H_
#define CHROME_BROWSER_UI_SCREEN_CAPTURE_NOTIFICATION_UI_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"

// Interface for screen capture notification UI shown when content of the screen
// is being captured.
class ScreenCaptureNotificationUI : public MediaStreamUI {
 public:
  ScreenCaptureNotificationUI() = default;

  ScreenCaptureNotificationUI(const ScreenCaptureNotificationUI&) = delete;
  ScreenCaptureNotificationUI& operator=(const ScreenCaptureNotificationUI&) =
      delete;

  ~ScreenCaptureNotificationUI() override = default;

  // Creates platform-specific screen capture notification UI. |text| specifies
  // the text that should be shown in the notification.
  static std::unique_ptr<ScreenCaptureNotificationUI> Create(
      const std::u16string& text,
      content::WebContents* capturing_web_contents);
};

#endif  // CHROME_BROWSER_UI_SCREEN_CAPTURE_NOTIFICATION_UI_H_
