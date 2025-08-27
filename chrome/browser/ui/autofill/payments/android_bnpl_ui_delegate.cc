// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/android_bnpl_ui_delegate.h"

#include "base/functional/callback.h"

namespace autofill::payments {

AndroidBnplUiDelegate::AndroidBnplUiDelegate() = default;

AndroidBnplUiDelegate::~AndroidBnplUiDelegate() = default;

void AndroidBnplUiDelegate::ShowSelectBnplIssuerUi(
    std::vector<BnplIssuerContext> bnpl_issuer_context,
    std::string app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
  // TODO(crbug.com/438783909): Add JNI call to show the TouchToFill bottom
  // sheet with the BNPL issuer selection screen.
}

void AndroidBnplUiDelegate::DismissSelectBnplIssuerUi() {
  // TODO(crbug.com/438783909): Add JNI call to dismiss the TouchToFill bottom
  // sheet with the BNPL issuer selection screen.
}

}  // namespace autofill::payments
