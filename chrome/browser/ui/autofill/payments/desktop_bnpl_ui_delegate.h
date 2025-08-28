// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_BNPL_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_BNPL_UI_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/ui/payments/bnpl_ui_delegate.h"

namespace autofill {

struct AutofillErrorDialogContext;
enum class AutofillProgressDialogType;
class BnplIssuer;
class BnplTosControllerImpl;
struct BnplTosModel;
class ContentAutofillClient;

namespace payments {

struct BnplIssuerContext;
class SelectBnplIssuerDialogControllerImpl;

// Desktop implementation of the BnplUiDelegate interface. This class handles
// the UI for the BNPL autofill flow on the Desktop platform.
class DesktopBnplUiDelegate : public BnplUiDelegate {
 public:
  explicit DesktopBnplUiDelegate(ContentAutofillClient* client);
  DesktopBnplUiDelegate(const DesktopBnplUiDelegate& other) = delete;
  DesktopBnplUiDelegate& operator=(const DesktopBnplUiDelegate& other) = delete;
  ~DesktopBnplUiDelegate() override;

  // BnplUiDelegate:
  void ShowSelectBnplIssuerUi(
      std::vector<BnplIssuerContext> bnpl_issuer_context,
      std::string app_locale,
      base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
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
  const raw_ref<ContentAutofillClient> client_;

  std::unique_ptr<SelectBnplIssuerDialogControllerImpl>
      select_bnpl_issuer_dialog_controller_;

  std::unique_ptr<BnplTosControllerImpl> bnpl_tos_controller_;
};

}  // namespace payments

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_BNPL_UI_DELEGATE_H_
