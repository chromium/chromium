// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_TRACK_IMAGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_TRACK_IMAGE_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "ui/views/controls/button/image_button.h"

namespace gfx {
struct VectorIcon;
}

namespace views {

// A resizable previous/next track image button.
class TrackImageButton : public views::ImageButton {
 public:
  explicit TrackImageButton(PressedCallback callback,
                            const gfx::VectorIcon& icon,
                            base::string16 label);

  // Overridden from views::View.
  void SetVisible(bool is_visible) override;

 protected:
  // Overridden from views::View.
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  const gfx::ImageSkia image_;

  // Last visible size of the image button.
  gfx::Size last_visible_size_;

  DISALLOW_COPY_AND_ASSIGN(TrackImageButton);
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_TRACK_IMAGE_BUTTON_H_
