// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_SELECT_BNPL_ISSUER_DIALOG_CONTROLLER_H_

#include <vector>

#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "ui/gfx/range/range.h"

namespace autofill::payments {

// Contains a string of text and the location of a substring for a link.
struct TextWithLink {
  std::u16string text;
  gfx::Range offset;
};

// An enum indicating the elgibility of a BNPL issuer on the current page for
// the Select BNPL issuer dialog.
enum class BnplIssuerEligibilityForPage {
  kUndefined = 0,
  kIsEligible = 1,
  // Note: If an issuer is not eligible due to checkout amount and lack of
  // merchant support, then lack of merchant support takes precedence.
  kNotEligibleIssuerDoesNotSupportMerchant = 2,
  kNotEligibleCheckoutAmountTooLow = 3,
  kNotEligibleCheckoutAmountTooHigh = 4,
  kMaxValue = kNotEligibleCheckoutAmountTooHigh
};

// A struct containing a BNPL issuer and the context necessary to display it in
// the Select BNPL issuer dialog.
struct BnplIssuerContext {
 public:
  BnplIssuerContext();
  BnplIssuerContext(BnplIssuer issuer,
                    BnplIssuerEligibilityForPage eligibility);
  BnplIssuerContext(const BnplIssuerContext& other);
  BnplIssuerContext(BnplIssuerContext&&);
  BnplIssuerContext& operator=(const BnplIssuerContext& other);
  BnplIssuerContext& operator=(BnplIssuerContext&&);
  ~BnplIssuerContext();
  bool operator==(const BnplIssuerContext&) const;

  // The BNPL issuer to display in the dialog.
  BnplIssuer issuer;

  // The eligibility of the BNPL issuer on the current page for the Select BNPL
  // issuer dialog.
  BnplIssuerEligibilityForPage eligibility =
      BnplIssuerEligibilityForPage::kUndefined;
};

// Interface that exposes controller functionality to the
// SelectBnplIssuerDialogView.
class SelectBnplIssuerDialogController {
 public:
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
