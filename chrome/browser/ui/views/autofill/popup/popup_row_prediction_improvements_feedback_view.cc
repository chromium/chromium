// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_prediction_improvements_feedback_view.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
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
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace autofill {

namespace {

// The size of the icons used in the buttons.
constexpr int kIconSize = 16;

// The button radius used to paint the background.
constexpr int kButtonRadius = 12;

// Creates the suggestion container view.
std::unique_ptr<PopupRowContentView> CreateFeedbackContentView(
    base::RepeatingClosure manage_prediction_improvements_clicked) {
  // First create the outer container, which contains text details about the
  // feature and then the feedback section.
  auto feedback_outer_container = std::make_unique<PopupRowContentView>();
  feedback_outer_container->SetInsideBorderInsets(
      gfx::Insets(autofill::PopupBaseView::ArrowHorizontalMargin()));
  feedback_outer_container->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  feedback_outer_container->SetBetweenChildSpacing(
      autofill::PopupBaseView::ArrowHorizontalMargin());
  feedback_outer_container->SetMainAxisAlignment(
      views::LayoutAlignment::kStart);
  feedback_outer_container->AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_DETAILS))
          .SetTextStyle(views::style::STYLE_BODY_5)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetMultiLine(true)
          .Build());

  // The feedback section, containing general text about it. Later on, thumbs up
  // and down buttons are added.
  auto feedback_title_and_button_container =
      views::Builder<views::BoxLayoutView>()
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          .SetID(PopupRowPredictionImprovementsFeedbackView::
                     kFeedbackTextAndButtonsContainerViewID)
          .Build();
  views::StyledLabel::RangeStyleInfo style_info =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          std::move(manage_prediction_improvements_clicked));
  style_info.text_style = views::style::TextStyle::STYLE_LINK_5;

  std::vector<size_t> replacement_offsets;
  const std::u16string manage_prediction_improvements_link_text =
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGE_PREDICTION_IMPROVEMENTS);
  const std::u16string formatted_text = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_TEXT,
      /*replacements=*/{manage_prediction_improvements_link_text},
      &replacement_offsets);

  feedback_title_and_button_container->SetFlexForView(
      feedback_title_and_button_container->AddChildView(
          views::Builder<views::StyledLabel>()
              .SetText(formatted_text)
              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
              .SetDefaultTextStyle(views::style::STYLE_BODY_5)
              .AddStyleRange(
                  gfx::Range(
                      replacement_offsets[0],
                      replacement_offsets[0] +
                          manage_prediction_improvements_link_text.length()),
                  style_info)
              // This is used in tests only.
              .SetID(PopupRowPredictionImprovementsFeedbackView::
                         kLearnMoreStyledLabelViewID)
              .Build()),
      1);

  feedback_outer_container->AddChildView(
      std::move(feedback_title_and_button_container));
  return feedback_outer_container;
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
  const std::u16string tooltip =
      is_thumbs_up
          ? l10n_util::GetStringUTF16(
                IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_THUMBS_UP_BUTTON_TOOLTIP)
          : l10n_util::GetStringUTF16(
                IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_THUMBS_DOWN_BUTTON_TOOLTIP);

  views::InstallFixedSizeCircleHighlightPathGenerator(button.get(),
                                                      kButtonRadius);
  button->SetPreferredSize(gfx::Size(kButtonRadius * 2, kButtonRadius * 2));
  button->SetTooltipText(tooltip);
  button->GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
  button->SetLayoutManager(std::make_unique<views::BoxLayout>());
  button->GetViewAccessibility().SetIsIgnored(true);
  return button;
}

