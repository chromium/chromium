// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"

// Stub implementation of the ScreenCaptureNotificationUI interface.
class ScreenCaptureNotificationUIStub : public ScreenCaptureNotificationUI {
 public:
  ScreenCaptureNotificationUIStub() = default;
  ~ScreenCaptureNotificationUIStub() override = default;

  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::vector<content::DesktopMediaID>& media_ids) override {
    NOTIMPLEMENTED();
    return 0;
  }
};

// static
std::unique_ptr<ScreenCaptureNotificationUI>
ScreenCaptureNotificationUI::Create(
    const std::u16string& title,
    content::WebContents* capturing_web_contents) {
  return std::make_unique<ScreenCaptureNotificationUIStub>();
}
