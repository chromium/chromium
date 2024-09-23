// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/indexed_suggestion_candidate_button.h"

#include "chrome/browser/ui/ash/input_method/colors.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"

namespace ui::ime {
const int kTopPadding = 4;
const int kBottomPadding = 0;
const int kLeftRightPadding = 2;
const int kBetweenSpacing = 1;
const int kBorderRadius = 2;
const int kCandidateSquareSide = 24;
const views::Label::CustomFont kCandidateTextFont = {
    .font_list = gfx::FontList(gfx::FontList({"Roboto"},
                                             gfx::Font::NORMAL,
                                             16,
                                             gfx::Font::Weight::MEDIUM))};
const views::Label::CustomFont kIndexFont = {
    .font_list = gfx::FontList(gfx::FontList({"Roboto"},
                                             gfx::Font::NORMAL,
                                             10,
                                             gfx::Font::Weight::MEDIUM))};

IndexedSuggestionCandidateButton::IndexedSuggestionCandidateButton(
    PressedCallback callback,
    const std::u16string& candidate_text,
    const std::u16string& index_text,
    bool create_legacy_candidate)
    : views::Button(std::move(callback)) {
  GetViewAccessibility().SetName(candidate_text);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets()
          .set_left_right(kLeftRightPadding, kLeftRightPadding)
          .set_top_bottom(kTopPadding, kBottomPadding),
      /* between_child_spacing=*/kBetweenSpacing));

  if (create_legacy_candidate) {
    // TODO(b/240357416): Remove when emoji suggestions uses horizontal layout.
    BuildLegacyCandidate(candidate_text);
  } else {
    BuildCandidate(candidate_text, index_text);
  }
}

void IndexedSuggestionCandidateButton::SetHighlight(bool highlighted) {
  if (highlighted && !background()) {
    // Legacy option does not have a rounded border.
    // TODO(b/240357416): Remove legacy option when emoji suggestions uses
    // horizontal layout.
    int border_radius = is_legacy_candidate_ ? 0 : kBorderRadius;
    SetBackground(views::CreateRoundedRectBackground(
        ResolveSemanticColor(cros_styles::ColorName::kRippleColor),
        border_radius));
  }
  if (!highlighted && background()) {
    SetBackground(nullptr);
  }
}

void IndexedSuggestionCandidateButton::BuildLegacyCandidate(
    const std::u16string& candidate_text) {
  is_legacy_candidate_ = true;
  AddChildView(
      std::make_unique<views::Label>(candidate_text, kCandidateTextFont));
}

void IndexedSuggestionCandidateButton::BuildCandidate(
    const std::u16string& candidate_text,
    const std::u16string& index_text) {
  is_legacy_candidate_ = false;

  // Displaying the candidate text (i.e. the "A" in the diagram below).
  //   +---+
  //   | A | <-- label being created
  //   |   |
  //   | 1 |
  //   +---+

  // Wrapper uses views::Label since it doesn't override the hover state of the
  // button.
  auto* candidate_wrapper = AddChildView(std::make_unique<views::Label>());
  views::FlexLayout* candidate_wrapper_layout =
      candidate_wrapper->SetLayoutManager(
          std::make_unique<views::FlexLayout>());
  candidate_wrapper_layout->SetCrossAxisAlignment(
      views::LayoutAlignment::kCenter);
  candidate_wrapper_layout->SetMainAxisAlignment(
      views::LayoutAlignment::kCenter);
  candidate_wrapper->SetProperty(views::kBoxLayoutFlexKey,
                                 views::BoxLayoutFlexSpecification());
  candidate_wrapper->SetPreferredSize(
      gfx::Size(kCandidateSquareSide, kCandidateSquareSide));
  candidate_wrapper->AddChildView(
      std::make_unique<views::Label>(candidate_text, kCandidateTextFont));

  // Displaying the index (i.e. the "1" in the diagram below).
  //   +---+
  //   | A |
  //   |   |
  //   | 1 | <-- label being created
  //   +---+
  auto* candidate_text_label =
      AddChildView(std::make_unique<views::Label>(index_text, kIndexFont));
  candidate_text_label->SetEnabledColor(
      ResolveSemanticColor(cros_styles::ColorName::kTextColorSecondary));
}

IndexedSuggestionCandidateButton::~IndexedSuggestionCandidateButton() = default;

BEGIN_METADATA(IndexedSuggestionCandidateButton)
END_METADATA

}  // namespace ui::ime
