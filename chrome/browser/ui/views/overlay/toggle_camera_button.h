// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_TOGGLE_CAMERA_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_TOGGLE_CAMERA_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class ToggleCameraButton : public OverlayWindowImageButton {
  METADATA_HEADER(ToggleCameraButton, OverlayWindowImageButton)

 public:
  explicit ToggleCameraButton(PressedCallback callback);
  ToggleCameraButton(const ToggleCameraButton&) = delete;
  ToggleCameraButton& operator=(const ToggleCameraButton&) = delete;
  ~ToggleCameraButton() override = default;

  void SetCameraState(bool is_turned_on);

  bool is_turned_on_for_testing() const { return is_turned_on_; }

 protected:
  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  void UpdateImageAndTooltipText();

  bool is_turned_on_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_TOGGLE_CAMERA_BUTTON_H_
