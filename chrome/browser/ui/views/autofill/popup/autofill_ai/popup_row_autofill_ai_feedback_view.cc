// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/autofill_ai/popup_row_autofill_ai_feedback_view.h"

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
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
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
#include "ui/views/view_class_properties.h"

namespace autofill_ai {

namespace {

using autofill::AutofillPopupController;
using autofill::PopupRowContentView;
using autofill::PopupRowView;

// The size of the icons used in the buttons.
constexpr int kIconSize = 16;

// The button radius used to paint the background.
constexpr int kButtonRadius = 12;

constexpr int kContentHorizontalPadding = 12;
constexpr int kContentVerticalPadding = 8;
constexpr int kFeedbackSuggestionTargetWidth = 320;

int GetButtonsWrapperInsiderBorderHorizontalMargin() {
  return kContentHorizontalPadding;
}

int GetBetweenButtonsMargin() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);
}

class FeedbackLabel : public views::StyledLabel {
  METADATA_HEADER(FeedbackLabel, views::StyledLabel)
 public:
  FeedbackLabel() = default;

  // views::StyledLabel:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    if (available_size.width().is_bounded()) {
      return views::StyledLabel::CalculatePreferredSize(available_size);
    }
    // Subtrack the expected size for the button wrapper from the target width.
    //
    // Expected size for one button, which is its diameter.
    int button_size = kButtonRadius * 2;
    // This is the margin added around the buttons, x2 because it is added in
    // the left and in the right.
    int outer_margins = GetButtonsWrapperInsiderBorderHorizontalMargin() * 2;
    // The margin/space between the buttons.
    int between_buttons_margin = GetBetweenButtonsMargin();
    // The total size from the button section is:
    // 1. button_size * 2 (there are 2 buttons), plus
    // 2. The outer margins, plus
    // 3. The space between the buttons.
    return GetLayoutSizeInfoForWidth(
               kFeedbackSuggestionTargetWidth -
               (button_size * 2 + outer_margins + between_buttons_margin))
        .total_size;
  }
};

BEGIN_METADATA(FeedbackLabel)
END_METADATA

// Creates the suggestion container view.
std::unique_ptr<PopupRowContentView> CreateFeedbackContentView(
    base::RepeatingClosure manage_prediction_improvements_clicked) {
  // First create the outer container, which contains text details about the
  // feature and then the feedback section.
  auto feedback_outer_container = std::make_unique<PopupRowContentView>();
  // Reset the default `PopupRowContentView` padding, here it's managed by
  // the inner views, to make proper background highlighting possible.
  feedback_outer_container->SetInsideBorderInsets(gfx::Insets(0));
  feedback_outer_container->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  feedback_outer_container->SetMainAxisAlignment(
      views::LayoutAlignment::kStart);
  if (autofill_ai::kShowDetailsText.Get()) {
    feedback_outer_container->AddChildView(
        views::Builder<views::Label>()
            .SetText(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_DETAILS))
            .SetTextStyle(views::style::STYLE_BODY_5)
            .SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
                kContentVerticalPadding, kContentHorizontalPadding)))
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .SetMultiLine(true)
            .Build());
  }

  // The feedback section, containing general text about it. Later on, thumbs up
  // and down buttons are added.
  auto feedback_title_and_button_container =
      views::Builder<views::BoxLayoutView>()
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          .SetID(PopupRowAutofillAiFeedbackView::
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
          views::Builder<views::StyledLabel>(std::make_unique<FeedbackLabel>())
              .SetText(formatted_text)
              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
              .SetDefaultTextStyle(views::style::STYLE_BODY_5)
              .SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
                  kContentVerticalPadding, kContentHorizontalPadding)))
              .AddStyleRange(
                  gfx::Range(
                      replacement_offsets[0],
                      replacement_offsets[0] +
                          manage_prediction_improvements_link_text.length()),
                  style_info)
              .SetID(
                  PopupRowAutofillAiFeedbackView::kLearnMoreStyledLabelViewID)
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
// must be in sync with `PopupRowAutofillAiFeedbackView`'s rendered
// views. It returns the `FocusableControl` which should have focus next after
// the reference `control`, wrapping the list, so that if the current `control`
// is the last one, the next will be the first element if moving `forward`.
PopupRowAutofillAiFeedbackView::FocusableControl
GetNextHorizontalFocusableControl(
    PopupRowAutofillAiFeedbackView::FocusableControl control,
    bool forward) {
  using Control = PopupRowAutofillAiFeedbackView::FocusableControl;
  static constexpr std::array kControls{
      Control::kManageAutofillAiLink, Control::kThumbsUp, Control::kThumbsDown};
  const auto found_it = std::find(kControls.begin(), kControls.end(), control);
  CHECK(found_it != kControls.end());

  const int index = found_it - kControls.begin();
  const int step = forward ? +1 : -1;
  const int next_index = (index + step + kControls.size()) % kControls.size();

  return kControls[next_index];
}

}  // namespace

