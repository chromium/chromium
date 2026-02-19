// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_UTIL_H_

#include <string>

#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
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
  kTemporarilyEligibleCheckoutAmountNotYetKnown = 5,
  kMaxValue = kTemporarilyEligibleCheckoutAmountNotYetKnown
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
  gfx::Range bold_range;
  gfx::Range offset;
  GURL url;
};

// BnplTosModel holds the data required to show the BNPL ToS view.
struct BnplTosModel {
  BnplTosModel();
  BnplTosModel(const BnplTosModel& other);
  BnplTosModel(BnplTosModel&& other);
  BnplTosModel& operator=(const BnplTosModel& other);
  BnplTosModel& operator=(BnplTosModel&& other);
  ~BnplTosModel();
  bool operator==(const BnplTosModel&) const;

  // Used to show the BNPL Issuer logo and name.
  BnplIssuer issuer;
  // Used to show the legal message.
  LegalMessageLines legal_message_lines;
};

// Return all BNPL Issuer contexts including eligibility in order of:
// eligible + linked, eligible + unlinked, uneligible + linked,
// uneligible + unlinked.
std::vector<BnplIssuerContext> GetSortedBnplIssuerContext(
    const AutofillClient& client,
    std::optional<int64_t> checkout_amount);

// Returns the appropriate suggestion icon based on the issuer and its link
// status.
Suggestion::Icon GetBnplSuggestionIcon(BnplIssuer::IssuerId issuer_id,
                                       bool is_linked);

// Returns the selection option text for a given BNPL issuer.
std::u16string GetBnplIssuerSelectionOptionText(
    BnplIssuer::IssuerId issuer_id,
    const std::string& app_locale,
    base::span<const BnplIssuerContext> issuer_contexts);

// Returns the footer text to be displayed in a BNPL flow.
TextWithLink GetBnplUiFooterText();

// Returns the footer text to be displayed in a BNPL flow with AI-based amount
// extraction.
TextWithLink GetBnplUiFooterTextForAi(
    const PaymentsDataManager& payments_data_manager);

// Returns true if the user has initiated an action on the credit card form
// and the current context meets all conditions for BNPL eligibility to be
// shown.
bool ShouldShowBnplSuggestions(const AutofillClient& client,
                               FieldType trigger_field_type);

// Determines if autofill BNPL is supported.
// Returns true if:
// 1. The profile is not off the record.
// 2. The `client` has an `AutofillOptimizationGuideDecider` assigned.
// 3. The URL being visited is within the BNPL issuer allowlist.
bool IsEligibleForBnpl(const AutofillClient& client);

// Determines if Pay Later tab should open initially with the loading spinner to
// indicate that amount extraction is in progress. Returns true if the AI terms
// have been seen by the user before.
bool ShouldStartPayLaterWithLoadingSpinner(
    const PaymentsDataManager& payments_data_manager);

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_UTIL_H_
