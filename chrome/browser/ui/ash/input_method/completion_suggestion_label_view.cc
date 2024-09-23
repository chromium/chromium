// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/completion_suggestion_label_view.h"

#include "chrome/browser/ui/ash/input_method/colors.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ui {
namespace ime {

const char CompletionSuggestionLabelView::kFontName[] = "Roboto";

CompletionSuggestionLabelView::CompletionSuggestionLabelView() {
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetAutoColorReadabilityEnabled(false);
  // StyledLabel eats event, probably because it has to handle links.
  // Explicitly sets can_process_events_within_subtree to false for
  // hover to work correctly.
  SetCanProcessEventsWithinSubtree(false);
}

void CompletionSuggestionLabelView::SetPrefixAndPrediction(
    const std::u16string& prefix,
    const std::u16string& prediction) {
  const gfx::FontList kSuggestionFont({kFontName}, gfx::Font::NORMAL, kFontSize,
                                      gfx::Font::Weight::NORMAL);
  // SetText clears the existing style only if the text to set is different from
  // the previous one.
  SetText(u"");
  SetText(prefix + prediction);

  // Create style range for prefix if it's not empty.
  if (!prefix.empty()) {
    views::StyledLabel::RangeStyleInfo prefix_style;
    prefix_style.custom_font = kSuggestionFont;
    prefix_style.override_color =
        ResolveSemanticColor(cros_styles::ColorName::kTextColorPrimary);
    AddStyleRange(gfx::Range(0, prefix.length()), prefix_style);
  }

  // Create style range for the prediction.
  views::StyledLabel::RangeStyleInfo prediction_style;
  prediction_style.custom_font = kSuggestionFont;
  prediction_style.override_color =
      ResolveSemanticColor(cros_styles::ColorName::kTextColorSecondary);
  AddStyleRange(
      gfx::Range(prefix.length(), prefix.length() + prediction.length()),
      prediction_style);

  // TODO(crbug.com/40137343): Add tests to check view's height and width with
  // a non-empty prefix.
  // Maximum width for suggestion.
  SizeToFit(448);
}

int CompletionSuggestionLabelView::GetPrefixWidthPx() const {
  if (children().size() == 2) {
    return static_cast<views::Label*>(children()[0])->width();
  }
  return 0;
}

BEGIN_METADATA(CompletionSuggestionLabelView)
END_METADATA

}  // namespace ime
}  // namespace ui
