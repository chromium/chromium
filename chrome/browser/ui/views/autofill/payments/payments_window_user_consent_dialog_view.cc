// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/payments_window_user_consent_dialog_view.h"

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/autofill/payments/view_factory.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
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
  return dialog_view->GetWeakPtr();
}

namespace payments {

PaymentsWindowUserConsentDialogView::PaymentsWindowUserConsentDialogView(
    base::WeakPtr<PaymentsWindowUserConsentDialogController> controller)
    : payments_window_user_consent_dialog_controller_(controller) {
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      payments_window_user_consent_dialog_controller_->GetOkButtonLabel());
  SetShowCloseButton(false);
  SetModalType(ui::MODAL_TYPE_CHILD);
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
  GetBubbleFrameView()->SetTitleView(CreateTitleView(
      GetWindowTitle(), TitleWithIconAndSeparatorView::Icon::GOOGLE_PAY));
}

std::u16string PaymentsWindowUserConsentDialogView::GetWindowTitle() const {
  return payments_window_user_consent_dialog_controller_->GetWindowTitle();
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PaymentsWindowUserConsentDialogView,
                                      kTopViewId);

}  // namespace payments

}  // namespace autofill
