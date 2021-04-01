// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_HANG_UP_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_HANG_UP_BUTTON_H_

#include "ui/views/controls/button/image_button.h"

class HangUpButton : public views::ImageButton {
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
