// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/passes/loyalty_card.h"

namespace autofill {

LoyaltyCard::LoyaltyCard(std::string loyalty_card_id,
                         std::string merchant_name,
                         std::string program_name,
                         GURL program_logo,
                         std::string unmasked_loyalty_card_suffix)
    : loyalty_card_id(std::move(loyalty_card_id)),
      merchant_name(std::move(merchant_name)),
      program_name(std::move(program_name)),
      program_logo(std::move(program_logo)),
      unmasked_loyalty_card_suffix(std::move(unmasked_loyalty_card_suffix)) {}

LoyaltyCard::LoyaltyCard(const LoyaltyCard&) = default;
LoyaltyCard::LoyaltyCard(LoyaltyCard&&) = default;

LoyaltyCard::~LoyaltyCard() = default;

bool LoyaltyCard::IsValid() const {
  return !loyalty_card_id.empty() &&
         (program_logo.is_empty() || program_logo.is_valid());
}

}  // namespace autofill
