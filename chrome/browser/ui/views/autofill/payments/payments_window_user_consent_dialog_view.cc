// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/payments_window_user_consent_dialog_view.h"

#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "chrome/browser/ui/autofill/payments/view_factory.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/metrics/payments/payments_window_metrics.h"
#include "components/autofill/core/browser/ui/payments/payments_window_user_consent_dialog_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {

base::WeakPtr<payments::PaymentsWindowUserConsentDialog>
CreateAndShowPaymentsWindowUserConsentDialog(
    base::WeakPtr<payments::PaymentsWindowUserConsentDialogController>
        controller,
    content::WebContents* web_contents,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback) {
  payments::PaymentsWindowUserConsentDialogView* dialog_view =
      new payments::PaymentsWindowUserConsentDialogView(controller);
  dialog_view->SetAcceptCallback(std::move(accept_callback));
  dialog_view->SetCancelCallback(std::move(cancel_callback));
  constrained_window::ShowWebModalDialogViews(dialog_view, web_contents);
  autofill_metrics::LogPaymentsWindowUserConsentDialogShown();
  return dialog_view->GetWeakPtr();
}

namespace payments {

namespace {

using PaymentsWindowUserConsentDialogResult =
    autofill_metrics::PaymentsWindowUserConsentDialogResult;

PaymentsWindowUserConsentDialogResult GetDialogResultForClosedReason(
    views::Widget::ClosedReason closed_reason) {
  switch (closed_reason) {
    case views::Widget::ClosedReason::kCloseButtonClicked:
    case views::Widget::ClosedReason::kLostFocus:
      // There is no close button present for the consent dialog, so this should
      // never be reached. It is also a tab-modal dialog, so it should never
      // lose focus.
      NOTREACHED();
    case views::Widget::ClosedReason::kUnspecified:
      return PaymentsWindowUserConsentDialogResult::kTabOrBrowserClosed;
    case views::Widget::ClosedReason::kEscKeyPressed:
      return PaymentsWindowUserConsentDialogResult::kEscapeKeyPressed;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      return PaymentsWindowUserConsentDialogResult::kCancelButtonClicked;
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      return PaymentsWindowUserConsentDialogResult::kAcceptButtonClicked;
  }
}

}  // namespace

PaymentsWindowUserConsentDialogView::PaymentsWindowUserConsentDialogView(
    base::WeakPtr<PaymentsWindowUserConsentDialogController> controller)
    : payments_window_user_consent_dialog_controller_(controller) {
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      payments_window_user_consent_dialog_controller_->GetOkButtonLabel());
  SetShowCloseButton(false);
  RegisterWindowWillCloseCallback(
      base::BindOnce(&PaymentsWindowUserConsentDialogView::OnDialogClosing,
                     weak_ptr_factory_.GetWeakPtr()));
  SetModalType(ui::mojom::ModalType::kChild);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto* consent_dialog_description_label =
      AddChildView(std::make_unique<views::Label>(
          payments_window_user_consent_dialog_controller_
              ->GetDialogDescription(),
          ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
          views::style::STYLE_SECONDARY));
  consent_dialog_description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  consent_dialog_description_label->SetMultiLine(true);

  SetProperty(views::kElementIdentifierKey, kTopViewId);
}

PaymentsWindowUserConsentDialogView::~PaymentsWindowUserConsentDialogView() =
    default;

void PaymentsWindowUserConsentDialogView::Dismiss() {
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

base::WeakPtr<PaymentsWindowUserConsentDialog>
PaymentsWindowUserConsentDialogView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PaymentsWindowUserConsentDialogView::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAfterLabelView>(
          GetWindowTitle(), TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
}

std::u16string PaymentsWindowUserConsentDialogView::GetWindowTitle() const {
  return payments_window_user_consent_dialog_controller_->GetWindowTitle();
}

void PaymentsWindowUserConsentDialogView::OnDialogClosing() {
  if (payments_window_user_consent_dialog_controller_) {
    payments_window_user_consent_dialog_controller_->OnDialogClosing(
        GetDialogResultForClosedReason(GetWidget()->closed_reason()));
  }
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PaymentsWindowUserConsentDialogView,
                                      kTopViewId);

}  // namespace payments

}  // namespace autofill
