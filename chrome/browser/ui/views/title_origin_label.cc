// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/title_origin_label.h"

std::unique_ptr<views::Label> CreateTitleOriginLabel(
    const std::u16string& text,
    const std::vector<std::pair<size_t, size_t>> bolded_ranges) {
  auto label =
      std::make_unique<views::Label>(text, views::style::CONTEXT_DIALOG_TITLE);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetCollapseWhenHidden(true);

  // Show the full origin in multiple lines. Breaking characters in the middle
  // of a word are explicitly allowed, as long origins are treated as one word.
  // Note that in English, GetWindowTitle() returns a string "$ORIGIN wants to".
  // In other languages, the non-origin part may appear before the
  // origin (e.g., in Filipino, "Gusto ng $ORIGIN na"). See crbug.com/40095827.
  label->SetElideBehavior(gfx::NO_ELIDE);
  label->SetMultiLine(true);
  label->SetAllowCharacterBreak(true);
  label->SetTextStyle(views::style::STYLE_HEADLINE_4);

  for (auto bolded_range : bolded_ranges) {
    label->SetTextStyleRange(
        views::style::STYLE_HEADLINE_4_BOLD,
        gfx::Range(bolded_range.first, bolded_range.second));
  }

  return label;
}
