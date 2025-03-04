// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/notreached.h"
#include "components/autofill/core/browser/data_model/payments/payment_instrument.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

BnplIssuer::EligiblePriceRange::EligiblePriceRange(
    const BnplIssuer::EligiblePriceRange&) = default;

BnplIssuer::EligiblePriceRange& BnplIssuer::EligiblePriceRange::operator=(
    const BnplIssuer::EligiblePriceRange&) = default;

BnplIssuer::EligiblePriceRange::EligiblePriceRange(
    BnplIssuer::EligiblePriceRange&&) = default;

BnplIssuer::EligiblePriceRange& BnplIssuer::EligiblePriceRange::operator=(
    BnplIssuer::EligiblePriceRange&&) = default;

BnplIssuer::EligiblePriceRange::~EligiblePriceRange() = default;

bool BnplIssuer::EligiblePriceRange::operator==(
    const BnplIssuer::EligiblePriceRange&) const = default;

bool operator==(const BnplIssuer& a, const BnplIssuer& b) = default;

BnplIssuer::BnplIssuer(std::optional<int64_t> instrument_id,
                       std::string issuer_id,
                       std::vector<EligiblePriceRange> eligible_price_ranges)
    : issuer_id_(std::move(issuer_id)),
      payment_instrument_(
          instrument_id.has_value()
              ? std::make_optional<PaymentInstrument>(
                    instrument_id.value(),
                    u"",
                    GURL(),
                    DenseSet({PaymentInstrument::PaymentRail::kCardNumber}))
              : std::nullopt),
      eligible_price_ranges_(std::move(eligible_price_ranges)) {}

BnplIssuer::BnplIssuer(const BnplIssuer&) = default;

BnplIssuer& BnplIssuer::operator=(const BnplIssuer&) = default;

BnplIssuer::BnplIssuer(BnplIssuer&&) = default;

BnplIssuer& BnplIssuer::operator=(BnplIssuer&&) = default;

BnplIssuer::~BnplIssuer() = default;

base::optional_ref<const BnplIssuer::EligiblePriceRange>
BnplIssuer::GetEligiblePriceRangeForCurrency(
    const std::string& currency) const {
  for (const EligiblePriceRange& range : eligible_price_ranges_) {
    if (range.currency == currency) {
      return range;
    }
  }
  return std::nullopt;
}

bool BnplIssuer::IsEligibleAmount(uint64_t amount_in_micros,
                                  const std::string& currency) const {
  base::optional_ref<const EligiblePriceRange> range =
      GetEligiblePriceRangeForCurrency(currency);
  return range.has_value() && amount_in_micros >= range->price_lower_bound &&
         amount_in_micros <= range->price_upper_bound;
}

std::u16string BnplIssuer::GetDisplayName() const {
  if (issuer_id_ == kBnplAffirmIssuerId) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_AFFIRM);
  }
  if (issuer_id_ == kBnplZipIssuerId) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_ZIP);
  }
  if (issuer_id_ == kBnplAfterpayIssuerId) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_AFTER_PAY);
  }
  NOTREACHED() << "Unknown issuer_id_ " << issuer_id_;
}

}  // namespace autofill
