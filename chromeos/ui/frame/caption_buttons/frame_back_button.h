// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_BACK_BUTTON_H_
#define ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_BACK_BUTTON_H_

#include "base/component_export.h"
#include "ui/views/window/frame_caption_button.h"

namespace chromeos {

// A button to send back key events. It's used in Chrome hosted app windows,
// among other places.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) FrameBackButton
    : public views::FrameCaptionButton,
      public views::ButtonListener {
 public:
  FrameBackButton();
  ~FrameBackButton() override;

  // views::ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameBackButton);
};

}  // namespace chromeos

#endif  //  ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_BACK_BUTTON_H_
