// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_no_creds.h"

#include <memory>

#include "build/build_config.h"
#include "components/payments/content/secure_payment_confirmation_no_creds_view.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

// static
std::unique_ptr<SecurePaymentConfirmationNoCreds>
SecurePaymentConfirmationNoCreds::Create() {
  return std::make_unique<SecurePaymentConfirmationNoCreds>();
}

SecurePaymentConfirmationNoCreds::SecurePaymentConfirmationNoCreds() = default;

SecurePaymentConfirmationNoCreds::~SecurePaymentConfirmationNoCreds() {
  CloseDialog();
}

void SecurePaymentConfirmationNoCreds::ShowDialog(
    content::WebContents* web_contents,
    const std::u16string& merchant_name,
    ResponseCallback response_callback) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED();
#endif  // BUILDFLAG(IS_ANDROID)
  DCHECK(!view_);

  view_ = SecurePaymentConfirmationNoCredsView::Create();
  view_->ShowDialog(web_contents,
                    l10n_util::GetStringFUTF16(
                        IDS_NO_MATCHING_CREDENTIAL_DESCRIPTION, merchant_name),
                    std::move(response_callback));
}

void SecurePaymentConfirmationNoCreds::CloseDialog() {
  if (!view_)
    return;
  view_->HideDialog();
}

}  // namespace payments
