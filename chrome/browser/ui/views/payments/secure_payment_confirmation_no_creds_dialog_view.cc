// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_no_creds_dialog_view.h"

#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_views_util.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/payments/content/payment_ui_observer.h"
#include "components/payments/content/secure_payment_confirmation_no_creds_model.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
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
    base::WeakPtr<SecurePaymentConfirmationNoCredsModel> model,
    ResponseCallback response_callback,
    OptOutCallback opt_out_callback) {
  DCHECK(model);
  model_ = model;

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  response_callback_ = std::move(response_callback);
  opt_out_callback_ = std::move(opt_out_callback);

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kOk));

  InitChildViews();

  SetAccessibleTitle(model_->no_creds_text());

  SetAcceptCallback(base::BindOnce(
      &SecurePaymentConfirmationNoCredsDialogView::OnDialogClosed,
      weak_ptr_factory_.GetWeakPtr()));
  SetCancelCallback(base::BindOnce(
      &SecurePaymentConfirmationNoCredsDialogView::OnDialogClosed,
      weak_ptr_factory_.GetWeakPtr()));
  SetCloseCallback(base::BindOnce(
      &SecurePaymentConfirmationNoCredsDialogView::OnDialogClosed,
      weak_ptr_factory_.GetWeakPtr()));

  SetModalType(ui::mojom::ModalType::kChild);

  views::Widget* widget =
      constrained_window::ShowWebModalDialogViews(this, web_contents);
  extensions::SecurityDialogTracker::GetInstance()->AddSecurityDialog(widget);
}

void SecurePaymentConfirmationNoCredsDialogView::HideDialog() {
  if (GetWidget())
    GetWidget()->Close();
}

bool SecurePaymentConfirmationNoCredsDialogView::ClickOptOutForTesting() {
  if (!model_->opt_out_visible())
    return false;
  OnOptOutClicked();
  return true;
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

void SecurePaymentConfirmationNoCredsDialogView::OnOptOutClicked() {
  DCHECK(model_->opt_out_visible());

  std::move(opt_out_callback_).Run();

  if (observer_for_test_) {
    observer_for_test_->OnOptOutClicked();
  }
}

void SecurePaymentConfirmationNoCredsDialogView::InitChildViews() {
  RemoveAllChildViews();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  AddChildView(CreateSecurePaymentConfirmationHeaderIcon(
      static_cast<int>(DialogViewID::HEADER_ICON), /*use_cart_image=*/true));

  AddChildView(CreateBodyView());

  if (model_->opt_out_visible()) {
    SetFootnoteView(CreateSecurePaymentConfirmationOptOutView(
        model_->relying_party_id(), model_->opt_out_label(),
        model_->opt_out_link_label(),
        base::BindRepeating(
            &SecurePaymentConfirmationNoCredsDialogView::OnOptOutClicked,
            weak_ptr_factory_.GetWeakPtr())));
  }

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
SecurePaymentConfirmationNoCredsDialogView::CreateBodyView() {
  std::unique_ptr<views::Label> no_matching_creds_view =
      std::make_unique<views::Label>(model_->no_creds_text(),
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

BEGIN_METADATA(SecurePaymentConfirmationNoCredsDialogView)
END_METADATA

}  // namespace payments
