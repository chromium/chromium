// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_ai/save_or_update_autofill_ai_data_bubble_view.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace autofill_ai {

namespace {

// The padding between the header (image and title) and the elements around it.
constexpr int kHeaderPadding = 20;
constexpr int kBubbleWidth = 320;
constexpr int kNewOrUpdatedAttributeDotSize = 4;
constexpr int kNewOrUpdatedAttributeDotRightSpacing = 4;
constexpr int kNewOrUpdatedAttributeDotTopSpacing = 8;

int GetVerticaSpaceBetweenDialogSections() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
             DISTANCE_CONTROL_LIST_VERTICAL) *
         2;
}

gfx::Insets GetBubbleInnerMargins() {
  return ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl);
}

int GetEntityAttributeAndValueLabelMaxWidth() {
  // The maximum width is the bubble size minus its margin divided by two.
  // One half is for the entity attribute name and the other for the value.
  return (kBubbleWidth - GetBubbleInnerMargins().width() -
          ChromeLayoutProvider::Get()->GetDistanceMetric(
              DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL)) /
         2;
}

std::unique_ptr<views::BoxLayoutView> GetEntityAttributeAndValueLayout(
    views::BoxLayout::CrossAxisAlignment aligment) {
  auto row =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetCrossAxisAlignment(aligment)
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          // The minimum width is also set because we want to always reserve the
          // same size for both the attribute name and its value, meaning no
          // resizing/stretching.
          .SetMinimumCrossAxisSize(GetEntityAttributeAndValueLabelMaxWidth())
          .Build();
  return row;
}

SaveOrUpdateAutofillAiDataController::AutofillAiBubbleClosedReason
GetAutofillAiBubbleClosedReasonFromWidget(const views::Widget* widget) {
  DCHECK(widget);
  if (!widget->IsClosed()) {
    return SaveOrUpdateAutofillAiDataController::AutofillAiBubbleClosedReason::
        kUnknown;
  }

  switch (widget->closed_reason()) {
    case views::Widget::ClosedReason::kUnspecified:
      return SaveOrUpdateAutofillAiDataController::
          AutofillAiBubbleClosedReason::kNotInteracted;
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kCloseButtonClicked:
      return SaveOrUpdateAutofillAiDataController::
          AutofillAiBubbleClosedReason::kClosed;
    case views::Widget::ClosedReason::kLostFocus:
      return SaveOrUpdateAutofillAiDataController::
          AutofillAiBubbleClosedReason::kLostFocus;
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      return SaveOrUpdateAutofillAiDataController::
          AutofillAiBubbleClosedReason::kAccepted;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      return SaveOrUpdateAutofillAiDataController::
          AutofillAiBubbleClosedReason::kCancelled;
  }
}

}  // namespace

SaveOrUpdateAutofillAiDataBubbleView::SaveOrUpdateAutofillAiDataBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SaveOrUpdateAutofillAiDataController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller->GetWeakPtr()) {
  set_fixed_width(kBubbleWidth);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  set_margins(GetBubbleInnerMargins());
  SetAccessibleTitle(controller_->GetDialogTitle());

  auto* main_content_wrapper = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(GetVerticaSpaceBetweenDialogSections())
          .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
          .Build());

  const bool is_save_prompt = controller_->IsSavePrompt();
  if (is_save_prompt) {
    main_content_wrapper->AddChildView(
        views::Builder<views::Label>()
            .SetText(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_AI_SAVE_ENTITY_DIALOG_SUBTITLE))
            .SetTextStyle(views::style::STYLE_BODY_4)
            .SetAccessibleRole(ax::mojom::Role::kDetails)
            .SetMultiLine(true)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
            .Build());
  }

  auto* attributes_wrapper = main_content_wrapper->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_CONTROL_LIST_VERTICAL))
          .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
          .SetAccessibleRole(ax::mojom::Role::kDescriptionList)
          .Build());

  const std::vector<
      SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails>
      attributes_details = controller_->GetUpdatedAttributesDetails();
  for (const SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails&
           detail : attributes_details) {
    attributes_wrapper->AddChildView(BuildEntityAttributeRow(detail));
  }

  DialogDelegate::SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_NO_THANKS_BUTTON));
  DialogDelegate::SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON));
  SetAcceptCallback(
      base::BindOnce(&SaveOrUpdateAutofillAiDataBubbleView::OnDialogAccepted,
                     base::Unretained(this)));
  SetShowCloseButton(true);
}

SaveOrUpdateAutofillAiDataBubbleView::~SaveOrUpdateAutofillAiDataBubbleView() =
    default;

