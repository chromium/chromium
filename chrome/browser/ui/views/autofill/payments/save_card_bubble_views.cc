// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"

namespace autofill {

SaveCardBubbleViews::SaveCardBubbleViews(views::View* anchor_view,
                                         content::WebContents* web_contents,
                                         SaveCardBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(controller);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, controller->GetAcceptButtonText());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, controller->GetDeclineButtonText());
  SetAcceptCallback(base::BindOnce(&SaveCardBubbleViews::OnDialogAccepted,
                                   base::Unretained(this)));

  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

void SaveCardBubbleViews::Show(DisplayReason reason) {
  ShowForReason(reason);
  AssignIdsToDialogButtons();
}

void SaveCardBubbleViews::Hide() {
  CloseBubble();

  // If |controller_| is null, WindowClosing() won't invoke OnBubbleClosed(), so
  // do that here. This will clear out |controller_|'s reference to |this|. Note
  // that WindowClosing() happens only after the _asynchronous_ Close() task
  // posted in CloseBubble() completes, but we need to fix references sooner.
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void SaveCardBubbleViews::OnDialogAccepted() {
  // TODO(https://crbug.com/1046793): Maybe delete this.
  if (controller_)
    controller_->OnSaveButton({});
}

void SaveCardBubbleViews::AddedToWidget() {
  // Use a custom title container if offering to upload a server card.
  // Done when this view is added to the widget, so the bubble frame
  // view is guaranteed to exist.
  if (!controller_->IsUploadSave())
    return;

  GetBubbleFrameView()->SetTitleView(CreateTitleView(
      GetWindowTitle(), TitleWithIconAndSeparatorView::Icon::GOOGLE_PAY));
}

std::u16string SaveCardBubbleViews::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void SaveCardBubbleViews::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

views::View* SaveCardBubbleViews::GetFootnoteViewForTesting() {
  return footnote_view_;
}

const std::u16string SaveCardBubbleViews::GetCardIdentifierString() const {
  return controller_->GetCard().CardNameAndLastFourDigits();
}

SaveCardBubbleViews::~SaveCardBubbleViews() = default;

// Overridden
std::unique_ptr<views::View> SaveCardBubbleViews::CreateMainContentView() {
  ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();

  auto view = std::make_unique<views::BoxLayoutView>();
  view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  view->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  // If applicable, add the upload explanation label.  Appears above the card
  // info.
  std::u16string explanation = controller_->GetExplanatoryMessage();
  if (!explanation.empty()) {
    auto* const explanation_label =
        view->AddChildView(std::make_unique<views::Label>(
            explanation, views::style::CONTEXT_DIALOG_BODY_TEXT,
            views::style::STYLE_SECONDARY));
    explanation_label->SetMultiLine(true);
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  // Add the card network icon, last four digits and expiration date.
  auto* description_view =
      view->AddChildView(std::make_unique<views::BoxLayoutView>());
  description_view->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));

  const CreditCard& card = controller_->GetCard();
  auto* const card_network_icon =
      description_view->AddChildView(std::make_unique<views::ImageView>());
  card_network_icon->SetImage(
      ui::ResourceBundle::GetSharedInstance()
          .GetImageNamed(CreditCard::IconResourceId(card.network()))
          .AsImageSkia());
  card_network_icon->SetTooltipText(card.NetworkForDisplay());
  auto* card_identifier_view =
      description_view->AddChildView(GetCardIdentifierView());

  // Flex |card_identifier_view| to fill up space before the expiry date.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillMoveLegalTermsAndIconForNewCardEnrollment)) {
    description_view->SetFlexForView(card_identifier_view, 1);
  }

  return view;
}

void SaveCardBubbleViews::InitFootnoteView(views::View* footnote_view) {
  DCHECK(!footnote_view_);
  footnote_view_ = footnote_view;
  footnote_view_->SetID(DialogViewId::FOOTNOTE_VIEW);
}

std::unique_ptr<views::View> SaveCardBubbleViews::GetCardIdentifierView() {
  // If feature enabled, display the expiration date of this view in a
  // separate lines. Else, card name, last 4 digit and expiration date
  // in the same line
  auto card_identifier_view = std::make_unique<views::BoxLayoutView>();
  if (base::FeatureList::IsEnabled(
          features::kAutofillMoveLegalTermsAndIconForNewCardEnrollment)) {
    card_identifier_view->SetOrientation(
        views::BoxLayout::Orientation::kVertical);
    card_identifier_view->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
  } else {
    card_identifier_view->SetBetweenChildSpacing(
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  }

  auto* const card_identifier_label =
      card_identifier_view->AddChildView(std::make_unique<views::Label>(
          GetCardIdentifierString(), views::style::CONTEXT_DIALOG_BODY_TEXT,
          views::style::STYLE_PRIMARY));
  card_identifier_label->SetMultiLine(true);
  card_identifier_label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);

  if (!base::FeatureList::IsEnabled(
          features::kAutofillMoveLegalTermsAndIconForNewCardEnrollment)) {
    // Flex |card_identifier_label| to fill up remaining space and tail align
    // the expiry date.
    card_identifier_view->SetFlexForView(card_identifier_label, 1);
  }

  const CreditCard& card = controller_->GetCard();
  if (!card.IsExpired(base::Time::Now())) {
    auto* expiration_date_label =
        card_identifier_view->AddChildView(std::make_unique<views::Label>(
            card.AbbreviatedExpirationDateForDisplay(false),
            views::style::CONTEXT_DIALOG_BODY_TEXT,
            views::style::STYLE_SECONDARY));
    expiration_date_label->SetID(DialogViewId::EXPIRATION_DATE_LABEL);
  }

  return card_identifier_view;
}

void SaveCardBubbleViews::AssignIdsToDialogButtons() {
  auto* ok_button = GetOkButton();
  if (ok_button)
    ok_button->SetID(DialogViewId::OK_BUTTON);
  auto* cancel_button = GetCancelButton();
  if (cancel_button)
    cancel_button->SetID(DialogViewId::CANCEL_BUTTON);
}

void SaveCardBubbleViews::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  // For server cards, there is an explanation between the title and the
  // controls; use DialogContentType::kText. For local cards, since there is no
  // explanation, use DialogContentType::kControl instead.
  // When feature kAutofillMoveLegalTermsAndIconForNewCardEnrollment is enabled,
  // there are legal messages before the buttons for server cards, so use
  // DialogContentType::kText. For local card, since there is no legal message,
  // use DialogContentType::kControl instead.
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      controller_->GetExplanatoryMessage().empty()
          ? views::DialogContentType::kControl
          : views::DialogContentType::kText,
      base::FeatureList::IsEnabled(
          features::kAutofillMoveLegalTermsAndIconForNewCardEnrollment) &&
              !controller_->GetLegalMessageLines().empty()
          ? views::DialogContentType::kText
          : views::DialogContentType::kControl));
  AddChildView(CreateMainContentView());
}

}  // namespace autofill
