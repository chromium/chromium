// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_prediction_improvements/save_autofill_prediction_improvements_bubble_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_prediction_improvements/save_autofill_prediction_improvements_controller.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"

namespace autofill {

namespace {

// TODO(crbug.com/362227996): Icons related to the feedback view is repeating in
// the suggestion specific class. Consolidate them into a single file. The size
// of the icons used in the buttons.
constexpr int kIconSize = 16;

// The button radius used to paint the background.
constexpr int kButtonRadius = 12;

// The padding between the header (image and title) and the elements around it.
constexpr int kHeaderPadding = 20;

constexpr int kHeaderImageWidthAndHeight = 36;
constexpr int kBubbleWidth = 320;
constexpr gfx::Size kHeaderImageSize(kHeaderImageWidthAndHeight,
                                     kHeaderImageWidthAndHeight);

std::unique_ptr<views::View> BuildPredictedValueRow(const std::string key,
                                                    const std::string value) {
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .AddChildren(
          views::Builder<views::Label>()
              .SetText(base::UTF8ToUTF16(value))
              .SetTextStyle(views::style::STYLE_BODY_3_MEDIUM)
              .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT),
          views::Builder<views::Label>()
              .SetText(base::UTF8ToUTF16(key))
              .SetTextStyle(views::style::STYLE_BODY_5)
              .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT))
      .Build();
}

SaveAutofillPredictionImprovementsController::
    PredictionImprovementsBubbleClosedReason
    GetPredictionImprovementsBubbleClosedReasonFromWidget(
        const views::Widget* widget) {
  DCHECK(widget);
  if (!widget->IsClosed()) {
    return SaveAutofillPredictionImprovementsController::
        PredictionImprovementsBubbleClosedReason::kUnknown;
  }

  switch (widget->closed_reason()) {
    case views::Widget::ClosedReason::kUnspecified:
      return SaveAutofillPredictionImprovementsController::
          PredictionImprovementsBubbleClosedReason::kNotInteracted;
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kCloseButtonClicked:
      return SaveAutofillPredictionImprovementsController::
          PredictionImprovementsBubbleClosedReason::kClosed;
    case views::Widget::ClosedReason::kLostFocus:
      return SaveAutofillPredictionImprovementsController::
          PredictionImprovementsBubbleClosedReason::kLostFocus;
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      return SaveAutofillPredictionImprovementsController::
          PredictionImprovementsBubbleClosedReason::kAccepted;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      return SaveAutofillPredictionImprovementsController::
          PredictionImprovementsBubbleClosedReason::kCancelled;
  }
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
  // This is used in tests only.
  button->SetID(
      is_thumbs_up
          ? SaveAutofillPredictionImprovementsBubbleView::kThumbsUpButtonViewID
          : SaveAutofillPredictionImprovementsBubbleView::
                kThumbsDownButtonViewID);
  return button;
}

std::unique_ptr<views::View> CreateFooterView(
    base::WeakPtr<SaveAutofillPredictionImprovementsController> controller) {
  auto footer_container =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_CONTROL_LIST_VERTICAL))
          .Build();
  footer_container->AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_FOOTER_DETAILS))
          .SetMultiLine(true)
          .SetTextStyle(views::style::STYLE_BODY_5)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .Build());

  auto* feedback_container = footer_container->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .Build());

  views::StyledLabel::RangeStyleInfo style_info =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &SaveAutofillPredictionImprovementsController::OnLearnMoreClicked,
          controller));
  style_info.text_style = views::style::TextStyle::STYLE_LINK_5;

  std::vector<size_t> replacement_offsets;
  const std::u16string manage_prediction_improvements_link_text =
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGE_PREDICTION_IMPROVEMENTS);
  const std::u16string formatted_text = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_TEXT,
      /*replacements=*/{manage_prediction_improvements_link_text},
      &replacement_offsets);

  // Create the feedback container with its text and buttons.
  feedback_container->SetFlexForView(
      feedback_container->AddChildView(
          views::Builder<views::StyledLabel>()
              .SetText(formatted_text)
              .SetDefaultTextStyle(views::style::STYLE_BODY_5)
              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
              .AddStyleRange(
                  gfx::Range(
                      replacement_offsets[0],
                      replacement_offsets[0] +
                          manage_prediction_improvements_link_text.length()),
                  style_info)
              // This is used in tests only.
              .SetID(SaveAutofillPredictionImprovementsBubbleView::
                         kLearnMoreStyledLabelViewID)
              .Build()),
      1);

  auto buttons_wrapper = views::Builder<views::BoxLayoutView>()
                             .SetBetweenChildSpacing(
                                 ChromeLayoutProvider::Get()->GetDistanceMetric(
                                     DISTANCE_RELATED_LABEL_HORIZONTAL_LIST))
                             .Build();
  buttons_wrapper->AddChildView(CreateFeedbackButton(
      vector_icons::kThumbUpIcon,
      base::BindRepeating(
          &SaveAutofillPredictionImprovementsController::OnThumbsUpClicked,
          controller)));
  buttons_wrapper->AddChildView(CreateFeedbackButton(
      vector_icons::kThumbDownIcon,
      base::BindRepeating(
          &SaveAutofillPredictionImprovementsController::OnThumbsDownClicked,
          controller)));
  feedback_container->AddChildView(std::move(buttons_wrapper));

  return footer_container;
}
}  // namespace

