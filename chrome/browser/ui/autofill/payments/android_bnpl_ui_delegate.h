// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_ANDROID_BNPL_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_ANDROID_BNPL_UI_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/ui/payments/bnpl_ui_delegate.h"

namespace autofill {

struct AutofillErrorDialogContext;
enum class AutofillProgressDialogType;
class BnplIssuer;
struct BnplTosModel;

namespace payments {

struct BnplIssuerContext;
class PaymentsAutofillClient;

// Android implementation of the BnplUiDelegate interface. This class handles
// the UI for the BNPL autofill flow on the Android platform.
class AndroidBnplUiDelegate : public BnplUiDelegate {
 public:
  explicit AndroidBnplUiDelegate(PaymentsAutofillClient* client);
  AndroidBnplUiDelegate(const AndroidBnplUiDelegate& other) = delete;
  AndroidBnplUiDelegate& operator=(const AndroidBnplUiDelegate& other) = delete;
  ~AndroidBnplUiDelegate() override;

  // BnplUiDelegate:
  void ShowSelectBnplIssuerUi(
      std::vector<BnplIssuerContext> bnpl_issuer_context,
      std::string app_locale,
      base::OnceCallback<void(autofill::BnplIssuer)> selected_issuer_callback,
      base::OnceClosure cancel_callback) override;
  void DismissSelectBnplIssuerUi() override;
  void ShowBnplTosUi(BnplTosModel bnpl_tos_model,
                     base::OnceClosure accept_callback,
                     base::OnceClosure cancel_callback) override;
  void CloseBnplTosUi() override;
  void ShowProgressUi(AutofillProgressDialogType autofill_progress_dialog_type,
                      base::OnceClosure cancel_callback) override;
  void CloseProgressUi(bool show_confirmation_before_closing) override;
  void ShowAutofillErrorUi(AutofillErrorDialogContext context) override;

 private:
  const raw_ref<PaymentsAutofillClient> client_;
};

}  // namespace payments

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_ANDROID_BNPL_UI_DELEGATE_H_
