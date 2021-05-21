// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential_enrollment_bridge_desktop.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "build/build_config.h"
#include "components/payments/content/payment_credential_enrollment_model.h"
#include "components/payments/content/payment_credential_enrollment_view.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

// static
std::unique_ptr<PaymentCredentialEnrollmentBridge>
PaymentCredentialEnrollmentBridge::Create() {
  return std::make_unique<PaymentCredentialEnrollmentBridgeDesktop>();
}

PaymentCredentialEnrollmentBridgeDesktop::
    PaymentCredentialEnrollmentBridgeDesktop() = default;

PaymentCredentialEnrollmentBridgeDesktop::
    ~PaymentCredentialEnrollmentBridgeDesktop() = default;

void PaymentCredentialEnrollmentBridgeDesktop::ShowDialog(
    content::WebContents* web_contents,
    std::unique_ptr<SkBitmap> instrument_icon,
    const std::u16string& instrument_name,
    ResponseCallback response_callback) {
#if defined(OS_ANDROID)
  NOTREACHED();
#endif  // OS_ANDROID
  DCHECK(!view_);

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
      web_contents->GetBrowserContext()->IsOffTheRecord()
          ? l10n_util::GetStringUTF16(
                IDS_PAYMENT_CREDENTIAL_ENROLLMENT_OFF_THE_RECORD_DESCRIPTION)
          : std::u16string());

  model_.set_accept_button_label(l10n_util::GetStringUTF16(
      IDS_PAYMENT_CREDENTIAL_ENROLLMENT_ACCEPT_BUTTON_LABEL));

  model_.set_cancel_button_label(l10n_util::GetStringUTF16(
      IDS_PAYMENT_CREDENTIAL_ENROLLMENT_CANCEL_BUTTON_LABEL));

  view_ = PaymentCredentialEnrollmentView::Create();
  view_->ShowDialog(web_contents, model_.GetWeakPtr(),
                    std::move(response_callback));
}

void PaymentCredentialEnrollmentBridgeDesktop::ShowProcessingSpinner() {
  if (!view_)
    return;

  model_.set_progress_bar_visible(true);
  model_.set_accept_button_enabled(false);
  model_.set_cancel_button_enabled(false);
  view_->OnModelUpdated();
}

void PaymentCredentialEnrollmentBridgeDesktop::CloseDialog() {
  if (view_) {
    view_->HideDialog();
    view_.reset();
  }
}

}  // namespace payments
