// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_no_creds_dialog_view.h"

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_views_util.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/payments/content/payment_ui_observer.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace payments {

// static
base::WeakPtr<SecurePaymentConfirmationNoCredsView>
SecurePaymentConfirmationNoCredsView::Create() {
  // On desktop, the SecurePaymentConfirmationNoCredsView object is memory
  // managed by the views:: machinery. It is deleted when the window is closed
  // and views::DialogDelegateView::DeleteDelegate() is called by its
  // corresponding views::Widget.
  return (new SecurePaymentConfirmationNoCredsDialogView(
              /*observer_for_test=*/nullptr))
      ->GetWeakPtr();
}

SecurePaymentConfirmationNoCredsDialogView::
    SecurePaymentConfirmationNoCredsDialogView(
        ObserverForTest* observer_for_test)
    : observer_for_test_(observer_for_test) {}
SecurePaymentConfirmationNoCredsDialogView::
    ~SecurePaymentConfirmationNoCredsDialogView() = default;

void SecurePaymentConfirmationNoCredsDialogView::ShowDialog(
    content::WebContents* web_contents,
    const std::u16string& no_creds_text,
    ResponseCallback response_callback) {
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetButtons(ui::DIALOG_BUTTON_OK);
  SetDefaultButton(ui::DIALOG_BUTTON_OK);

  InitChildViews(no_creds_text);

  SetAccessibleTitle(no_creds_text);

  response_callback_ = std::move(response_callback);

  SetAcceptCallback(base::BindOnce(
      &SecurePaymentConfirmationNoCredsDialogView::OnDialogClosed,
      weak_ptr_factory_.GetWeakPtr()));
  SetCancelCallback(base::BindOnce(
      &SecurePaymentConfirmationNoCredsDialogView::OnDialogClosed,
      weak_ptr_factory_.GetWeakPtr()));
  SetCloseCallback(base::BindOnce(
      &SecurePaymentConfirmationNoCredsDialogView::OnDialogClosed,
      weak_ptr_factory_.GetWeakPtr()));

  SetModalType(ui::MODAL_TYPE_CHILD);

  constrained_window::ShowWebModalDialogViews(this, web_contents);

  // observer_for_test_ is used in views browsertests.
  if (observer_for_test_)
    observer_for_test_->OnDialogOpened();
}

void SecurePaymentConfirmationNoCredsDialogView::HideDialog() {
  if (GetWidget())
    GetWidget()->Close();
}

bool SecurePaymentConfirmationNoCredsDialogView::ShouldShowCloseButton() const {
  return false;
}

base::WeakPtr<SecurePaymentConfirmationNoCredsDialogView>
SecurePaymentConfirmationNoCredsDialogView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SecurePaymentConfirmationNoCredsDialogView::OnDialogClosed() {
  auto callback = std::move(response_callback_);
  if (!callback)
    return;

  std::move(callback).Run();
  HideDialog();

  if (observer_for_test_)
    observer_for_test_->OnDialogClosed();
}

void SecurePaymentConfirmationNoCredsDialogView::InitChildViews(
    const std::u16string& no_creds_text) {
  RemoveAllChildViews();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  AddChildView(CreateSecurePaymentConfirmationHeaderView(
      static_cast<int>(DialogViewID::PROGRESS_BAR),
      static_cast<int>(DialogViewID::HEADER_IMAGE), /*use_cart_image=*/true));

  AddChildView(CreateBodyView(no_creds_text));

  InvalidateLayout();
}

// Creates the body.
// +------------------------------------------+
// |              [header image]              |
// |                                          |
// | No matching credentials text             |
// |                                     [OK] |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
std::unique_ptr<views::View>
SecurePaymentConfirmationNoCredsDialogView::CreateBodyView(
    const std::u16string& no_creds_text) {
  std::unique_ptr<views::Label> no_matching_creds_view =
      std::make_unique<views::Label>(no_creds_text,
                                     views::style::CONTEXT_DIALOG_BODY_TEXT,
                                     views::style::STYLE_PRIMARY);
  no_matching_creds_view->SetMultiLine(true);
  no_matching_creds_view->SetLineHeight(kDescriptionLineHeight);
  no_matching_creds_view->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  no_matching_creds_view->SetAllowCharacterBreak(true);
  no_matching_creds_view->SetID(
      static_cast<int>(DialogViewID::NO_MATCHING_CREDS_TEXT));
  no_matching_creds_view->SetBorder(views::CreateEmptyBorder(kBodyExtraInset));

  return no_matching_creds_view;
}

BEGIN_METADATA(SecurePaymentConfirmationNoCredsDialogView,
               views::DialogDelegateView)
END_METADATA

}  // namespace payments
