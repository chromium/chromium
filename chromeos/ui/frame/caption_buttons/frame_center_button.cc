// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/caption_buttons/frame_center_button.h"

#include <algorithm>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/numerics/safe_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/custom_frame_view.h"
#include "ui/views/window/frame_caption_button.h"

namespace chromeos {

namespace {

// The margin between the contents inside the button if several of them are set.
constexpr int kMarginBetweenContents = 3;

constexpr int kLeadingMarginJelly = 12;
constexpr int kLeadingMarginTextJelly = 8;
constexpr int kLeadingMarginSubIconJelly = 6;
constexpr int kTailingMarginJelly = 10;

constexpr float kDefaultHighlightOpacityForLight = 0.12f;
constexpr float kDefaultHighlightOpacityForDark = 0.20f;

}  // namespace

FrameCenterButton::FrameCenterButton(PressedCallback callback)
    : FrameCaptionButton(std::move(callback),
                         views::CAPTION_BUTTON_ICON_CENTER,
                         HTMENU) {
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CENTER));
  background_color_changed_subscription_ = AddBackgroundColorChangedCallback(
      base::BindRepeating(&FrameCenterButton::OnBackgroundColorChanged,
                          base::Unretained(this)));
}

FrameCenterButton::~FrameCenterButton() = default;

gfx::Size FrameCenterButton::GetMinimumSize() const {
  gfx::Size size = GetPreferredSize();
  // Similar to CalculatePreferredSize(), but allow the text width to be zero.
  size.set_width((sub_icon_image_
                      ? base::ClampCeil(icon_image().width() / 2.0f) +
                            kMarginBetweenContents +
                            base::ClampCeil(sub_icon_image_->width() / 2.0f)
                      : 0) +
                 views::GetCaptionButtonWidth());
  return size;
}

void FrameCenterButton::SetSubImage(const gfx::VectorIcon& icon_definition) {
  gfx::ImageSkia new_icon_image = gfx::CreateVectorIcon(
      icon_definition, GetButtonColor(GetBackgroundColor()));
  if (sub_icon_image_ &&
      new_icon_image.BackedBySameObjectAs(*sub_icon_image_)) {
    return;
  }
  sub_icon_definition_ = &icon_definition;
  sub_icon_image_ = new_icon_image;

  if (parent())
    parent()->InvalidateLayout();
}

void FrameCenterButton::SetText(absl::optional<std::u16string> text) {
  if (text_ && text_->text() == text)
    return;

  if (!text) {
    if (text_)
      text_.reset();
    return;
  }

  if (!text_) {
    std::unique_ptr<gfx::RenderText> render_text =
        gfx::RenderText::CreateRenderText();
    render_text->SetFontList(
        chromeos::features::IsJellyEnabled()
            ? gfx::FontList({"Google Sans", "Roboto"},
                            gfx::Font::FontStyle::NORMAL, 13,
                            gfx::Font::Weight::NORMAL)
            : views::CustomFrameView::GetWindowTitleFontList());
    render_text->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    render_text->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
    text_ = std::move(render_text);
  }
  text_->SetText(*text);

  if (parent())
    parent()->InvalidateLayout();
}

// |--------------[FrameCaptionButton width]--------------|
// | (i) |  (ii)  | (iii)  | (iv) |  (v)   | (vi) | (vii) |
// | primary icon | margin | text | margin |   sub icon   |
//
// (i) The left semicircle (views::kCaptionButtonWidth / 2)
// (ii) The right semicircle of the primary icon (icon_image().width() / 2)
// (iii) The margin between the primary icon and the text if set
//       (kMarginBetweenContents)
// (iv) The text if set (text_->GetStringSize().width())
// (v) The margin between the text and the sub icon (kMarginBetweenContents)
// (vi) The left semicircle of the sub icon (sub_icon_image_->width() / 2)
// (vii) The right semicircle of the sub icon (views::kCaptionButtonWidth / 2)
gfx::Size FrameCenterButton::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();

  if (chromeos::features::IsJellyEnabled()) {
    size.set_width(
        (text_ ? text_->GetStringSize().width() + kLeadingMarginTextJelly : 0) +
        (sub_icon_image_ ? sub_icon_image_->width() + kLeadingMarginSubIconJelly
                         : 0) +
        kLeadingMarginJelly + kTailingMarginJelly + icon_image().width());
    return size;
  }

  size.set_width(
      (text_ || sub_icon_image_ ? base::ClampCeil(icon_image().width() / 2.0f)
                                : 0) +
      (text_ ? kMarginBetweenContents + text_->GetStringSize().width() : 0) +
      (sub_icon_image_ ? kMarginBetweenContents +
                             base::ClampCeil(sub_icon_image_->width() / 2.0f)
                       : 0) +
      views::GetCaptionButtonWidth());
  return size;
}

void FrameCenterButton::DrawHighlight(gfx::Canvas* canvas,
                                      cc::PaintFlags flags) {
  const gfx::Size ink_drop_size = GetInkDropSize();
  int centered_origin_x = (width() - ink_drop_size.width()) / 2;
  int centered_origin_y = (height() - ink_drop_size.height()) / 2;
  canvas->DrawRoundRect(
      gfx::Rect(centered_origin_x, centered_origin_y, ink_drop_size.width(),
                ink_drop_size.height()),
      GetInkDropCornerRadius(), flags);
}

