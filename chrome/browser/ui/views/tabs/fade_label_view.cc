// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/fade_label_view.h"

#include "chrome/browser/ui/views/tabs/filename_elider.h"
#include "ui/gfx/canvas.h"

namespace {
template <typename T>
std::unique_ptr<T> CreateLabel(int context) {
  return std::make_unique<T>(std::u16string(), context,
                             views::style::STYLE_PRIMARY);
}
}  // namespace

// FadeLabel
// ----------------------------------------------------------

void FadeLabel::SetData(const FadeLabelViewData& data) {
  data_ = data;
  std::u16string text = data.text;
  const bool is_filename = data.is_filename;
  SetElideBehavior(is_filename ? gfx::NO_ELIDE : gfx::ELIDE_TAIL);
  if (is_filename) {
    text = TruncateFilenameToTwoLines(text);
  }
  SetText(text);
}

void FadeLabel::SetFade(double percent) {
  percent = std::min(1.0, percent);
  const SkAlpha alpha = base::saturated_cast<SkAlpha>(
      std::numeric_limits<SkAlpha>::max() * (1.0 - percent));
  SetBackgroundColor(SkColorSetA(GetBackgroundColor(), alpha));
  SetEnabledColor(SkColorSetA(GetEnabledColor(), alpha));
}

void FadeLabel::SetPaintBackground(bool paint_background) {
  paint_background_ = paint_background;
}

void FadeLabel::OnPaintBackground(gfx::Canvas* canvas) {
  if (paint_background_) {
    canvas->DrawColor(GetBackgroundColor());
  } else {
    views::Label::OnPaintBackground(canvas);
  }
}

// Returns a version of the text that's middle-elided on two lines.
std::u16string FadeLabel::TruncateFilenameToTwoLines(
    const std::u16string& text) const {
  FilenameElider elider(CreateRenderText());
  gfx::Rect text_rect = GetContentsBounds();
  text_rect.Inset(-gfx::ShadowValue::GetMargin(GetShadows()));
  return elider.Elide(text, text_rect);
}

// FadeLabelView:
// ----------------------------------------------------------

FadeLabelView::FadeLabelView(int context, int num_lines)
    : FadeView<FadeLabel, FadeLabel, FadeLabelViewData>(
          CreateLabel<FadeLabel>(context),
          CreateLabel<FadeLabel>(context)) {
  if (num_lines > 1) {
    primary_view_->SetMultiLine(true);
    fade_out_view_->SetMultiLine(true);
    primary_view_->SetMaxLines(num_lines);
    fade_out_view_->SetMaxLines(num_lines);
  }

  fade_out_view_->SetPaintBackground(true);
}

std::u16string FadeLabelView::GetText() {
  return primary_view_->GetText();
}
