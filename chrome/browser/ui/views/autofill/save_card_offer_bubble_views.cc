// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_card_offer_bubble_views.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/legal_message_line.h"
#include "components/autofill/core/browser/ui/save_card_bubble_controller.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

namespace {
const int kTooltipIconSize = 12;
}  // namespace

SaveCardOfferBubbleViews::SaveCardOfferBubbleViews(
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    content::WebContents* web_contents,
    SaveCardBubbleController* controller)
    : SaveCardBubbleViews(anchor_view, anchor_point, web_contents, controller) {
}

views::View* SaveCardOfferBubbleViews::CreateFootnoteView() {
  if (controller()->GetLegalMessageLines().empty())
    return nullptr;

  legal_message_view_ =
      new LegalMessageView(controller()->GetLegalMessageLines(), this);

  InitFootnoteView(legal_message_view_);
  return legal_message_view_;
}

bool SaveCardOfferBubbleViews::Accept() {
  if (controller())
    controller()->OnSaveButton(cardholder_name_textfield_
                                   ? cardholder_name_textfield_->text()
                                   : base::string16());
  return true;
}

base::string16 SaveCardOfferBubbleViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK
                                       ? IDS_AUTOFILL_SAVE_CARD_PROMPT_ACCEPT
                                       : IDS_NO_THANKS);
}

bool SaveCardOfferBubbleViews::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_CANCEL)
    return true;

  DCHECK_EQ(ui::DIALOG_BUTTON_OK, button);
  if (cardholder_name_textfield_) {
    // If requesting the user confirm the name, it cannot be blank.
    base::string16 trimmed_text;
    base::TrimWhitespace(cardholder_name_textfield_->text(), base::TRIM_ALL,
                         &trimmed_text);
    return !trimmed_text.empty();
  }
  return true;
}

void SaveCardOfferBubbleViews::StyledLabelLinkClicked(views::StyledLabel* label,
                                                      const gfx::Range& range,
                                                      int event_flags) {
  if (!controller())
    return;

  controller()->OnLegalMessageLinkClicked(
      legal_message_view_->GetUrlForLink(label, range));
}

void SaveCardOfferBubbleViews::ContentsChanged(
    views::Textfield* sender,
    const base::string16& new_contents) {
  DCHECK_EQ(cardholder_name_textfield_, sender);
  DialogModelChanged();
}

SaveCardOfferBubbleViews::~SaveCardOfferBubbleViews() {}

std::unique_ptr<views::View> SaveCardOfferBubbleViews::CreateMainContentView() {
  std::unique_ptr<views::View> view =
      SaveCardBubbleViews::CreateMainContentView();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  view->set_id(controller()->IsUploadSave()
                   ? DialogViewId::MAIN_CONTENT_VIEW_UPLOAD
                   : DialogViewId::MAIN_CONTENT_VIEW_LOCAL);

  // If necessary, add the cardholder name label and textfield to the upload
  // save dialog.
  if (controller()->ShouldRequestNameFromUser()) {
    std::unique_ptr<views::View> cardholder_name_label_row =
        std::make_unique<views::View>();

    // Set up cardholder name label.
    // TODO(jsaul): DISTANCE_RELATED_BUTTON_HORIZONTAL isn't the right choice
    //              here, but DISTANCE_RELATED_CONTROL_HORIZONTAL gives too much
    //              padding. Make a new Harmony DistanceMetric?
    cardholder_name_label_row->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::kHorizontal, gfx::Insets(),
            provider->GetDistanceMetric(
                views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
    std::unique_ptr<views::Label> cardholder_name_label =
        std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME),
            CONTEXT_BODY_TEXT_LARGE, ChromeTextStyle::STYLE_SECONDARY);
    cardholder_name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    cardholder_name_label_row->AddChildView(cardholder_name_label.release());

    // Prepare the prefilled cardholder name.
    base::string16 prefilled_name;
    if (!features::
            IsAutofillUpstreamBlankCardholderNameFieldExperimentEnabled()) {
      prefilled_name =
          base::UTF8ToUTF16(controller()->GetAccountInfo().full_name);
    }

    // Set up cardholder name label tooltip ONLY if the cardholder name
    // textfield will be prefilled.
    if (!prefilled_name.empty()) {
      std::unique_ptr<views::TooltipIcon> cardholder_name_tooltip =
          std::make_unique<views::TooltipIcon>(
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME_TOOLTIP),
              kTooltipIconSize);
      cardholder_name_tooltip->set_anchor_point_arrow(
          views::BubbleBorder::Arrow::TOP_LEFT);
      cardholder_name_tooltip->set_id(DialogViewId::CARDHOLDER_NAME_TOOLTIP);
      cardholder_name_label_row->AddChildView(
          cardholder_name_tooltip.release());
    }

    // Set up cardholder name textfield.
    DCHECK(!cardholder_name_textfield_);
    cardholder_name_textfield_ = new views::Textfield();
    cardholder_name_textfield_->set_controller(this);
    cardholder_name_textfield_->set_id(DialogViewId::CARDHOLDER_NAME_TEXTFIELD);
    cardholder_name_textfield_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME));
    cardholder_name_textfield_->SetTextInputType(
        ui::TextInputType::TEXT_INPUT_TYPE_TEXT);
    cardholder_name_textfield_->SetText(prefilled_name);
    AutofillMetrics::LogSaveCardCardholderNamePrefilled(
        !prefilled_name.empty());

    // Add cardholder name elements to a single view, then to the final dialog.
    std::unique_ptr<views::View> cardholder_name_view =
        std::make_unique<views::View>();
    cardholder_name_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical, gfx::Insets(),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
    cardholder_name_view->AddChildView(cardholder_name_label_row.release());
    cardholder_name_view->AddChildView(cardholder_name_textfield_);
    view->AddChildView(cardholder_name_view.release());
  }

  return view;
}

}  // namespace autofill