void FrameCenterButton::DrawIconContents(gfx::Canvas* canvas,
                                         gfx::ImageSkia image,
                                         int x,
                                         int y,
                                         cc::PaintFlags flags) {
  absl::optional<gfx::ImageSkia> left_icon = icon_image();
  absl::optional<gfx::ImageSkia> right_icon = sub_icon_image_;
  const bool is_rtl = base::i18n::IsRTL();
  if (is_rtl) {
    std::swap(left_icon, right_icon);
  }

  if (chromeos::features::IsJellyEnabled()) {
    // With Jelly, we want to have default highlight to make the button
    // prominent.
    cc::PaintFlags button_bg_flags;
    button_bg_flags.setColor(views::InkDrop::Get(this)->GetBaseColor());
    button_bg_flags.setAlphaf(color_utils::IsDark(GetBackgroundColor())
                                  ? kDefaultHighlightOpacityForDark
                                  : kDefaultHighlightOpacityForLight);
    button_bg_flags.setAntiAlias(true);
    DrawHighlight(canvas, button_bg_flags);

    int offset = is_rtl ? kTailingMarginJelly : kLeadingMarginJelly;
    if (left_icon) {
      canvas->DrawImageInt(*left_icon, offset,
                           (height() - left_icon->height()) / 2, flags);
      offset += left_icon->width();
    }

    if (text_) {
      offset += is_rtl ? kLeadingMarginSubIconJelly : kLeadingMarginTextJelly;
      const int max_text_width = width() - kLeadingMarginJelly -
                                 kTailingMarginJelly - icon_image().width() -
                                 (sub_icon_image_ ? kLeadingMarginSubIconJelly +
                                                        sub_icon_image_->width()
                                                  : 0);
      const gfx::Rect text_bounds =
          gfx::Rect(offset, (height() - text_->GetStringSize().height()) / 2,
                    std::min(text_->GetStringSize().width(), max_text_width),
                    text_->GetStringSize().height());
      text_->SetDisplayRect(text_bounds);
      text_->SetColor(SkColorSetA(GetButtonColor(GetBackgroundColor()),
                                  flags.getAlphaf() * SK_AlphaOPAQUE));
      text_->Draw(canvas);
      offset += text_bounds.width();
    }

    if (right_icon) {
      offset += is_rtl ? kLeadingMarginTextJelly : kLeadingMarginSubIconJelly;
      canvas->DrawImageInt(*right_icon, offset,
                           (height() - right_icon->height()) / 2, flags);
    }

    return;
  }

  int full_content_width =
      icon_image().width() +
      (text_ ? kMarginBetweenContents + text_->GetStringSize().width() : 0) +
      (sub_icon_image_ ? kMarginBetweenContents + sub_icon_image_->width() : 0);
  // The width available is basically the same as width(), but we need to
  // adjust the corner radius on both sides from views::kCaptionButtonWidth to
  // the actual content radius.
  int available_content_width =
      width() - views::GetCaptionButtonWidth() +
      base::ClampCeil(icon_image().width() / 2.0f) +
      (sub_icon_image_ ? base::ClampCeil(sub_icon_image_->width() / 2.0f)
                       : base::ClampCeil(icon_image().width() / 2.0f));
  int content_width = std::min(full_content_width, available_content_width);
  int current_offset = (width() - content_width) / 2;

  if (left_icon) {
    canvas->DrawImageInt(*left_icon, current_offset,
                         (height() - left_icon->height()) / 2, flags);
    current_offset += left_icon->width() + kMarginBetweenContents;
  }

  if (text_) {
    int available_text_width =
        content_width - icon_image().width() - kMarginBetweenContents -
        (sub_icon_image_ ? kMarginBetweenContents + sub_icon_image_->width()
                         : 0);
    gfx::Rect text_bounds = gfx::Rect(
        current_offset, (height() - text_->GetStringSize().height()) / 2,
        std::min(text_->GetStringSize().width(), available_text_width),
        text_->GetStringSize().height());
    text_->SetDisplayRect(text_bounds);
    text_->SetColor(SkColorSetA(GetButtonColor(GetBackgroundColor()),
                                flags.getAlphaf() * SK_AlphaOPAQUE));
    text_->Draw(canvas);
    current_offset += text_bounds.width() + kMarginBetweenContents;
  }

  if (right_icon) {
    canvas->DrawImageInt(*right_icon, current_offset,
                         (height() - right_icon->height()) / 2, flags);
  }
}

// The width calculated here is the same as that of CalculatePreferredSize()
// except that |ink_drop_corner_radius_| is used instead of
// |views::GetCaptionButtonWidth()|.
// See CalculatePreferredSize() for more details.
gfx::Size FrameCenterButton::GetInkDropSize() const {
  if (chromeos::features::IsJellyEnabled()) {
    return gfx::Size(width(), 2 * GetInkDropCornerRadius());
  }

  int full_width = 2 * GetInkDropCornerRadius();
  if (text_ || sub_icon_image_)
    full_width += icon_image().width() / 2;
  if (text_)
    full_width += kMarginBetweenContents + text_->GetStringSize().width();
  if (sub_icon_image_)
    full_width += kMarginBetweenContents + sub_icon_image_->width() / 2;
  return gfx::Size(std::min(full_width, width()), 2 * GetInkDropCornerRadius());
}

void FrameCenterButton::OnBackgroundColorChanged() {
  if (sub_icon_definition_)
    SetSubImage(*sub_icon_definition_);
  if (text_)
    text_->SetColor(GetButtonColor(GetBackgroundColor()));
}

BEGIN_METADATA(FrameCenterButton, views::FrameCaptionButton)
END_METADATA

}  // namespace chromeos
