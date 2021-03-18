// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential_enrollment_controller.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "build/build_config.h"
#include "components/payments/content/payment_credential_enrollment_model.h"
#include "components/payments/content/payment_credential_enrollment_view.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

PaymentCredentialEnrollmentController::ScopedToken::ScopedToken() = default;
PaymentCredentialEnrollmentController::ScopedToken::~ScopedToken() = default;

base::WeakPtr<PaymentCredentialEnrollmentController::ScopedToken>
PaymentCredentialEnrollmentController::ScopedToken::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// static
PaymentCredentialEnrollmentController*
PaymentCredentialEnrollmentController::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  // Creates a new object only if WebContents does not already have one attached
  // to it:
  PaymentCredentialEnrollmentController::CreateForWebContents(web_contents);
  return PaymentCredentialEnrollmentController::FromWebContents(web_contents);
}

PaymentCredentialEnrollmentController::PaymentCredentialEnrollmentController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

PaymentCredentialEnrollmentController::
    ~PaymentCredentialEnrollmentController() = default;

void PaymentCredentialEnrollmentController::ShowDialog(
    content::GlobalFrameRoutingId initiator_frame_routing_id,
    std::unique_ptr<SkBitmap> instrument_icon,
    const std::u16string& instrument_name,
    ResponseCallback response_callback) {
#if defined(OS_ANDROID)
  NOTREACHED();
#endif  // OS_ANDROID
  DCHECK(!view_);

  is_user_response_recorded_ = false;
  initiator_frame_routing_id_ = initiator_frame_routing_id;
  response_callback_ = std::move(response_callback);

  model_.set_instrument_icon(std::move(instrument_icon));

  model_.set_progress_bar_visible(false);
  model_.set_accept_button_enabled(true);
  model_.set_cancel_button_enabled(true);

  model_.set_title(
      l10n_util::GetStringUTF16(IDS_PAYMENT_CREDENTIAL_ENROLLMENT_TITLE));

  model_.set_description(
      l10n_util::GetStringUTF16(IDS_PAYMENT_CREDENTIAL_ENROLLMENT_DESCRIPTION));

  model_.set_instrument_name(instrument_name);

  model_.set_extra_description(
      web_contents()->GetBrowserContext()->IsOffTheRecord()
          ? l10n_util::GetStringUTF16(
                IDS_PAYMENT_CREDENTIAL_ENROLLMENT_OFF_THE_RECORD_DESCRIPTION)
          : std::u16string());

  model_.set_accept_button_label(l10n_util::GetStringUTF16(
      IDS_PAYMENT_CREDENTIAL_ENROLLMENT_ACCEPT_BUTTON_LABEL));

  model_.set_cancel_button_label(l10n_util::GetStringUTF16(
      IDS_PAYMENT_CREDENTIAL_ENROLLMENT_CANCEL_BUTTON_LABEL));

  view_ = PaymentCredentialEnrollmentView::Create();
  view_->ShowDialog(
      web_contents(), model_.GetWeakPtr(),
      base::BindOnce(&PaymentCredentialEnrollmentController::OnConfirm,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PaymentCredentialEnrollmentController::OnCancel,
                     weak_ptr_factory_.GetWeakPtr()));

  if (observer_for_test_)
    observer_for_test_->OnDialogOpened();
}

void PaymentCredentialEnrollmentController::ShowProcessingSpinner() {
  if (!view_)
    return;

  model_.set_progress_bar_visible(true);
  model_.set_accept_button_enabled(false);
  model_.set_cancel_button_enabled(false);
  view_->OnModelUpdated();
}

void PaymentCredentialEnrollmentController::CloseDialog() {
  RecordFirstCloseReason(SecurePaymentConfirmationEnrollDialogResult::kClosed);

  if (view_) {
    view_->HideDialog();
    view_.reset();
  }
}

void PaymentCredentialEnrollmentController::OnCancel() {
  RecordFirstCloseReason(
      SecurePaymentConfirmationEnrollDialogResult::kCanceled);

  // Prevent use-after-move on `response_callback_` due to CloseDialog()
  // re-entering into OnCancel().
  ResponseCallback callback = std::move(response_callback_);

  if (!callback)
    return;  // The dialog is closing after user interaction has completed.

  CloseDialog();  // CloseDialog() will re-enter into OnCancel().

  std::move(callback).Run(false);
}

void PaymentCredentialEnrollmentController::OnConfirm() {
  DCHECK(web_contents());

  RecordFirstCloseReason(
      SecurePaymentConfirmationEnrollDialogResult::kAccepted);

  ShowProcessingSpinner();

  // This will trigger WebAuthn with OS-level UI (if any) on top of the |view_|
  // with its animated processing spinner. For example, on Linux, there's no
  // OS-level UI, while on MacOS, there's an OS-level prompt for the Touch ID
  // that shows on top of Chrome.
  std::move(response_callback_).Run(true);
}

std::unique_ptr<PaymentCredentialEnrollmentController::ScopedToken>
PaymentCredentialEnrollmentController::GetTokenIfAvailable() {
  if (token_)
    return nullptr;

  auto token = std::make_unique<ScopedToken>();
  token_ = token->GetWeakPtr();
  return token;
}

base::WeakPtr<PaymentCredentialEnrollmentController>
PaymentCredentialEnrollmentController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PaymentCredentialEnrollmentController::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Close the dialog if either the initiator frame (which may be an iframe) or
  // main frame was navigated away.
  if (!navigation_handle->IsSameDocument() &&
      (navigation_handle->IsInMainFrame() ||
       navigation_handle->GetPreviousRenderFrameHostId() ==
           initiator_frame_routing_id_)) {
    CloseDialog();
  }
}

void PaymentCredentialEnrollmentController::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // Close the dialog if either the initiator frame (which may be an iframe) or
  // main frame was deleted.
  if (render_frame_host == web_contents()->GetMainFrame() ||
      render_frame_host->GetGlobalFrameRoutingId() ==
          initiator_frame_routing_id_) {
    CloseDialog();
  }
}

void PaymentCredentialEnrollmentController::RecordFirstCloseReason(
    SecurePaymentConfirmationEnrollDialogResult result) {
  if (!is_user_response_recorded_ && view_) {
    is_user_response_recorded_ = true;
    RecordEnrollDialogResult(result);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PaymentCredentialEnrollmentController)

}  // namespace payments
