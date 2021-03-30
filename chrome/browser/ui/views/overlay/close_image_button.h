// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_CLOSE_IMAGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_CLOSE_IMAGE_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace views {

// An image button representing a close button.
class CloseImageButton : public views::ImageButton {
 public:
  METADATA_HEADER(CloseImageButton);

  explicit CloseImageButton(PressedCallback callback);
  CloseImageButton(const CloseImageButton&) = delete;
  CloseImageButton& operator=(const CloseImageButton&) = delete;
  ~CloseImageButton() override = default;

  // Sets the position of itself with an offset from the given window size.
  void SetPosition(const gfx::Size& size,
                   OverlayWindowViews::WindowQuadrant quadrant);
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_CLOSE_IMAGE_BUTTON_H_
