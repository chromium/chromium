// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_RESIZE_HANDLE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_RESIZE_HANDLE_BUTTON_H_

#include "chrome/browser/ui/views/overlay/video_overlay_window_views.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"

// An image button representing a white resize handle affordance.
class ResizeHandleButton : public views::ImageButton {
  METADATA_HEADER(ResizeHandleButton, views::ImageButton)

 public:
  explicit ResizeHandleButton(PressedCallback callback);
  ResizeHandleButton(const ResizeHandleButton&) = delete;
  ResizeHandleButton& operator=(const ResizeHandleButton&) = delete;
  ~ResizeHandleButton() override;

  void OnThemeChanged() override;

  void SetPosition(const gfx::Size& size,
                   VideoOverlayWindowViews::WindowQuadrant quadrant);
  int GetHTComponent() const;
  void SetQuadrant(VideoOverlayWindowViews::WindowQuadrant quadrant);

 private:
  void UpdateImageForQuadrant();

  VideoOverlayWindowViews::WindowQuadrant current_quadrant_ =
      VideoOverlayWindowViews::WindowQuadrant::kBottomRight;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_RESIZE_HANDLE_BUTTON_H_
