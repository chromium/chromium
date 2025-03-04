// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_CONTROLLER_H_

#include <vector>

#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"

namespace autofill::payments {

// Interface that exposes controller functionality to the
// SelectBnplIssuerDialogView.
class SelectBnplIssuerDialogController {
 public:
  // Callbacks for the View. When the dialog is accepted, it passes the ID of
  // the issuer that was selected by the user.
  virtual void OnAccepted(const std::string& issuer_id) = 0;
  virtual void OnCancel() = 0;
  virtual void OnDialogClosed() = 0;

  // List of issuers to be displayed.
  virtual const std::vector<BnplIssuer>& GetIssuers() const = 0;

 protected:
  virtual ~SelectBnplIssuerDialogController() = default;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_CONTROLLER_H_