PopupRowAutofillAiFeedbackView::PopupRowAutofillAiFeedbackView(
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
              autofill::AutofillAiSuggestionButtonAction::kLearnMoreClicked))) {
  CHECK(line_number < controller->GetLineCount() &&
        !controller->GetSuggestionAt(line_number).voice_over->empty());

  manage_prediction_improvements_link_ = GetContentView().GetViewByID(
      PopupRowAutofillAiFeedbackView::kLearnMoreStyledLabelViewID);
  CHECK(manage_prediction_improvements_link_);

  // Create the feedback buttons.
  auto* feedback_text_and_buttons_container = GetContentView().GetViewByID(
      PopupRowAutofillAiFeedbackView::kFeedbackTextAndButtonsContainerViewID);
  CHECK(feedback_text_and_buttons_container);
  auto* buttons_wrapper = feedback_text_and_buttons_container->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetInsideBorderInsets(gfx::Insets::VH(
              0, GetButtonsWrapperInsiderBorderHorizontalMargin()))
          .Build());
  buttons_wrapper->SetBetweenChildSpacing(GetBetweenButtonsMargin());
  thumbs_up_button_ = buttons_wrapper->AddChildView(CreateFeedbackButton(
      vector_icons::kThumbUpIcon,
      base::BindRepeating(
          &AutofillPopupController::PerformButtonActionForSuggestion,
          controller, line_number,
          autofill::AutofillAiSuggestionButtonAction::kThumbsUpClicked)));
  thumbs_down_button_ = buttons_wrapper->AddChildView(CreateFeedbackButton(
      vector_icons::kThumbDownIcon,
      base::BindRepeating(
          &AutofillPopupController::PerformButtonActionForSuggestion,
          controller, line_number,
          autofill::AutofillAiSuggestionButtonAction::kThumbsDownClicked)));

  auto* content_layout = static_cast<views::BoxLayout*>(
      feedback_text_and_buttons_container->GetLayoutManager());
  content_layout->SetFlexForView(buttons_wrapper, 0);
}

PopupRowAutofillAiFeedbackView::~PopupRowAutofillAiFeedbackView() = default;

void PopupRowAutofillAiFeedbackView::SetSelectedCell(
    std::optional<CellType> cell) {
  autofill::PopupRowView::SetSelectedCell(cell);
  UpdateFocusedControl(
      cell == CellType::kContent
          ? std::optional(FocusableControl::kManageAutofillAiLink)
          : std::nullopt);
}

bool PopupRowAutofillAiFeedbackView::HandleKeyPressEvent(
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
        case FocusableControl::kManageAutofillAiLink:
          controller()->PerformButtonActionForSuggestion(
              line_number(),
              autofill::AutofillAiSuggestionButtonAction::kLearnMoreClicked);
          break;
        case FocusableControl::kThumbsUp:
          controller()->PerformButtonActionForSuggestion(
              line_number(),
              autofill::AutofillAiSuggestionButtonAction::kThumbsUpClicked);
          break;
        case FocusableControl::kThumbsDown:
          controller()->PerformButtonActionForSuggestion(
              line_number(),
              autofill::AutofillAiSuggestionButtonAction::kThumbsDownClicked);
          break;
      }
      UpdateFocusedControl(std::nullopt);
      return true;
  }

  return PopupRowView::HandleKeyPressEvent(event);
}

void PopupRowAutofillAiFeedbackView::OnCellSelected(
    std::optional<CellType> type,
    autofill::PopupCellSelectionSource source) {
  if (source == autofill::PopupCellSelectionSource::kMouse) {
    return;
  }

  PopupRowView::OnCellSelected(type, source);
}

views::View& PopupRowAutofillAiFeedbackView::GetFocusableControlView(
    FocusableControl focused_control) {
  switch (focused_control) {
    case FocusableControl::kManageAutofillAiLink:
      return *this;
    case FocusableControl::kThumbsUp:
      return *thumbs_up_button_;
    case FocusableControl::kThumbsDown:
      return *thumbs_down_button_;
  }
}

void PopupRowAutofillAiFeedbackView::UpdateFocusedControl(
    std::optional<FocusableControl> new_focused_control) {
  if (focused_control_ == new_focused_control) {
    return;
  }

  const std::optional<FocusableControl>& current_focused_control =
      focused_control_;

  if (current_focused_control) {
    GetFocusableControlView(*current_focused_control)
        .GetViewAccessibility()
        .SetIsSelected(false);
    NotifyAccessibilityEvent(ax::mojom::Event::kSelectedChildrenChanged, true);
  }

  if (new_focused_control) {
    views::View& view = GetFocusableControlView(*new_focused_control);
    GetA11ySelectionDelegate().NotifyAXSelection(view);
    view.GetViewAccessibility().SetIsSelected(true);
    NotifyAccessibilityEvent(ax::mojom::Event::kSelectedChildrenChanged, true);
  }

  manage_prediction_improvements_link_->SetBackground(
      new_focused_control == FocusableControl::kManageAutofillAiLink
          ? views::CreateThemedRoundedRectBackground(
                ui::kColorDropdownBackgroundSelected,
                ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
                    views::Emphasis::kMedium))
          : nullptr);

  SetHoverStyleImageButton(thumbs_up_button_, /*hover=*/new_focused_control ==
                                                  FocusableControl::kThumbsUp);
  SetHoverStyleImageButton(
      thumbs_down_button_,
      /*hover=*/new_focused_control == FocusableControl::kThumbsDown);

  focused_control_ = new_focused_control;
}

BEGIN_METADATA(PopupRowAutofillAiFeedbackView)
END_METADATA

}  // namespace autofill_ai
