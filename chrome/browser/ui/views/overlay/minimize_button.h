// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_MINIMIZE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_MINIMIZE_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "chrome/browser/ui/views/overlay/video_overlay_window_views.h"
#include "ui/base/metadata/metadata_header_macros.h"

// An image button representing a close button.
class OverlayWindowMinimizeButton : public OverlayWindowImageButton {
  METADATA_HEADER(OverlayWindowMinimizeButton, OverlayWindowImageButton)

 public:
  explicit OverlayWindowMinimizeButton(PressedCallback callback);
  OverlayWindowMinimizeButton(const OverlayWindowMinimizeButton&) = delete;
  OverlayWindowMinimizeButton& operator=(const OverlayWindowMinimizeButton&) =
      delete;
  ~OverlayWindowMinimizeButton() override = default;

  // Sets the position of itself with an offset from the given window size.
  void SetPosition(const gfx::Size& size,
                   VideoOverlayWindowViews::WindowQuadrant quadrant);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_MINIMIZE_BUTTON_H_