// The focusable controls form a visually recognizable horizontal line of
// elements on the UI. These controls are focused by the LEFT/RIGHT keys.
// This function defines the order in which controls are selected and its list
// must be in sync with `PopupRowPredictionImprovementsFeedbackView`'s rendered
// views. It returns the `FocusableControl` which should have focus next after
// the reference `control`, wrapping the list, so that if the current `control`
// is the last one, the next will be the first element if moving `forward`.
PopupRowPredictionImprovementsFeedbackView::FocusableControl
GetNextHorizontalFocusableControl(
    PopupRowPredictionImprovementsFeedbackView::FocusableControl control,
    bool forward) {
  using Control = PopupRowPredictionImprovementsFeedbackView::FocusableControl;
  static constexpr std::array kControls{
      Control::kManagePredictionImprovementsLink, Control::kThumbsUp,
      Control::kThumbsDown};
  const auto found_it = std::find(kControls.begin(), kControls.end(), control);
  CHECK(found_it != kControls.end());

  const int index = found_it - kControls.begin();
  const int step = forward ? +1 : -1;
  const int next_index = (index + step + kControls.size()) % kControls.size();

  return kControls[next_index];
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
  auto* feedback_text_and_buttons_container =
      GetContentView().GetViewByID(PopupRowPredictionImprovementsFeedbackView::
                                       kFeedbackTextAndButtonsContainerViewID);
  CHECK(feedback_text_and_buttons_container);
  auto* buttons_wrapper = feedback_text_and_buttons_container->AddChildView(
      std::make_unique<views::BoxLayoutView>());
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

  auto* content_layout = static_cast<views::BoxLayout*>(
      feedback_text_and_buttons_container->GetLayoutManager());
  content_layout->SetFlexForView(buttons_wrapper, 0);
}

PopupRowPredictionImprovementsFeedbackView::
    ~PopupRowPredictionImprovementsFeedbackView() = default;

void PopupRowPredictionImprovementsFeedbackView::SetSelectedCell(
    std::optional<CellType> cell) {
  autofill::PopupRowView::SetSelectedCell(cell);
  UpdateFocusedControl(
      cell == CellType::kContent
          ? std::optional(FocusableControl::kManagePredictionImprovementsLink)
          : std::nullopt);
}

bool PopupRowPredictionImprovementsFeedbackView::HandleKeyPressEvent(
    const input::NativeWebKeyboardEvent& event) {
  if (!focused_control_.has_value()) {
    return PopupRowView::HandleKeyPressEvent(event);
  }

  switch (event.windows_key_code) {
    case ui::VKEY_LEFT:
      UpdateFocusedControl(GetNextHorizontalFocusableControl(
          *focused_control_, /*forward=*/base::i18n::IsRTL()));
      return true;
    case ui::VKEY_RIGHT:
      UpdateFocusedControl(GetNextHorizontalFocusableControl(
          *focused_control_, /*forward=*/!base::i18n::IsRTL()));
      return true;
    case ui::VKEY_RETURN:
      switch (*focused_control_) {
        case FocusableControl::kManagePredictionImprovementsLink:
          controller()->PerformButtonActionForSuggestion(
              line_number(),
              PredictionImprovementsButtonActions::kLearnMoreClicked);
          break;
        case FocusableControl::kThumbsUp:
          controller()->PerformButtonActionForSuggestion(
              line_number(),
              PredictionImprovementsButtonActions::kThumbsUpClicked);
          break;
        case FocusableControl::kThumbsDown:
          controller()->PerformButtonActionForSuggestion(
              line_number(),
              PredictionImprovementsButtonActions::kThumbsDownClicked);
          break;
      }
      UpdateFocusedControl(std::nullopt);
      return true;
  }

  return PopupRowView::HandleKeyPressEvent(event);
}

void PopupRowPredictionImprovementsFeedbackView::UpdateFocusedControl(
    std::optional<FocusableControl> focused_control) {
  focused_control_ = focused_control;

  SetHoverStyleImageButton(thumbs_up_button_, /*hover=*/focused_control_ ==
                                                  FocusableControl::kThumbsUp);
  SetHoverStyleImageButton(
      thumbs_down_button_,
      /*hover=*/focused_control_ == FocusableControl::kThumbsDown);
}

BEGIN_METADATA(PopupRowPredictionImprovementsFeedbackView)
END_METADATA

}  // namespace autofill
