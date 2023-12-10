// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/rich_answers_definition_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_text_label.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"

namespace quick_answers {

// RichAnswersDefinitionView
// -----------------------------------------------------------

RichAnswersDefinitionView::RichAnswersDefinitionView(
    const gfx::Rect& anchor_view_bounds,
    base::WeakPtr<QuickAnswersUiController> controller,
    DefinitionResult& definition_result)
    : RichAnswersView(anchor_view_bounds,
                      controller,
                      ResultType::kDefinitionResult),
      definition_result_(definition_result) {
  InitLayout();

  // TODO (b/274184670): Add custom focus behavior according to
  // approved greenlines.
}

RichAnswersDefinitionView::~RichAnswersDefinitionView() = default;

void RichAnswersDefinitionView::InitLayout() {
  // TODO (b/265254908): Populate definition view contents.
  content_view_ = GetContentView();

  AddHeaderViews();
}

void RichAnswersDefinitionView::AddHeaderViews() {
  auto* header_view = AddFillLayoutChildView(content_view_);
  std::unique_ptr<views::View> word_label =
      QuickAnswersTextLabel::CreateLabelWithStyle(
          definition_result_.word, GetFontList(TypographyToken::kCrosTitle1),
          kContentHeaderWidth,
          /*is_multi_line=*/false);
  std::unique_ptr<views::View> phonetics_label =
      QuickAnswersTextLabel::CreateLabelWithStyle(
          "/" + definition_result_.phonetics_info.text + "/",
          GetFontList(TypographyToken::kCrosBody2), kContentTextWidth,
          /*is_multi_line=*/true);

  // Display the word and phonetics in a single line heading if possible,
  // otherwise show the phonetics as a subheading below the word heading.
  if (word_label->width() + phonetics_label->width() <= kContentHeaderWidth) {
    std::vector<std::unique_ptr<views::View>> header_labels;
    header_labels.push_back(std::move(word_label));
    header_labels.push_back(std::move(phonetics_label));
    AddHorizontalViews(header_view, header_labels);
  } else {
    auto* header_text = header_view->AddChildView(
        views::Builder<views::FlexLayoutView>()
            .SetOrientation(views::LayoutOrientation::kHorizontal)
            .Build());
    header_text->AddChildView(std::move(word_label));
    AddFillLayoutChildView(content_view_, std::move(phonetics_label));
  }

  AddSettingsButtonTo(header_view);
}

BEGIN_METADATA(RichAnswersDefinitionView, RichAnswersView)
END_METADATA

}  // namespace quick_answers
