// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_ai/save_autofill_ai_data_bubble_view.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_ai/save_autofill_ai_data_controller.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/popup/autofill_ai/autofill_ai_icon_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"

namespace autofill_ai {

namespace {

// The padding between the header (image and title) and the elements around it.
constexpr int kHeaderPadding = 20;

constexpr int kBubbleWidth = 320;

std::unique_ptr<views::View> BuildEntityAttributeRow(
    std::string_view key,
    std::u16string_view value) {
  auto row =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
          .AddChildren(
              views::Builder<views::Label>()
                  .SetText(base::UTF8ToUTF16(key))
                  .SetTextStyle(views::style::STYLE_BODY_4)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT),
              views::Builder<views::Label>()
                  .SetText(std::u16string(value))
                  .SetTextStyle(views::style::STYLE_BODY_3_MEDIUM)
                  .SetHorizontalAlignment(
                      gfx::HorizontalAlignment::ALIGN_RIGHT))
          .Build();

  // Set every child to expand with the same ratio.
  for (auto child : row->children()) {
    row->SetFlexForView(child, 1);
  }
  return row;
}

SaveAutofillAiDataController::AutofillAiBubbleClosedReason
GetAutofillAiBubbleClosedReasonFromWidget(const views::Widget* widget) {
  DCHECK(widget);
  if (!widget->IsClosed()) {
    return SaveAutofillAiDataController::AutofillAiBubbleClosedReason::kUnknown;
  }

  switch (widget->closed_reason()) {
    case views::Widget::ClosedReason::kUnspecified:
      return SaveAutofillAiDataController::AutofillAiBubbleClosedReason::
          kNotInteracted;
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kCloseButtonClicked:
      return SaveAutofillAiDataController::AutofillAiBubbleClosedReason::
          kClosed;
    case views::Widget::ClosedReason::kLostFocus:
      return SaveAutofillAiDataController::AutofillAiBubbleClosedReason::
          kLostFocus;
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      return SaveAutofillAiDataController::AutofillAiBubbleClosedReason::
          kAccepted;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      return SaveAutofillAiDataController::AutofillAiBubbleClosedReason::
          kCancelled;
  }
}

}  // namespace

SaveAutofillAiDataBubbleView::SaveAutofillAiDataBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SaveAutofillAiDataController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller->GetWeakPtr()) {
  set_fixed_width(kBubbleWidth);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));
  SetAccessibleTitle(controller_->GetDialogTitle());

  const int kVerficalSpacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTROL_LIST_VERTICAL);

  auto* improved_predicted_values_container =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::BoxLayout::Orientation::kVertical)
                       .SetBetweenChildSpacing(kVerficalSpacing)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                       .Build());

  for (const autofill::AttributeInstance& attribute :
       controller_->GetAutofillAiData()->attributes()) {
    improved_predicted_values_container->AddChildView(BuildEntityAttributeRow(
        attribute.type().name_as_string(), attribute.value()));
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
      &SaveAutofillAiDataBubbleView::OnDialogAccepted, base::Unretained(this)));
  SetShowCloseButton(true);
}

SaveAutofillAiDataBubbleView::~SaveAutofillAiDataBubbleView() = default;

void SaveAutofillAiDataBubbleView::Hide() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetAutofillAiBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void SaveAutofillAiDataBubbleView::AddedToWidget() {
  const int kHorizontalSpacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);
  auto header_container =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBetweenChildSpacing(kHorizontalSpacing)
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          // The bottom padding has to be subtracted by the distance between the
          // information that will be saved, so to avoid double padding.
          .SetInsideBorderInsets(gfx::Insets::TLBR(
              kHeaderPadding, kHeaderPadding,
              std::min(0, kHeaderPadding -
                              ChromeLayoutProvider::Get()->GetDistanceMetric(
                                  DISTANCE_CONTROL_LIST_VERTICAL)),
              kHeaderPadding))
          .Build();
  header_container->AddChildView(
      autofill_ai::CreateLargeAutofillAiIconImageView());
  header_container->AddChildView(
      views::Builder<views::Label>()
          .SetText(controller_->GetDialogTitle())
          .SetTextStyle(views::style::STYLE_HEADLINE_4)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .Build());
  GetBubbleFrameView()->SetHeaderView(std::move(header_container));
}

void SaveAutofillAiDataBubbleView::WindowClosing() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetAutofillAiBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void SaveAutofillAiDataBubbleView::OnDialogAccepted() {
  if (controller_) {
    controller_->OnSaveButtonClicked();
  }
}

BEGIN_METADATA(SaveAutofillAiDataBubbleView)
END_METADATA
}  // namespace autofill_ai
