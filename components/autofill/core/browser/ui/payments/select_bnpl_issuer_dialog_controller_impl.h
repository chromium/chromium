// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_CONTROLLER_IMPL_H_

#include <vector>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller.h"

namespace autofill::payments {
struct BnplIssuerContext;
class PaymentsAutofillClient;
class SelectBnplIssuerView;
struct TextWithLink;

// Implementation for the SelectBnplIssuerDialogController.
class SelectBnplIssuerDialogControllerImpl
    : public SelectBnplIssuerDialogController {
 public:
  explicit SelectBnplIssuerDialogControllerImpl(PaymentsAutofillClient* client);
  SelectBnplIssuerDialogControllerImpl(
      const SelectBnplIssuerDialogControllerImpl&) = delete;
  SelectBnplIssuerDialogControllerImpl& operator=(
      const SelectBnplIssuerDialogControllerImpl&) = delete;
  ~SelectBnplIssuerDialogControllerImpl() override;

  // Show the dialog with the given issuers.
  void ShowDialog(base::OnceCallback<std::unique_ptr<SelectBnplIssuerView>()>
                      create_and_show_dialog_callback,
                  std::vector<BnplIssuerContext> issuer_contexts,
                  std::string app_locale,
                  base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
                  base::OnceClosure cancel_callback);

  base::WeakPtr<SelectBnplIssuerDialogControllerImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void UpdateDialogWithIssuers(
      std::vector<BnplIssuerContext> issuer_contexts) override;
  void OnIssuerSelected(BnplIssuer issuer) override;
  void OnUserCancelled() override;
  void Dismiss() override;
  const std::vector<BnplIssuerContext>& GetIssuerContexts() const override;
  const std::string& GetAppLocale() const override;
  TextWithLink GetLinkText() const override;
  std::u16string GetTitle() const override;
  std::u16string GetSelectionOptionText(
      autofill::BnplIssuer::IssuerId issuer_id) const override;

 private:
  // The dialog view, managed by the views infrastructure on desktop.
  std::unique_ptr<SelectBnplIssuerView> dialog_view_;

  // List of issuers with their corresponding contexts to be displayed in the
  // selection view.
  std::vector<BnplIssuerContext> issuer_contexts_;

  std::string app_locale_;

  // Callback invoked when the user confirmed an issuer to use.
  // TODO(crbug.com/444684247): Make this a base::RepeatingCallback for the case
  // where the user selects an issuer that is not available after amount
  // extraction runs, so the user must select a different issuer.
  base::OnceCallback<void(BnplIssuer)> selected_issuer_callback_;

  // Callback invoked when the user cancelled the dialog.
  base::OnceClosure cancel_callback_;

  const raw_ref<PaymentsAutofillClient> client_;

  base::WeakPtrFactory<SelectBnplIssuerDialogControllerImpl> weak_ptr_factory_{
      this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_CONTROLLER_IMPL_H_
