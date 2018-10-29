// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_LINUX_BROWSER_FRAME_VIEW_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_LINUX_BROWSER_FRAME_VIEW_LAYOUT_H_

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"

// A specialization of OpaqueBrowserFrameViewLayout that is also able
// to layout frame buttons that were rendered by GTK.
class DesktopLinuxBrowserFrameViewLayout : public OpaqueBrowserFrameViewLayout {
 public:
  DesktopLinuxBrowserFrameViewLayout(
      views::NavButtonProvider* nav_button_provider);

  // OpaqueBrowserFrameViewLayout:
  int CaptionButtonY(chrome::FrameButtonDisplayType button_id,
                     bool restored) const override;
  int TopAreaPadding() const override;
  int GetWindowCaptionSpacing(views::FrameButton button_id,
                              bool leading_spacing,
                              bool is_leading_button) const override;
  bool ShouldDrawImageMirrored(views::ImageButton* button,
                               ButtonAlignment alignment) const override;

 private:
  int TitlebarTopThickness() const;

  views::NavButtonProvider* nav_button_provider_;

  DISALLOW_COPY_AND_ASSIGN(DesktopLinuxBrowserFrameViewLayout);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_LINUX_BROWSER_FRAME_VIEW_LAYOUT_H_
