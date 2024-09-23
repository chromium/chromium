// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "chrome/browser/ui/views/overlay/video_overlay_window_views.h"
#include "ui/base/metadata/metadata_header_macros.h"

// An image button representing a back-to-tab button.
class OverlayWindowBackToTabButton : public OverlayWindowImageButton {
  METADATA_HEADER(OverlayWindowBackToTabButton, OverlayWindowImageButton)

 public:
  explicit OverlayWindowBackToTabButton(PressedCallback callback);
  OverlayWindowBackToTabButton(const OverlayWindowBackToTabButton&) = delete;
  OverlayWindowBackToTabButton& operator=(const OverlayWindowBackToTabButton&) =
      delete;
  ~OverlayWindowBackToTabButton() override = default;

  // Sets the position of itself with an offset from the given window size.
  void SetPosition(const gfx::Size& size,
                   VideoOverlayWindowViews::WindowQuadrant quadrant);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_BUTTON_H_
