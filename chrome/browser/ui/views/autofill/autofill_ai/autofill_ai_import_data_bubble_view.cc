// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_ai/autofill_ai_import_data_bubble_view.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_data_controller.h"
#include "chrome/browser/ui/autofill/autofill_ai/entity_attribute_update_details.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/autofill_ai/autofill_ai_bubble_utils.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model_utils.h"
#include "ui/base/resource/resource_bundle.h"
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
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

namespace {

AutofillClient::AutofillAiBubbleResult GetAutofillAiBubbleResultFromWidget(
    const views::Widget* widget) {
  DCHECK(widget);
  if (!widget->IsClosed()) {
    return AutofillClient::AutofillAiBubbleResult::kUnknown;
  }

  switch (widget->closed_reason()) {
    case views::Widget::ClosedReason::kUnspecified:
      return AutofillClient::AutofillAiBubbleResult::kNotInteracted;
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kCloseButtonClicked:
      return AutofillClient::AutofillAiBubbleResult::kClosed;
    case views::Widget::ClosedReason::kLostFocus:
      return AutofillClient::AutofillAiBubbleResult::kLostFocus;
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      return AutofillClient::AutofillAiBubbleResult::kAccepted;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      return AutofillClient::AutofillAiBubbleResult::kCancelled;
  }
}

}  // namespace

AutofillAiImportDataBubbleView::AutofillAiImportDataBubbleView(
    views::BubbleAnchor anchor_view,
    content::WebContents* web_contents,
    AutofillAiImportDataController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller->GetWeakPtr()) {
  set_fixed_width(kAutofillAiBubbleWidth);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  set_margins(GetAutofillAiBubbleInnerMargins());
  SetAccessibleTitle(controller_->GetSaveUpdateDialogTitle());
  if (!controller_->IsWalletableEntity()) {
    SetTitle(controller_->GetSaveUpdateDialogTitle());
  }
  auto* main_content_wrapper =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::BoxLayout::Orientation::kVertical)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                       .Build());
  std::unique_ptr<views::BoxLayoutView> subtitle_container =
      CreateAutofillAiBubbleSubtitleContainer();
  if (controller_->IsWalletableEntity()) {
    subtitle_container->AddChildView(GetWalletableEntitySubtitle());
  } else {
    subtitle_container->AddChildView(GetLocalEntitySubtitle());
  }
  main_content_wrapper->AddChildView(std::move(subtitle_container));

  auto* attributes_wrapper = main_content_wrapper->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_CONTENT_LIST_VERTICAL_SINGLE))
          .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
          .SetAccessibleRole(ax::mojom::Role::kDescriptionList)
          .Build());

  const std::vector<EntityAttributeUpdateDetails> attributes_details =
      controller_->GetUpdatedAttributesDetails();
  for (const EntityAttributeUpdateDetails& detail : attributes_details) {
    attributes_wrapper->AddChildView(BuildEntityAttributeRow(detail));
  }

  DialogDelegate::SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_NO_THANKS_BUTTON));
  DialogDelegate::SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      controller_->GetSaveUpdateDialogPrimaryButtonText());
  SetAcceptCallbackWithClose(
      base::BindRepeating(&AutofillAiImportDataBubbleView::OnDialogAccepted,
                          base::Unretained(this)));
  SetShowCloseButton(true);

  loading_progress_row_ = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd)
          .SetVisible(false)
          .SetInsideBorderInsets(gfx::Insets::TLBR(40, 0, 0, 40))
          .AddChildren(views::Builder<views::Throbber>().CopyAddressTo(
              &loading_throbber_))
          .Build());
  loading_throbber_->SetID(DialogViewId::LOADING_THROBBER);
}

AutofillAiImportDataBubbleView::~AutofillAiImportDataBubbleView() = default;

std::unique_ptr<views::View>
AutofillAiImportDataBubbleView::BuildEntityAttributeRow(
    const EntityAttributeUpdateDetails& detail) {
  const bool existing_entity_added_or_updated_attribute =
      !controller_->IsSavePrompt() &&
      detail.update_type() !=
          EntityAttributeUpdateType::kNewEntityAttributeUnchanged;

  std::optional<std::u16string> accessibility_value;
  if (existing_entity_added_or_updated_attribute) {
    if (base::FeatureList::IsEnabled(features::kAutofillAiNewUpdatePrompt)) {
      if (detail.update_type() ==
          EntityAttributeUpdateType::kNewEntityAttributeAdded) {
        accessibility_value = l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_AI_UPDATE_ENTITY_DIALOG_NEW_ATTRIBUTE_ACCESSIBLE_NAME_V2,
            detail.attribute_value());
      } else {
        accessibility_value = l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_AI_UPDATE_ENTITY_DIALOG_UPDATED_ATTRIBUTE_ACCESSIBLE_NAME_V2,
            detail.attribute_value(),
            detail.old_attribute_value().value_or(u""));
      }
    } else {
      accessibility_value = l10n_util::GetStringFUTF16(
          detail.update_type() ==
                  EntityAttributeUpdateType::kNewEntityAttributeAdded
              ? IDS_AUTOFILL_AI_UPDATE_ENTITY_DIALOG_NEW_ATTRIBUTE_ACCESSIBLE_NAME
              : IDS_AUTOFILL_AI_UPDATE_ENTITY_DIALOG_UPDATED_ATTRIBUTE_ACCESSIBLE_NAME,
          detail.attribute_value());
    }
  }

  const int new_value_font_style = [&]() {
    if (!base::FeatureList::IsEnabled(features::kAutofillAiNewUpdatePrompt)) {
      return (controller_->IsSavePrompt() ||
              existing_entity_added_or_updated_attribute)
                 ? views::style::STYLE_BODY_4_MEDIUM
                 : views::style::STYLE_BODY_4;
    }
    if (controller_->IsSavePrompt()) {
      return views::style::STYLE_BODY_4_MEDIUM;
    }
    return detail.update_type() ==
                   EntityAttributeUpdateType::kNewEntityAttributeUnchanged
               ? views::style::STYLE_BODY_4
               : views::style::STYLE_BODY_4_BOLD;
  }();

  const bool with_blue_dot =
      !base::FeatureList::IsEnabled(features::kAutofillAiNewUpdatePrompt) &&
      existing_entity_added_or_updated_attribute;

  return CreateAutofillAiBubbleAttributeRow(
      detail.attribute_name(), detail.attribute_value(),
      detail.old_attribute_value(), accessibility_value, new_value_font_style,
      with_blue_dot);
}

