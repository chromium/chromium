// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_SIMPLE_OVERLAY_WINDOW_IMAGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_SIMPLE_OVERLAY_WINDOW_IMAGE_BUTTON_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace gfx {
struct VectorIcon;
}

// A simple image button with a custom icon and label.
class SimpleOverlayWindowImageButton : public OverlayWindowImageButton {
  METADATA_HEADER(SimpleOverlayWindowImageButton, OverlayWindowImageButton)

 public:
  explicit SimpleOverlayWindowImageButton(PressedCallback callback,
                                          const gfx::VectorIcon& icon,
                                          std::u16string label);
  SimpleOverlayWindowImageButton(const SimpleOverlayWindowImageButton&) =
      delete;
  SimpleOverlayWindowImageButton& operator=(
      const SimpleOverlayWindowImageButton&) = delete;
  ~SimpleOverlayWindowImageButton() override = default;

  // Overridden from views::View.
  void SetVisible(bool is_visible) override;

 protected:
  // Overridden from views::View.
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  void UpdateImage();

  const raw_ref<const gfx::VectorIcon> icon_;

  // Last visible size of the image button.
  gfx::Size last_visible_size_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_SIMPLE_OVERLAY_WINDOW_IMAGE_BUTTON_H_
