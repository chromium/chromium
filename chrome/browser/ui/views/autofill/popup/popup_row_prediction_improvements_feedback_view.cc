// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_prediction_improvements_feedback_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace autofill {

namespace {

// The size of the icons used in the buttons.
constexpr int kIconSize = 20;

// The button radius used to paint the background.
constexpr int kButtonRadius = 12;

// Creates the suggestion container view with its `text`.
// also looks for the "Learn more" substring in `text` and make it a link which
// will trigger `learn_more_cliked`.
std::unique_ptr<PopupRowContentView> CreateFeedbackContentView(
    base::RepeatingClosure learn_more_clicked) {
  auto feedback_container = std::make_unique<PopupRowContentView>();

  views::StyledLabel::RangeStyleInfo style_info =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          std::move(learn_more_clicked));
  std::vector<size_t> replacement_offsets;
  const std::u16string learn_more_link_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_ROW_LEARN_MORE);
  const std::u16string formatted_text = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_ROW_TITLE,
      /*replacements=*/{learn_more_link_text}, &replacement_offsets);

  feedback_container->SetFlexForView(
      feedback_container->AddChildView(
          views::Builder<views::StyledLabel>()
              .SetText(formatted_text)
              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
              .AddStyleRange(gfx::Range(replacement_offsets[0],
                                        replacement_offsets[0] +
                                            learn_more_link_text.length()),
                             style_info)
              // This is used in tests only.
              .SetID(PopupRowPredictionImprovementsFeedbackView::
                         kLearnMoreStyledLabelViewID)
              .Build()),
      1);

  return feedback_container;
}

void SetHoverStyleImageButton(views::ImageButton* button, bool hover) {
  views::InkDrop::Get(button->ink_drop_view())->GetInkDrop()->SetHovered(hover);
}

std::unique_ptr<views::ImageButton> CreateFeedbackButton(
    const gfx::VectorIcon& icon,
    base::RepeatingClosure button_action) {
  CHECK(icon.name == vector_icons::kThumbUpIcon.name ||
        icon.name == vector_icons::kThumbDownIcon.name);

  std::unique_ptr<views::ImageButton> button =
      views::CreateVectorImageButtonWithNativeTheme(button_action, icon,
                                                    kIconSize);
  const bool is_thumbs_up = icon.name == vector_icons::kThumbUpIcon.name;
  // TODO(b/362468426): Make these strings come from a finch config.
  const std::u16string tooltip = is_thumbs_up ? u"Thumbs up" : u"Thumbs down";

  views::InstallFixedSizeCircleHighlightPathGenerator(button.get(),
                                                      kButtonRadius);
  button->SetPreferredSize(gfx::Size(kButtonRadius * 2, kButtonRadius * 2));
  button->SetTooltipText(tooltip);
  button->GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
  button->SetLayoutManager(std::make_unique<views::BoxLayout>());
  button->GetViewAccessibility().SetIsIgnored(true);
  return button;
}

}  // namespace

PopupRowPredictionImprovementsFeedbackView::
    PopupRowPredictionImprovementsFeedbackView(
        AccessibilitySelectionDelegate& a11y_selection_delegate,
        SelectionDelegate& selection_delegate,
        base::WeakPtr<AutofillPopupController> controller,
        int line_number)
    : PopupRowView(
          a11y_selection_delegate,
          selection_delegate,
          controller,
          line_number,
          /*content_view=*/
          CreateFeedbackContentView(base::BindRepeating(
              &AutofillPopupController::PerformButtonActionForSuggestion,
              controller,
              line_number,
              PredictionImprovementsButtonActions::kLearnMoreClicked))) {
  // Create the feedback buttons.
  auto* buttons_wrapper =
      GetContentView().AddChildView(std::make_unique<views::BoxLayoutView>());
  buttons_wrapper->SetBetweenChildSpacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  thumbs_up_button_ = buttons_wrapper->AddChildView(CreateFeedbackButton(
      vector_icons::kThumbUpIcon,
      base::BindRepeating(
          &AutofillPopupController::PerformButtonActionForSuggestion,
          controller, line_number,
          PredictionImprovementsButtonActions::kThumbsUpClicked)));
  thumbs_down_button_ = buttons_wrapper->AddChildView(CreateFeedbackButton(
      vector_icons::kThumbDownIcon,
      base::BindRepeating(
          &AutofillPopupController::PerformButtonActionForSuggestion,
          controller, line_number,
          PredictionImprovementsButtonActions::kThumbsDownClicked)));

  auto* content_layout =
      static_cast<views::BoxLayout*>(GetContentView().GetLayoutManager());
  content_layout->SetFlexForView(buttons_wrapper, 0);
}

PopupRowPredictionImprovementsFeedbackView::
    ~PopupRowPredictionImprovementsFeedbackView() = default;

void PopupRowPredictionImprovementsFeedbackView::SetSelectedCell(
    std::optional<CellType> cell) {
  autofill::PopupRowView::SetSelectedCell(cell);
  if (cell != CellType::kContent) {
    // When the row is not selected, no button should have a hover style.
    SetHoverStyleImageButton(thumbs_up_button_, /*hover=*/false);
    SetHoverStyleImageButton(thumbs_down_button_, /*hover=*/false);
  }
}

BEGIN_METADATA(PopupRowPredictionImprovementsFeedbackView)
END_METADATA

}  // namespace autofill