std::unique_ptr<views::Label>
AutofillAiImportDataBubbleView::GetLocalEntitySubtitle() const {
  std::u16string subtitle_text = l10n_util::GetStringUTF16(
      controller_->IsSavePrompt()
          ? IDS_AUTOFILL_AI_SAVE_ENTITY_DIALOG_SUBTITLE
          : IDS_AUTOFILL_AI_UPDATE_ENTITY_DIALOG_SUBTITLE);
  return views::Builder<views::Label>()
      .SetText(std::move(subtitle_text))
      .SetTextStyle(views::style::STYLE_BODY_4)
      .SetEnabledColor(ui::kColorSysOnSurfaceSubtle)
      .SetAccessibleRole(ax::mojom::Role::kDetails)
      .SetMultiLine(true)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .Build();
}

std::unique_ptr<views::StyledLabel>
AutofillAiImportDataBubbleView::GetWalletableEntitySubtitle() const {
  std::vector<size_t> offsets;
  std::u16string formatted_text;
  gfx::Range link_range;

  const std::u16string google_wallet_text =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_WALLET_TITLE);
  const std::u16string account_email = controller_->GetPrimaryAccountEmail();

  if (controller_->IsSavePrompt() &&
      base::FeatureList::IsEnabled(features::kAutofillAiWalletPrivatePasses)) {
    const std::u16string manage_info_text =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_YOUR_INFO_LINK);

    formatted_text = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_DIALOG_SUBTITLE_NEW,
        {manage_info_text, google_wallet_text, account_email}, &offsets);

    link_range = gfx::Range(offsets[0], offsets[0] + manage_info_text.size());

  } else {
    formatted_text = l10n_util::GetStringFUTF16(
        controller_->IsSavePrompt()
            ? IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_DIALOG_SUBTITLE
            : IDS_AUTOFILL_AI_UPDATE_ENTITY_TO_WALLET_DIALOG_SUBTITLE,
        {google_wallet_text, account_email}, &offsets);

    link_range = gfx::Range(offsets[0], offsets[0] + google_wallet_text.size());
  }

  auto go_to_wallet =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &AutofillAiImportDataController::OnGoToWalletLinkClicked,
          controller_));

  return views::Builder<views::StyledLabel>()
      .SetText(std::move(formatted_text))
      .SetDefaultTextStyle(views::style::STYLE_BODY_4)
      .SetDefaultEnabledColorId(ui::kColorSysOnSurfaceSubtle)
      .SetAccessibleRole(ax::mojom::Role::kDetails)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .AddStyleRange(link_range, go_to_wallet)
      .Build();
}

void AutofillAiImportDataBubbleView::Hide() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetAutofillAiBubbleResultFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void AutofillAiImportDataBubbleView::AddedToWidget() {
  if (controller_->IsSavePrompt()) {
    int image = controller_->GetSaveUpdateDialogTitleImagesResourceId();
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

    std::unique_ptr<views::ImageView> image_view =
        std::make_unique<views::ImageView>(
            bundle.GetThemedLottieImageNamed(image));
    image_view->GetViewAccessibility().SetIsInvisible(true);

    GetBubbleFrameView()->SetHeaderView(std::move(image_view));
  }
  if (controller_->IsWalletableEntity()) {
    GetBubbleFrameView()->SetTitleView(
        CreateWalletBubbleTitleView(controller_->GetSaveUpdateDialogTitle()));
  }
}

void AutofillAiImportDataBubbleView::WindowClosing() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetAutofillAiBubbleResultFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

bool AutofillAiImportDataBubbleView::OnDialogAccepted() {
  if (!controller_) {
    return true;
  }
  controller_->OnSaveButtonClicked();
  if (controller_->CloseOnAccept()) {
    return true;
  }

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  loading_progress_row_->SetVisible(true);
  loading_throbber_->Start();
  loading_throbber_->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_WALLET_UPLOAD_THROBBER_ACCESSIBLE_NAME));
  DialogModelChanged();
  return false;
}

BEGIN_METADATA(AutofillAiImportDataBubbleView)
END_METADATA
}  // namespace autofill
