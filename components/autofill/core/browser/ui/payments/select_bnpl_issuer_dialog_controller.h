// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_CONTROLLER_H_

#include <vector>

#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"

namespace autofill::payments {

struct BnplIssuerContext;
struct TextWithLink;

// Interface that exposes controller functionality to the
// SelectBnplIssuerDialogView.
class SelectBnplIssuerDialogController {
 public:
  // Method called by BnplManager when issuer data is ready to dismiss the
  // throbber and show the issuer dialog.
  virtual void UpdateDialogWithIssuers(
      std::vector<BnplIssuerContext> issuer_contexts) = 0;

  // Callbacks for the View. When an issuer is selected, it passes the
  // issuer that was selected by the user.
  virtual void OnIssuerSelected(BnplIssuer issuer) = 0;
  virtual void OnUserCancelled() = 0;
  virtual void Dismiss() = 0;
  virtual TextWithLink GetLinkText() const = 0;
  virtual std::u16string GetTitle() const = 0;
  virtual std::u16string GetSelectionOptionText(
      autofill::BnplIssuer::IssuerId issuer_id) const = 0;

  // List of issuers with their corresponding contexts to be displayed on the
  // Select BNPL issuer dialog.
  virtual const std::vector<BnplIssuerContext>& GetIssuerContexts() const = 0;
  virtual const std::string& GetAppLocale() const = 0;

 protected:
  virtual ~SelectBnplIssuerDialogController() = default;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_CONTROLLER_H_
