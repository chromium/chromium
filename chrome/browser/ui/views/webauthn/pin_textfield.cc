// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/pin_textfield.h"

#include "base/strings/strcat.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/views/border.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"

namespace {

// Size specs of a pin cell.
constexpr int kCellWidth = 28;
constexpr int kCellHeight = 36;
constexpr int kCellSpacing = 8;

// Creates obscured render text for displaying a glyph in a specific pin cell.
std::unique_ptr<gfx::RenderText> CreatePinDigitRenderText(
    const gfx::FontList& font_list) {
  std::unique_ptr<gfx::RenderText> render_text =
      gfx::RenderText::CreateRenderText();
  render_text->SetCursorEnabled(false);
  render_text->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  render_text->SetObscured(true);
  render_text->SetFontList(font_list);
  return render_text;
}

}  // namespace

PinTextfield::PinTextfield(int pin_digits_amount)
    : views::Textfield(), pin_digits_count_(pin_digits_amount) {
  CHECK_GE(pin_digits_count_, 0);

  SetCursorEnabled(false);
  SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  // Custom border handling is implemented in `OnPaint`.
  SetBorder(views::CreateEmptyBorder(0));

  const gfx::FontList& font_list = views::TypographyProvider::Get().GetFont(
      views::style::CONTEXT_TEXTFIELD,
      views::style::TextStyle::STYLE_BODY_1_BOLD);
  for (int i = 0; i < pin_digits_count_; i++) {
    render_texts_.push_back(CreatePinDigitRenderText(font_list));
  }
}

PinTextfield::~PinTextfield() = default;

bool PinTextfield::AppendDigit(std::u16string digit) {
  if (digits_typed_count_ >= pin_digits_count_) {
    return false;
  }

  render_texts_[digits_typed_count_++]->SetText(std::move(digit));
  SchedulePaint();
  return true;
}

bool PinTextfield::RemoveDigit() {
  if (digits_typed_count_ <= 0) {
    return false;
  }

  render_texts_[--digits_typed_count_]->SetText(u"");
  SchedulePaint();
  return true;
}

std::u16string PinTextfield::GetPin() {
  std::u16string pin;
  for (int i = 0; i < digits_typed_count_; i++) {
    base::StrAppend(&pin, {render_texts_[i]->text()});
  }
  return pin;
}

void PinTextfield::SetObscured(bool obscured) {
  SetTextInputType(obscured ? ui::TEXT_INPUT_TYPE_PASSWORD
                            : ui::TEXT_INPUT_TYPE_TEXT);
  for (int i = 0; i < pin_digits_count_; i++) {
    render_texts_[i]->SetObscured(obscured);
  }
}

void PinTextfield::OnPaint(gfx::Canvas* canvas) {
  View::OnPaintBackground(canvas);

  cc::PaintFlags paint_flags;
  paint_flags.setStyle(cc::PaintFlags::kStroke_Style);
  paint_flags.setAntiAlias(true);

  for (int i = 0; i < pin_digits_count_; i++) {
    paint_flags.setColor(GetColorProvider()->GetColor(
        HasCellFocus(i) ? ui::kColorFocusableBorderFocused
                        : ui::kColorFocusableBorderUnfocused));
    float stroke_width = HasCellFocus(i) ? 2.f : 1.f;
    paint_flags.setStrokeWidth(stroke_width);

    gfx::Rect cell_rect(i * (kCellWidth + kCellSpacing), 0, kCellWidth,
                        kCellHeight);
    // Draw cell border.
    gfx::RectF cell_rect_f(cell_rect);
    cell_rect_f.Inset(stroke_width / 2.f);
    canvas->DrawRoundRect(cell_rect_f, 2.f, paint_flags);
    // Draw cell text.
    render_texts_[i]->SetDisplayRect(cell_rect);
    render_texts_[i]->Draw(canvas);
  }
}

gfx::Size PinTextfield::CalculatePreferredSize() const {
  return gfx::Size(
      pin_digits_count_ * kCellWidth + (pin_digits_count_ - 1) * kCellSpacing,
      kCellHeight);
}

void PinTextfield::OnThemeChanged() {
  views::View::OnThemeChanged();

  SkColor text_color =
      GetColorProvider()->GetColor(views::TypographyProvider::Get().GetColorId(
          views::style::CONTEXT_TEXTFIELD, views::style::STYLE_PRIMARY));
  for (int i = 0; i < pin_digits_count_; i++) {
    render_texts_[i]->SetColor(text_color);
  }
}

bool PinTextfield::HasCellFocus(int cell) const {
  int cell_with_focus = digits_typed_count_ == pin_digits_count_
                            ? pin_digits_count_ - 1
                            : digits_typed_count_;
  return HasFocus() && cell == cell_with_focus;
}

BEGIN_METADATA(PinTextfield)
END_METADATA
