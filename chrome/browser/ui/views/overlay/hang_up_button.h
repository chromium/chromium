// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_HANG_UP_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_HANG_UP_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class HangUpButton : public OverlayWindowImageButton {
  METADATA_HEADER(HangUpButton, OverlayWindowImageButton)

 public:
  explicit HangUpButton(PressedCallback callback);
  HangUpButton(const HangUpButton&) = delete;
  HangUpButton& operator=(const HangUpButton&) = delete;
  ~HangUpButton() override = default;

 protected:
  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  void UpdateImage();
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_HANG_UP_BUTTON_H_