std::unique_ptr<views::View>
SaveOrUpdateAutofillAiDataBubbleView::GetAttributeValueView(
    const SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails&
        detail) {
  std::unique_ptr<views::BoxLayoutView> atribute_value_row_wrapper =
      GetEntityAttributeAndValueLayout(
          views::BoxLayout::CrossAxisAlignment::kEnd);
  std::unique_ptr<views::Label> label =
      views::Builder<views::Label>()
          .SetText(detail.attribute_value)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT)
          .SetTextStyle(views::style::STYLE_BODY_3_MEDIUM)
          .SetAccessibleRole(ax::mojom::Role::kDefinition)
          .SetMultiLine(true)
          .SetAllowCharacterBreak(true)
          .SetMaximumWidth(GetEntityAttributeAndValueLabelMaxWidth())
          .Build();
  attribute_values_observation_.AddObservation(label.get());

  // Only update dialogs have a dot circle in front of added or updated values.
  const bool existing_entity_added_or_updated_attribute =
      !controller_->IsSavePrompt() &&
      detail.update_type !=
          SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateType::
              kNewEntityAttributeUnchanged;
  if (!existing_entity_added_or_updated_attribute) {
    atribute_value_row_wrapper->AddChildView(std::move(label));
    return atribute_value_row_wrapper;
  }

  views::View* updated_entity_dot_and_value_wrapper =
      atribute_value_row_wrapper->AddChildView(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kStart)
              .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
              .Build());
  views::BoxLayoutView* updated_entity_dot_wrapper =
      updated_entity_dot_and_value_wrapper->AddChildView(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kCenter)
              .Build());

  views::BoxLayoutView* updated_entity_dot =
      updated_entity_dot_wrapper->AddChildView(
          views::Builder<views::BoxLayoutView>()
              .SetProperty(
                  views::kMarginsKey,
                  gfx::Insets::TLBR(kNewOrUpdatedAttributeDotTopSpacing, 0, 0,
                                    kNewOrUpdatedAttributeDotRightSpacing))
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kCenter)
              .SetMainAxisAlignment(
                  views::BoxLayout::MainAxisAlignment::kCenter)
              .Build());
  updated_entity_dot->SetPreferredSize(
      gfx::Size(kNewOrUpdatedAttributeDotSize, kNewOrUpdatedAttributeDotSize));
  updated_entity_dot->SizeToPreferredSize();
  updated_entity_dot->SetBackground(views::CreateRoundedRectBackground(
      ui::kColorButtonBackgroundProminent, kNewOrUpdatedAttributeDotSize / 2));
  label->GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
      detail.update_type ==
              SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateType::
                  kNewEntityAttributeAdded
          ? IDS_AUTOFILL_AI_UPDATE_ENTITY_DIALOG_NEW_ATTRIBUTE_ACCESSIBLE_NAME
          : IDS_AUTOFILL_AI_UPDATE_ENTITY_DIALOG_UPDATED_ATTRIBUTE_ACCESSIBLE_NAME,
      detail.attribute_value));
  updated_entity_dot_and_value_wrapper->AddChildView(std::move(label));

  return atribute_value_row_wrapper;
}

std::unique_ptr<views::View>
SaveOrUpdateAutofillAiDataBubbleView::BuildEntityAttributeRow(
    const SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails&
        detail) {
  auto row = views::Builder<views::BoxLayoutView>()
                 .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                 .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
                 .Build();

  views::BoxLayoutView* entity_attribute_wrapper =
      row->AddChildView(GetEntityAttributeAndValueLayout(
          views::BoxLayout::CrossAxisAlignment::kStart));
  entity_attribute_wrapper->AddChildView(
      views::Builder<views::Label>()
          .SetText(detail.attribute_name)
          .SetTextStyle(views::style::STYLE_BODY_4)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetAccessibleRole(ax::mojom::Role::kTerm)
          .SetElideBehavior(gfx::ELIDE_TAIL)
          .SetMaximumWidthSingleLine(GetEntityAttributeAndValueLabelMaxWidth())
          .Build());
  row->AddChildView(GetAttributeValueView(detail));
  // Set every child to expand with the same ratio.
  for (auto child : row->children()) {
    row->SetFlexForView(child, 1);
  }
  return row;
}

void SaveOrUpdateAutofillAiDataBubbleView::Hide() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetAutofillAiBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void SaveOrUpdateAutofillAiDataBubbleView::OnViewBoundsChanged(
    views::View* observed_view) {
  views::Label* attribute_value_view =
      views::AsViewClass<views::Label>(observed_view);
  if (!attribute_value_view) {
    return;
  }

  // If the value leads to more than a single line, align the text to the left.
  if (attribute_value_view->GetRequiredLines() > 1) {
    attribute_value_view->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
  }
}

void SaveOrUpdateAutofillAiDataBubbleView::AddedToWidget() {
  auto header_container =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(GetVerticaSpaceBetweenDialogSections())
          // The bottom padding has to be subtracted by the distance between the
          // information that will be saved, so to avoid double padding.
          .SetInsideBorderInsets(gfx::Insets::TLBR(
              kHeaderPadding, kHeaderPadding,
              std::min(0, kHeaderPadding -
                              ChromeLayoutProvider::Get()->GetDistanceMetric(
                                  DISTANCE_CONTROL_LIST_VERTICAL)),
              kHeaderPadding))
          .Build();
  if (controller_->IsSavePrompt()) {
    std::pair<int, int> images = controller_->GetTitleImagesResourceId();
    header_container->AddChildView(
        std::make_unique<ThemeTrackingNonAccessibleImageView>(
            ui::ImageModel::FromResourceId(images.first),
            ui::ImageModel::FromResourceId(images.second),
            base::BindRepeating(&views::BubbleDialogDelegate::background_color,
                                base::Unretained(this))));
  }
  header_container->AddChildView(
      views::Builder<views::Label>()
          .SetText(controller_->GetDialogTitle())
          .SetTextStyle(views::style::STYLE_HEADLINE_4)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetAccessibleRole(ax::mojom::Role::kHeading)
          .Build());
  GetBubbleFrameView()->SetHeaderView(std::move(header_container));
}

void SaveOrUpdateAutofillAiDataBubbleView::WindowClosing() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetAutofillAiBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void SaveOrUpdateAutofillAiDataBubbleView::OnDialogAccepted() {
  if (controller_) {
    controller_->OnSaveButtonClicked();
  }
}

BEGIN_METADATA(SaveOrUpdateAutofillAiDataBubbleView)
END_METADATA
}  // namespace autofill_ai