SaveAutofillPredictionImprovementsBubbleView::
    SaveAutofillPredictionImprovementsBubbleView(
        views::View* anchor_view,
        content::WebContents* web_contents,
        SaveAutofillPredictionImprovementsController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller->GetWeakPtr()) {
  set_fixed_width(kBubbleWidth);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));

  const int kVerficalSpacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTROL_LIST_VERTICAL);

  auto* improved_predicted_values_container =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::BoxLayout::Orientation::kVertical)
                       .SetBetweenChildSpacing(kVerficalSpacing)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                       .Build());

  for (const optimization_guide::proto::UserAnnotationsEntry&
           prediction_improvement : controller_->GetPredictionImprovements()) {
    improved_predicted_values_container->AddChildView(BuildPredictedValueRow(
        prediction_improvement.key(), prediction_improvement.value()));
  }

  SetFootnoteView(CreateFooterView(controller_));

  DialogDelegate::SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_NO_THANKS_BUTTON));
  DialogDelegate::SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON));
  SetAcceptCallback(base::BindOnce(
      &SaveAutofillPredictionImprovementsBubbleView::OnDialogAccepted,
      base::Unretained(this)));
  SetShowCloseButton(true);
}

SaveAutofillPredictionImprovementsBubbleView::
    ~SaveAutofillPredictionImprovementsBubbleView() = default;

void SaveAutofillPredictionImprovementsBubbleView::Hide() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPredictionImprovementsBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void SaveAutofillPredictionImprovementsBubbleView::AddedToWidget() {
  const int kHorizontalSpacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);
  auto header_container =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBetweenChildSpacing(kHorizontalSpacing)
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
          // The bottom padding has to be subtracted by the distance between the
          // information that will be saved, so to avoid double padding.
          .SetInsideBorderInsets(gfx::Insets::TLBR(
              kHeaderPadding, kHeaderPadding,
              std::min(0, kHeaderPadding -
                              ChromeLayoutProvider::Get()->GetDistanceMetric(
                                  DISTANCE_CONTROL_LIST_VERTICAL)),
              kHeaderPadding))
          .Build();
  auto image = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      ui::ImageModel::FromResourceId(
          IDR_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_LOGO),
      ui::ImageModel::FromResourceId(
          IDR_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_LOGO_DARK),
      base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                          base::Unretained(this)));
  image->SetImageSize(kHeaderImageSize);
  header_container->AddChildView(std::move(image));
  header_container->AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_TITLE))
          .SetTextStyle(views::style::STYLE_HEADLINE_4)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .Build());
  GetBubbleFrameView()->SetHeaderView(std::move(header_container));
}

void SaveAutofillPredictionImprovementsBubbleView::WindowClosing() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPredictionImprovementsBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void SaveAutofillPredictionImprovementsBubbleView::OnDialogAccepted() {
  if (controller_) {
    controller_->OnSaveButtonClicked();
  }
}

BEGIN_METADATA(SaveAutofillPredictionImprovementsBubbleView)
END_METADATA
}  // namespace autofill
