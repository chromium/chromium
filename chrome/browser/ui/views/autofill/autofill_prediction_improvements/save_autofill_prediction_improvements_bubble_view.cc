// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_prediction_improvements/save_autofill_prediction_improvements_bubble_view.h"

#include "chrome/browser/ui/autofill/autofill_prediction_improvements/save_autofill_prediction_improvements_controller.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"

namespace autofill {

namespace {

constexpr int kHeaderImageWidthAndHeight = 36;
constexpr int kBubbleWidth = 320;
constexpr gfx::Size kHeaderImageSize(kHeaderImageWidthAndHeight,
                                     kHeaderImageWidthAndHeight);

std::unique_ptr<views::View> BuildPredictedValueRow(
    const std::u16string key,
    const std::u16string value) {
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .AddChildren(
          views::Builder<views::Label>()
              .SetText(key)
              .SetTextStyle(views::style::STYLE_BODY_4)
              .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT),
          views::Builder<views::Label>()
              .SetText(value)
              .SetTextStyle(views::style::STYLE_BODY_4_BOLD)
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
      views::DialogContentType::kText, views::DialogContentType::kText));

  const int kVerficalSpacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTROL_LIST_VERTICAL);

  auto* improved_predicted_values_container =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::BoxLayout::Orientation::kVertical)
                       .SetBetweenChildSpacing(kVerficalSpacing)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                       .Build());

  for (const SaveAutofillPredictionImprovementsController::
           PredictionImprovement& prediction_improvement :
       controller_->GetPredictionImprovements()) {
    improved_predicted_values_container->AddChildView(BuildPredictedValueRow(
        prediction_improvement.key, prediction_improvement.value));
  }

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
          .SetInsideBorderInsets(
              ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
                  views::DialogContentType::kText,
                  views::DialogContentType::kText))
          .Build();
  // TODO(crbug.com/362227379): This image is currently hardcoded, align with UX
  // on the right one.
  auto image = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      ui::ImageModel::FromResourceId(IDR_SAVE_ADDRESS),
      ui::ImageModel::FromResourceId(IDR_SAVE_ADDRESS_DARK),
      base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                          base::Unretained(this)));
  image->SetImageSize(kHeaderImageSize);
  header_container->AddChildView(std::move(image));
  header_container->AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_TITLE))
          .SetTextStyle(views::style::STYLE_HEADLINE_4_BOLD)
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
