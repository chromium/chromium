// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_LIVE_CAPTION_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_LIVE_CAPTION_BUTTON_H_

#include "chrome/browser/ui/views/overlay/simple_overlay_window_image_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

// Button that opens and closes the live caption dialog.
class OverlayWindowLiveCaptionButton : public SimpleOverlayWindowImageButton {
  METADATA_HEADER(OverlayWindowLiveCaptionButton,
                  SimpleOverlayWindowImageButton)

 public:
  explicit OverlayWindowLiveCaptionButton(PressedCallback callback);
  OverlayWindowLiveCaptionButton(const OverlayWindowLiveCaptionButton&) =
      delete;
  OverlayWindowLiveCaptionButton& operator=(
      const OverlayWindowLiveCaptionButton&) = delete;
  ~OverlayWindowLiveCaptionButton() override;

  void SetIsLiveCaptionDialogOpen(bool is_open);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_LIVE_CAPTION_BUTTON_H_
