// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_CAPTION_BUTTONS_CAPTION_BUTTON_MODEL_H_
#define CHROMEOS_UI_FRAME_CAPTION_BUTTONS_CAPTION_BUTTON_MODEL_H_

#include "ui/views/window/caption_button_types.h"

namespace chromeos {

// CaptionButtonModel describes the state of caption buttons
// for each CaptionButtonIcon types.
class CaptionButtonModel {
 public:
  virtual ~CaptionButtonModel() {}

  // Returns true if |type| is visible.
  virtual bool IsVisible(views::CaptionButtonIcon type) const = 0;

  // Returns true if |type| is enabled.
  virtual bool IsEnabled(views::CaptionButtonIcon type) const = 0;

  // In zoom mode, the maximize/restore button will be replaced
  // with zoom/unzoom button.
  virtual bool InZoomMode() const = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_CAPTION_BUTTONS_CAPTION_BUTTON_MODEL_H_
