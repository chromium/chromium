// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/common/autofill_features.h"
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
#include "ui/views/style/typography.h"

namespace autofill {

SaveCardBubbleViews::SyncPromoDelegate::SyncPromoDelegate(
    SaveCardBubbleController* controller,
    signin_metrics::AccessPoint access_point)
    : controller_(controller), access_point_(access_point) {
  DCHECK(controller_);
}

void SaveCardBubbleViews::SyncPromoDelegate::OnEnableSync(
    const AccountInfo& account,
    bool is_default_promo_account) {
  controller_->OnSyncPromoAccepted(account, access_point_,
                                   is_default_promo_account);
}

SaveCardBubbleViews::SaveCardBubbleViews(views::View* anchor_view,
                                         content::WebContents* web_contents,
                                         SaveCardBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  SetButtonLabel(ui::DIALOG_BUTTON_OK, controller->GetAcceptButtonText());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, controller->GetDeclineButtonText());
  SetCancelCallback(base::BindOnce(&SaveCardBubbleViews::OnDialogCancelled,
                                   base::Unretained(this)));
  SetAcceptCallback(base::BindOnce(&SaveCardBubbleViews::OnDialogAccepted,
                                   base::Unretained(this)));
  DCHECK(controller);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::SAVE_CARD);
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
  if (controller_)
    controller_->OnBubbleClosed(closed_reason_);

  controller_ = nullptr;
}

void SaveCardBubbleViews::OnDialogAccepted() {
  // TODO(https://crbug.com/1046793): Maybe delete this.
  if (controller_)
    controller_->OnSaveButton({});
}

void SaveCardBubbleViews::OnDialogCancelled() {
  // TODO(https://crbug.com/1046793): Maybe delete this.
  if (controller_)
    controller_->OnCancelButton();
}

gfx::Size SaveCardBubbleViews::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void SaveCardBubbleViews::AddedToWidget() {
  // Use a custom title container if offering to upload a server card.
  // Done when this view is added to the widget, so the bubble frame
  // view is guaranteed to exist.
  if (!controller_->IsUploadSave())
    return;

  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAndSeparatorView>(GetWindowTitle()));
}

bool SaveCardBubbleViews::ShouldShowCloseButton() const {
  return true;
}

base::string16 SaveCardBubbleViews::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : base::string16();
}

void SaveCardBubbleViews::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(closed_reason_);
    controller_ = nullptr;
  }
}

void SaveCardBubbleViews::OnWidgetClosing(views::Widget* widget) {
  LocationBarBubbleDelegateView::OnWidgetDestroying(widget);
  closed_reason_ = GetPaymentsBubbleClosedReasonFromWidgetClosedReason(
      widget->closed_reason());
}

views::View* SaveCardBubbleViews::GetFootnoteViewForTesting() {
  return footnote_view_;
}

const base::string16 SaveCardBubbleViews::GetCardIdentifierString() const {
  return controller_->GetCard().CardIdentifierStringForAutofillDisplay();
}

SaveCardBubbleViews::~SaveCardBubbleViews() = default;

// Overridden
std::unique_ptr<views::View> SaveCardBubbleViews::CreateMainContentView() {
  std::unique_ptr<views::View> view = std::make_unique<views::View>();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  // If applicable, add the upload explanation label.  Appears above the card
  // info.
  base::string16 explanation = controller_->GetExplanatoryMessage();
  if (!explanation.empty()) {
    auto* explanation_label =
        new views::Label(explanation, views::style::CONTEXT_DIALOG_BODY_TEXT,
                         views::style::STYLE_SECONDARY);
    explanation_label->SetMultiLine(true);
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    view->AddChildView(explanation_label);
  }

  // Add the card network icon, last four digits and expiration date.
  auto* description_view = new views::View();
  views::BoxLayout* box_layout =
      description_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          provider->GetDistanceMetric(
              views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
  view->AddChildView(description_view);

  const CreditCard& card = controller_->GetCard();
  auto* card_network_icon = new views::ImageView();
  card_network_icon->SetImage(
      ui::ResourceBundle::GetSharedInstance()
          .GetImageNamed(CreditCard::IconResourceId(card.network()))
          .AsImageSkia());
  card_network_icon->SetTooltipText(card.NetworkForDisplay());
  description_view->AddChildView(card_network_icon);

  views::Label* label = description_view->AddChildView(new views::Label(
      GetCardIdentifierString(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  int label_width =
      GetPreferredSize().width() -
      card_network_icon->GetPreferredSize().width() -
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

  if (!card.IsExpired(base::Time::Now())) {
    // The spacer will stretch to use the available horizontal space in the
    // dialog, which will end-align the expiration date label.
    auto* spacer = new views::View();
    description_view->AddChildView(spacer);
    box_layout->SetFlexForView(spacer, /*flex=*/1);

    auto* expiration_date_label = new views::Label(
        card.AbbreviatedExpirationDateForDisplay(false),
        views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
    expiration_date_label->SetID(DialogViewId::EXPIRATION_DATE_LABEL);
    description_view->AddChildView(expiration_date_label);
    constexpr int kExpirationDateLabelWidth = 60;
    label_width -=
        kExpirationDateLabelWidth +
        provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  }
  label->SetMaximumWidth(label_width);
  return view;
}

void SaveCardBubbleViews::InitFootnoteView(views::View* footnote_view) {
  DCHECK(!footnote_view_);
  footnote_view_ = footnote_view;
  footnote_view_->SetID(DialogViewId::FOOTNOTE_VIEW);
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
  // controls; use views::TEXT. For local cards, since there is no explanation,
  // use views::CONTROL instead.
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      controller_->GetExplanatoryMessage().empty() ? views::CONTROL
                                                   : views::TEXT,
      GetDialogButtons() == ui::DIALOG_BUTTON_NONE ? views::TEXT
                                                   : views::CONTROL));
  AddChildView(CreateMainContentView().release());
}

}  // namespace autofill
