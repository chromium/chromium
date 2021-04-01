// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_TOGGLE_MICROPHONE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_TOGGLE_MICROPHONE_BUTTON_H_

#include "ui/views/controls/button/image_button.h"

class ToggleMicrophoneButton : public views::ImageButton {
 public:
  explicit ToggleMicrophoneButton(PressedCallback callback);
  ToggleMicrophoneButton(const ToggleMicrophoneButton&) = delete;
  ToggleMicrophoneButton& operator=(const ToggleMicrophoneButton&) = delete;
  ~ToggleMicrophoneButton() override = default;

  void SetMutedState(bool is_muted);

 protected:
  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  void UpdateImageAndTooltipText();

  bool is_muted_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_TOGGLE_MICROPHONE_BUTTON_H_
