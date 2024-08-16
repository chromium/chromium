// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/pin_textfield.h"

#include "base/i18n/rtl.h"
#include "base/strings/strcat.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/render_text.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"

namespace {

// Size specs of a pin cell.
constexpr int kCellWidth = 30;
constexpr int kCellHeight = 36;
constexpr int kCellSpacing = 8;
constexpr float kCellRadius = 8.0;

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
  if (disabled_) {
    return false;
  }

  if (digits_typed_count_ >= pin_digits_count_) {
    return false;
  }

  render_texts_[digits_typed_count_++]->SetText(std::move(digit));
  SchedulePaint();
  UpdateAccessibilityAfterPinChange();
  return true;
}

bool PinTextfield::RemoveDigit() {
  if (disabled_) {
    return false;
  }

  if (digits_typed_count_ <= 0) {
    return false;
  }

  render_texts_[--digits_typed_count_]->SetText(u"");
  SchedulePaint();
  UpdateAccessibilityAfterPinChange();
  return true;
}

std::u16string PinTextfield::GetPin() {
  std::u16string pin;
  for (int i = 0; i < digits_typed_count_; i++) {
    base::StrAppend(&pin, {render_texts_[i]->text()});
  }
  return pin;
}

void PinTextfield::SetPin(const std::u16string& pin) {
  int pin_length = std::min(static_cast<int>(pin.length()), pin_digits_count_);
  for (int i = 0; i < pin_length; i++) {
    render_texts_[i]->SetText(std::u16string(1, pin[i]));
  }
  digits_typed_count_ = pin_length;
  SchedulePaint();
}

void PinTextfield::SetObscured(bool obscured) {
  if (obscured_ == obscured) {
    return;
  }

  obscured_ = obscured;
  GetViewAccessibility().SetIsProtected(obscured);
  for (int i = 0; i < pin_digits_count_; i++) {
    render_texts_[i]->SetObscured(obscured);
  }
  UpdateAccessibilityAfterPinChange();
  SchedulePaint();
}

void PinTextfield::SetDisabled(bool disabled) {
  if (disabled_ == disabled) {
    return;
  }

  disabled_ = disabled;
  SetTextInputType(disabled ? ui::TEXT_INPUT_TYPE_NONE
                            : ui::TEXT_INPUT_TYPE_PASSWORD);
  UpdateTextColor();
  SchedulePaint();
}

void PinTextfield::OnPaint(gfx::Canvas* canvas) {
  View::OnPaintBackground(canvas);

  cc::PaintFlags paint_flags;
  paint_flags.setStyle(cc::PaintFlags::kStroke_Style);
  paint_flags.setAntiAlias(true);

  SkColor background_color = GetColorProvider()->GetColor(
      disabled_ ? ui::kColorTextfieldBackgroundDisabled
                : ui::kColorTextfieldBackground);
  for (int i = 0; i < pin_digits_count_; i++) {
    paint_flags.setColor(GetColorProvider()->GetColor(
        disabled_ ? ui::kColorTextfieldOutlineDisabled
                  : (HasCellFocus(i) ? ui::kColorFocusableBorderFocused
                                     : ui::kColorTextfieldOutline)));
    float stroke_width = HasCellFocus(i) ? 2.f : 1.f;
    paint_flags.setStrokeWidth(stroke_width);

    // Drawing is adjusted in RTL so that the first cell is drawn rightmost.
    int index_rtl_adjusted =
        base::i18n::IsRTL() ? pin_digits_count_ - i - 1 : i;
    gfx::Rect cell_rect(index_rtl_adjusted * (kCellWidth + kCellSpacing), 0,
                        kCellWidth, kCellHeight);

    // Make sure background is not drawn outside of the rounded cell.
    SkPath path;
    path.addRoundRect(gfx::RectToSkRect(cell_rect), kCellRadius, kCellRadius);
    canvas->Save();
    canvas->ClipPath(path, /*do_anti_alias=*/true);

    // Draw cell background.
    canvas->FillRect(cell_rect, background_color);
    // Draw cell border.
    gfx::RectF cell_rect_f(cell_rect);
    cell_rect_f.Inset(stroke_width / 2.f);
    canvas->DrawRoundRect(cell_rect_f, kCellRadius, paint_flags);
    // Draw cell text.
    render_texts_[i]->SetDisplayRect(cell_rect);
    render_texts_[i]->Draw(canvas);

    canvas->Restore();
  }
}

gfx::Size PinTextfield::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(
      pin_digits_count_ * kCellWidth + (pin_digits_count_ - 1) * kCellSpacing,
      kCellHeight);
}

void PinTextfield::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateTextColor();
}

void PinTextfield::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  std::u16string pin = GetPin();
  node_data->SetValue(
      (obscured_ || disabled_)
          ? std::u16string(pin.size(),
                           gfx::RenderText::kPasswordReplacementChar)
          : pin);
}

void PinTextfield::UpdateAccessibleTextSelection() {
  // Pin textfield does not support selecting characters, set it to an empty
  // selection at the end of the currently typed pin.
  GetViewAccessibility().SetTextSelStart(digits_typed_count_);
  GetViewAccessibility().SetTextSelEnd(digits_typed_count_);
}

bool PinTextfield::HasCellFocus(int cell) const {
  int cell_with_focus = digits_typed_count_ == pin_digits_count_
                            ? pin_digits_count_ - 1
                            : digits_typed_count_;
  return HasFocus() && cell == cell_with_focus;
}

void PinTextfield::UpdateAccessibilityAfterPinChange() {
  UpdateAccessibleTextSelection();
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged,
                           /*send_native_event=*/true);

  // Don't announce the selected text (last typed digit) in `obscured_` mode.
  if (!obscured_) {
    NotifyAccessibilityEvent(ax::mojom::Event::kTextSelectionChanged,
                             /*send_native_event=*/true);
  }
}

void PinTextfield::UpdateTextColor() {
  if (!GetWidget()) {
    return;
  }

  int text_style =
      disabled_ ? views::style::STYLE_DISABLED : views::style::STYLE_PRIMARY;
  SkColor text_color =
      GetColorProvider()->GetColor(views::TypographyProvider::Get().GetColorId(
          views::style::CONTEXT_TEXTFIELD, text_style));
  for (int i = 0; i < pin_digits_count_; i++) {
    render_texts_[i]->SetColor(text_color);
  }
}

BEGIN_METADATA(PinTextfield)
END_METADATA
