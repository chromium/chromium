// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_CENTER_BUTTON_H_
#define CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_CENTER_BUTTON_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/gfx/render_text.h"
#include "ui/views/window/frame_caption_button.h"

namespace chromeos {

// A button to be shown at the center of the caption. The client of this class
// is responsible for managing the instance of this class, setting the primary
// image, and optionally setting a text and sub image.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) FrameCenterButton
    : public views::FrameCaptionButton {
  METADATA_HEADER(FrameCenterButton, views::FrameCaptionButton)

 public:
  FrameCenterButton(PressedCallback callback);
  FrameCenterButton(const FrameCenterButton&) = delete;
  FrameCenterButton& operator=(const FrameCenterButton&) = delete;
  ~FrameCenterButton() override;

  // views::View override:
  gfx::Size GetMinimumSize() const override;

  // Set the text to be painted next to the main icon. The text must be short
  // enough to fit the caption. Otherwise, the button wouldn't be drawn
  // properly.
  void SetText(std::optional<std::u16string> text);
  // Set the sub image to be painted next to the main icon and text.
  void SetSubImage(const gfx::VectorIcon& icon_image);

 protected:
  // views::View override:
  // Unlike other caption buttons, the size should be calculated dynamically as
  // this class may have an optional text and sub image.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // views::FrameCaptionButton override:
  void DrawHighlight(gfx::Canvas* canvas, cc::PaintFlags flags) override;
  void DrawIconContents(gfx::Canvas* canvas,
                        gfx::ImageSkia image,
                        int x,
                        int y,
                        cc::PaintFlags flags) override;
  // Returns the size of the inkdrop ripple. If the extra text and sub image
  // aren't set, it's simply the size of a circle with the radius of
  // |ink_drop_corner_radius_|.
  gfx::Size GetInkDropSize() const override;

 private:
  void OnBackgroundColorChanged();

  // The extra text shown in the button if set.
  std::unique_ptr<gfx::RenderText> text_;

  // The image id and image used to paint the sub icon if set.
  std::optional<gfx::ImageSkia> sub_icon_image_;
  raw_ptr<const gfx::VectorIcon> sub_icon_definition_ = nullptr;

  // Used to update the color of the optional contents when the background
  // color is updated.
  base::CallbackListSubscription background_color_changed_subscription_;
};

}  // namespace chromeos

#endif  //  CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_CENTER_BUTTON_H_
