// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_ai/save_or_update_autofill_ai_data_bubble_view.h"

#include <string>
#include <string_view>

#include "base/strings/strcat.h"
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
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_elider.h"
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

constexpr int kBubbleWidth = 320;
constexpr int kSubTitleBottomMargin = 16;
constexpr std::u16string_view kNewValueDot = u"•";

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
  SetTitle(controller_->GetDialogTitle());

  auto* main_content_wrapper =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::BoxLayout::Orientation::kVertical)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                       .Build());
  if (controller_->IsSavePrompt()) {
    auto subtitle_container =
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .SetInsideBorderInsets(
                gfx::Insets::TLBR(0, 0, kSubTitleBottomMargin, 0))
            .Build();
    subtitle_container->AddChildView(
        views::Builder<views::Label>()
            .SetText(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_AI_SAVE_ENTITY_DIALOG_SUBTITLE))
            .SetTextStyle(views::style::STYLE_BODY_4)
            .SetEnabledColor(ui::kColorSysOnSurfaceSubtle)
            .SetAccessibleRole(ax::mojom::Role::kDetails)
            .SetMultiLine(true)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
            .Build());
    main_content_wrapper->AddChildView(std::move(subtitle_container));
  }

  auto* attributes_wrapper = main_content_wrapper->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_CONTENT_LIST_VERTICAL_SINGLE))
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
          controller_->IsSavePrompt()
              ? IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON
              : IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_UPDATE_DIALOG_UPDATE_BUTTON));
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
  const bool existing_entity_added_or_updated_attribute =
      !controller_->IsSavePrompt() &&
      detail.update_type !=
          SaveOrUpdateAutofillAiDataController::EntityAttributeUpdateType::
              kNewEntityAttributeUnchanged;
  const bool should_value_have_medium_weight =
      controller_->IsSavePrompt() || existing_entity_added_or_updated_attribute;
  std::unique_ptr<views::BoxLayoutView> attribute_value_row_wrapper =
      GetEntityAttributeAndValueLayout(
          views::BoxLayout::CrossAxisAlignment::kEnd);
  std::unique_ptr<views::Label> label =
      views::Builder<views::Label>()
          .SetText(detail.attribute_value)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT)
          .SetTextStyle(should_value_have_medium_weight
                            ? views::style::STYLE_BODY_4_MEDIUM
                            : views::style::STYLE_BODY_4)
          .SetAccessibleRole(ax::mojom::Role::kDefinition)
          .SetMultiLine(true)
          .SetEnabledColor(ui::kColorSysOnSurface)
          .SetAllowCharacterBreak(true)
          .SetMaximumWidth(GetEntityAttributeAndValueLabelMaxWidth())
          .Build();

  // Only update dialogs have a dot circle in front of added or updated values.
  if (!existing_entity_added_or_updated_attribute) {
    label->SetText(detail.attribute_value);
    attribute_value_row_wrapper->AddChildView(std::move(label));
    return attribute_value_row_wrapper;
  }
  // In order to properly add a blue dot, it is necessary to have 3 labels.
  // 1. A blue label for the dot itself.
  // 2. A horizontally aligned label with the first line of the updated value.
  // 3. Optionally a third label with the remaining value.
  views::View* updated_entity_dot_and_value_wrapper =
      attribute_value_row_wrapper->AddChildView(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kStart)
              .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
              .Build());
  views::Label* blue_dot = updated_entity_dot_and_value_wrapper->AddChildView(
      views::Builder<views::Label>()
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT)
          .SetTextStyle(views::style::STYLE_BODY_4_MEDIUM)
          .SetEnabledColor(ui::kColorButtonBackgroundProminent)
          .SetText(base::StrCat({kNewValueDot, u" "}))
          .Build());

  // Reset the label style to handle the first line.
  label->SetMultiLine(false);
  label->SetAllowCharacterBreak(false);
  label->SetMaximumWidthSingleLine(GetEntityAttributeAndValueLabelMaxWidth() -
                                   blue_dot->GetPreferredSize().width());

  std::vector<std::u16string> substrings;
  gfx::ElideRectangleText(detail.attribute_value, label->font_list(),
                          GetEntityAttributeAndValueLabelMaxWidth() -
                              blue_dot->GetPreferredSize().width(),
                          label->GetLineHeight(), gfx::WRAP_LONG_WORDS,
                          &substrings);
  // At least one string should always exist.
  CHECK(!substrings.empty());
  const std::u16string& first_line = substrings[0];
  label->SetText(first_line);

  updated_entity_dot_and_value_wrapper->AddChildView(std::move(label));
  // One line was not enough.
  if (first_line != detail.attribute_value) {
    std::u16string remaining_lines =
        detail.attribute_value.substr(first_line.size());
    base::TrimWhitespace(std::move(remaining_lines), base::TRIM_ALL,
                         &remaining_lines);
    attribute_value_row_wrapper->AddChildView(
        views::Builder<views::Label>()
            .SetText(remaining_lines)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT)
            .SetTextStyle(should_value_have_medium_weight
                              ? views::style::STYLE_BODY_4_MEDIUM
                              : views::style::STYLE_BODY_4)
            .SetAccessibleRole(ax::mojom::Role::kDefinition)
            .SetMultiLine(true)
            .SetEnabledColor(ui::kColorSysOnSurface)
            .SetAllowCharacterBreak(true)
            .SetMaximumWidth(GetEntityAttributeAndValueLabelMaxWidth())
            .Build());
  }
  attribute_value_row_wrapper->SetAccessibleRole(ax::mojom::Role::kDefinition);
  attribute_value_row_wrapper->GetViewAccessibility().SetName(
      l10n_util::GetStringFUTF16(
          detail.update_type ==
                  SaveOrUpdateAutofillAiDataController::
                      EntityAttributeUpdateType::kNewEntityAttributeAdded
              ? IDS_AUTOFILL_AI_UPDATE_ENTITY_DIALOG_NEW_ATTRIBUTE_ACCESSIBLE_NAME
              : IDS_AUTOFILL_AI_UPDATE_ENTITY_DIALOG_UPDATED_ATTRIBUTE_ACCESSIBLE_NAME,
          detail.attribute_value));
  return attribute_value_row_wrapper;
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
          .SetEnabledColor(ui::kColorSysOnSurfaceSubtle)
          .SetTextStyle(views::style::STYLE_BODY_4)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetAccessibleRole(ax::mojom::Role::kTerm)
          .SetElideBehavior(gfx::ELIDE_TAIL)
          .SetMaximumWidthSingleLine(GetEntityAttributeAndValueLabelMaxWidth())
          .Build());
  row->AddChildView(GetAttributeValueView(detail));
  // Set every child to expand with the same ratio.
  for (auto child : row->children()) {
    row->SetFlexForView(child.get(), 1);
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

void SaveOrUpdateAutofillAiDataBubbleView::AddedToWidget() {
  if (controller_->IsSavePrompt()) {
    std::pair<int, int> images = controller_->GetTitleImagesResourceId();
    GetBubbleFrameView()->SetHeaderView(
        std::make_unique<ThemeTrackingNonAccessibleImageView>(
            ui::ImageModel::FromResourceId(images.first),
            ui::ImageModel::FromResourceId(images.second),
            base::BindRepeating(&views::BubbleDialogDelegate::background_color,
                                base::Unretained(this))));
  }
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
