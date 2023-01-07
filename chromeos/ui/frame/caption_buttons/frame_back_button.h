// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_BACK_BUTTON_H_
#define CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_BACK_BUTTON_H_

#include "base/component_export.h"
#include "ui/views/window/frame_caption_button.h"

namespace chromeos {

// A button to send back key events. It's used in Chrome hosted app windows,
// among other places.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) FrameBackButton
    : public views::FrameCaptionButton {
 public:
  FrameBackButton();

  FrameBackButton(const FrameBackButton&) = delete;
  FrameBackButton& operator=(const FrameBackButton&) = delete;

  ~FrameBackButton() override;

 private:
  void ButtonPressed();
};

}  // namespace chromeos

#endif  //  CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_BACK_BUTTON_H_
