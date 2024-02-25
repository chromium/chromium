// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_IMAGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_IMAGE_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"

// Base class for image buttons on the PiP window.
class OverlayWindowImageButton : public views::ImageButton {
  METADATA_HEADER(OverlayWindowImageButton, views::ImageButton)

 public:
  OverlayWindowImageButton(const OverlayWindowImageButton&) = delete;
  OverlayWindowImageButton& operator=(const OverlayWindowImageButton&) = delete;

 protected:
  explicit OverlayWindowImageButton(PressedCallback callback);
  ~OverlayWindowImageButton() override = default;

  // views::View:
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;
  void OnThemeChanged() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_IMAGE_BUTTON_H_
