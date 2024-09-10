// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_prediction_improvements/save_autofill_prediction_improvements_bubble_view.h"

#include "chrome/browser/ui/autofill/autofill_prediction_improvements/save_autofill_prediction_improvements_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"

namespace autofill {

namespace {
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
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  auto description = std::make_unique<views::Label>(
      u"Description", views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  AddChildView(std::move(description));
}

void SaveAutofillPredictionImprovementsBubbleView::Hide() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPredictionImprovementsBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void SaveAutofillPredictionImprovementsBubbleView::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<views::Label>(u"Prediction improvements"));
}

void SaveAutofillPredictionImprovementsBubbleView::WindowClosing() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPredictionImprovementsBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

SaveAutofillPredictionImprovementsBubbleView::
    ~SaveAutofillPredictionImprovementsBubbleView() = default;

BEGIN_METADATA(SaveAutofillPredictionImprovementsBubbleView)
END_METADATA
}  // namespace autofill
