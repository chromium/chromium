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

class SelectBnplIssuerView;

// Implementation for the SelectBnplIssuerDialogController.
class SelectBnplIssuerDialogControllerImpl
    : public SelectBnplIssuerDialogController {
 public:
  SelectBnplIssuerDialogControllerImpl(
      std::vector<BnplIssuer> issuers,
      base::OnceCallback<void(const std::string&)> selected_issuer_callback,
      base::OnceClosure cancel_callback);
  SelectBnplIssuerDialogControllerImpl(
      const SelectBnplIssuerDialogControllerImpl&) = delete;
  SelectBnplIssuerDialogControllerImpl& operator=(
      const SelectBnplIssuerDialogControllerImpl&) = delete;
  ~SelectBnplIssuerDialogControllerImpl() override;

  // Show the dialog with the given issuers.
  void ShowDialog(base::OnceCallback<std::unique_ptr<SelectBnplIssuerView>()>
                      create_and_show_dialog_callback);

  base::WeakPtr<SelectBnplIssuerDialogControllerImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // SelectBnplIssuerDialogController:
  void OnAccepted(const std::string& issuer_id) override;
  void OnCancel() override;
  void OnDialogClosed() override;
  const std::vector<BnplIssuer>& GetIssuers() const override;

 private:
  // The dialog view, managed by the views infrastructure on desktop.
  std::unique_ptr<SelectBnplIssuerView> dialog_view_;

  // List of issuers to be displayed in the selection view.
  std::vector<BnplIssuer> issuers_;

  // Callback invoked when the user confirmed an issuer to use.
  base::OnceCallback<void(const std::string&)> selected_issuer_callback_;

  // Callback invoked when the user cancelled the dialog.
  base::OnceClosure cancel_callback_;

  base::WeakPtrFactory<SelectBnplIssuerDialogControllerImpl> weak_ptr_factory_{
      this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_CONTROLLER_IMPL_H_
