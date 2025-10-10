// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_UTIL_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/field_types.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace autofill {

class AutofillClient;

namespace payments {

// An enum indicating the eligibility of a BNPL issuer on the current page.
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

// A struct containing a BNPL issuer and the context necessary to display it.
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

  // Returns the eligibility based on the `BnplIssuerEligibilityForPage`.
  bool IsEligible() const;

  // The BNPL issuer to display.
  BnplIssuer issuer;

  // The eligibility of the BNPL issuer on the current page.
  BnplIssuerEligibilityForPage eligibility =
      BnplIssuerEligibilityForPage::kUndefined;
};

// Contains a string of text and the location of a substring for a link.
struct TextWithLink {
  std::u16string text;
  gfx::Range offset;
  GURL url;
};

// A struct containing a BNPL ToS info to be shown on the bottomsheet screen.
struct BnplIssuerTosDetail {
 public:
  BnplIssuerTosDetail(std::u16string review_text,
                      std::u16string approve_text,
                      TextWithLink link_text);
  BnplIssuerTosDetail(const BnplIssuerTosDetail& other);
  BnplIssuerTosDetail(BnplIssuerTosDetail&&);
  BnplIssuerTosDetail& operator=(const BnplIssuerTosDetail& other);
  BnplIssuerTosDetail& operator=(BnplIssuerTosDetail&&);
  ~BnplIssuerTosDetail();

  // Sign-in/create account message.
  std::u16string review_text;

  // Eligibility check message.
  std::u16string approve_text;

  // Account link/unlink message.
  TextWithLink link_text;
};

// Returns the selection option text for a given BNPL issuer.
std::u16string GetBnplIssuerSelectionOptionText(
    BnplIssuer::IssuerId issuer_id,
    const std::string& app_locale,
    base::span<const BnplIssuerContext> issuer_contexts);

// Returns the footer text to be displayed in a BNPL flow.
TextWithLink GetBnplUiFooterText();

// Returns true if the user has initiated an action on the credit card form
// and the current context meets all conditions for BNPL eligibility to be
// shown.
bool ShouldAppendBnplSuggestion(const AutofillClient& client,
                                bool is_card_number_field_empty,
                                FieldType trigger_field_type);

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_UTIL_H_
