// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_ai/save_or_update_autofill_ai_data_bubble_view.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/popup/autofill_ai/autofill_ai_icon_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/strings/grit/components_strings.h"
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
constexpr int kNewOrUpdatedAttributeDotSpacing = 4;

std::unique_ptr<views::View> GetAttributeValueView(
    const SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails&
        detail,
    bool is_save_prompt) {
  std::unique_ptr<views::Label> label =
      std::make_unique<views::Label>(detail.attribute_value);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT);
  label->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);

  // Only update dialogs have a dot circle in front of added or updated values.
  const bool existing_entity_added_or_updated_attribute =
      !is_save_prompt &&
      detail.update_type !=
          SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateType::
              kNewEntityAttributeUnchanged;
  if (existing_entity_added_or_updated_attribute) {
    auto row = views::Builder<views::BoxLayoutView>()
                   .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                   .SetCrossAxisAlignment(
                       views::BoxLayout::CrossAxisAlignment::kCenter)
                   .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
                   .Build();

    views::BoxLayoutView* upated_entity_dot = row->AddChildView(
        views::Builder<views::BoxLayoutView>()
            .SetProperty(
                views::kMarginsKey,
                gfx::Insets::TLBR(0, 0, 0, kNewOrUpdatedAttributeDotSpacing))
            .Build());
    upated_entity_dot->SetPreferredSize(gfx::Size(
        kNewOrUpdatedAttributeDotSize, kNewOrUpdatedAttributeDotSize));
    upated_entity_dot->SizeToPreferredSize();
    upated_entity_dot->SetBackground(
        views::CreateRoundedRectBackground(ui::kColorButtonBackgroundProminent,
                                           kNewOrUpdatedAttributeDotSize / 2));
    row->AddChildView(std::move(label));
    row->SetAccessibleRole(ax::mojom::Role::kTerm);
    row->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
    row->GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
        detail.update_type ==
                SaveOrUpdateAutofillAiDataController::
                    EntityAttributeUpdateType::kNewEntityAttributeAdded
            ? IDS_AUTOFILL_AI_UPDATE_ENTITY_DIALOG_NEW_ATTRIBUTE_ACCESSIBLE_NAME
            : IDS_AUTOFILL_AI_UPDATE_ENTITY_DIALOG_UPDATED_ATTRIBUTE_ACCESSIBLE_NAME,
        detail.attribute_value));
    return row;
  } else {
    label->SetAccessibleRole(ax::mojom::Role::kTerm);
    label->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
    return label;
  }
}

// Helper to create a row displayed in the dialog. This row contains information
// about the entity to be saved or updated.
std::unique_ptr<views::View> BuildEntityAttributeRow(
    const SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails&
        detail,
    bool is_save_prompt) {
  auto row = views::Builder<views::BoxLayoutView>()
                 .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                 .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
                 .Build();
  row->AddChildView(
      views::Builder<views::Label>()
          .SetText(detail.attribute_name)
          .SetTextStyle(views::style::STYLE_BODY_4)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetAccessibleRole(ax::mojom::Role::kDefinition)
          .SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY)
          .Build());
  row->AddChildView(GetAttributeValueView(detail, is_save_prompt));
  // Set every child to expand with the same ratio.
  for (auto child : row->children()) {
    row->SetFlexForView(child, 1);
  }
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
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));
  SetAccessibleTitle(controller_->GetDialogTitle());

  const int kVerticalSpacingBetweenAttributes =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_CONTROL_LIST_VERTICAL);

  auto* main_content_wrapper = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(kVerticalSpacingBetweenAttributes * 2)
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
            .SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY)
            .Build());
  }

  auto* attributes_wrapper = main_content_wrapper->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetAccessibleRole(ax::mojom::Role::kDescriptionList)
          .SetBetweenChildSpacing(kVerticalSpacingBetweenAttributes * 2)
          .Build());
  auto* new_entity_added_or_updated_attributes_container =
      attributes_wrapper->AddChildView(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .SetBetweenChildSpacing(kVerticalSpacingBetweenAttributes)
              .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
              .Build());
  new_entity_added_or_updated_attributes_container->SetID(
      kNewEntityAddedOrUpdatedAttributesContainer);

  // Only present in the update case.
  views::View* new_entity_unchanged_attributes_container = nullptr;
  if (!is_save_prompt) {
    new_entity_unchanged_attributes_container =
        attributes_wrapper->AddChildView(
            views::Builder<views::BoxLayoutView>()
                .SetOrientation(views::BoxLayout::Orientation::kVertical)
                .SetBetweenChildSpacing(kVerticalSpacingBetweenAttributes)
                .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                .Build());
    new_entity_unchanged_attributes_container->SetID(
        kNewEntityUnchagedAttributesContainer);
  }

  const std::vector<
      SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails>
      attributes_details = controller_->GetUpdatedAttributesDetails();
  for (const SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateDetails&
           detail : attributes_details) {
    const bool new_entity_added_or_updated_attibute =
        (detail.update_type ==
         SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateType::
             kNewEntityAttributeUpdated) ||
        (detail.update_type ==
         SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateType::
             kNewEntityAttributeAdded);
    if (new_entity_added_or_updated_attibute) {
      new_entity_added_or_updated_attributes_container->AddChildView(
          BuildEntityAttributeRow(detail, is_save_prompt));
    } else {
      CHECK(!is_save_prompt);
      new_entity_unchanged_attributes_container->AddChildView(
          BuildEntityAttributeRow(detail, is_save_prompt));
    }
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

void SaveOrUpdateAutofillAiDataBubbleView::Hide() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetAutofillAiBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void SaveOrUpdateAutofillAiDataBubbleView::AddedToWidget() {
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
          .SetAccessibleRole(ax::mojom::Role::kHeading)
          .SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY)
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
