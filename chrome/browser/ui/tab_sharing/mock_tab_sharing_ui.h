// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_SHARING_MOCK_TAB_SHARING_UI_H_
#define CHROME_BROWSER_UI_TAB_SHARING_MOCK_TAB_SHARING_UI_H_

#include "chrome/browser/ui/tab_sharing/tab_sharing_ui.h"
#include "chrome/browser/ui/views/screen_sharing_util.h"
#include "content/public/browser/desktop_media_id.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockTabSharingUI : public TabSharingUI {
 public:
  MockTabSharingUI();
  ~MockTabSharingUI() override;

  MOCK_METHOD(void, StartSharing, (infobars::InfoBar * infobar));
  MOCK_METHOD(void, StopSharing, (std::string_view));

  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::vector<content::DesktopMediaID>& media_ids) override;

  ScreensharingControlsHistogramLogger& GetUmaLogger() override;

  void OnRegionCaptureRectChanged(
      const std::optional<gfx::Rect>& region_capture_rect) override;

 private:
  ScreensharingControlsHistogramLogger uma_logger_;
};

#endif  // CHROME_BROWSER_UI_TAB_SHARING_MOCK_TAB_SHARING_UI_H_
