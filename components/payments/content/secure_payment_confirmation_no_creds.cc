// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_no_creds.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
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
    const std::string& rp_id,
    ResponseCallback response_callback,
    OptOutCallback opt_out_callback) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION();
#endif  // BUILDFLAG(IS_ANDROID)
  DCHECK(!view_);

  model_.set_no_creds_text(l10n_util::GetStringFUTF16(
      IDS_NO_MATCHING_CREDENTIAL_DESCRIPTION, merchant_name));
  model_.set_opt_out_visible(!opt_out_callback.is_null());
  model_.set_opt_out_label(
      l10n_util::GetStringUTF16(IDS_SECURE_PAYMENT_CONFIRMATION_OPT_OUT_LABEL));
  model_.set_opt_out_link_label(l10n_util::GetStringUTF16(
      IDS_SECURE_PAYMENT_CONFIRMATION_OPT_OUT_LINK_LABEL));
  model_.set_relying_party_id(base::UTF8ToUTF16(rp_id));

  view_ = SecurePaymentConfirmationNoCredsView::Create();
  view_->ShowDialog(web_contents, model_.GetWeakPtr(),
                    std::move(response_callback), std::move(opt_out_callback));
}

void SecurePaymentConfirmationNoCreds::CloseDialog() {
  if (!view_)
    return;
  view_->HideDialog();
}

bool SecurePaymentConfirmationNoCreds::ClickOptOutForTesting() {
  if (!view_)
    return false;
  return view_->ClickOptOutForTesting();
}

}  // namespace payments
