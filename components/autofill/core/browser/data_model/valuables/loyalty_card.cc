// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"

#include <string>

#include "components/autofill/core/browser/data_model/valuables/valuable_types.h"

namespace autofill {

LoyaltyCard::LoyaltyCard(ValuableId id,
                         std::string merchant_name,
                         std::string program_name,
                         GURL program_logo,
                         std::string loyalty_card_number,
                         std::vector<GURL> merchant_domains)
    : id_(std::move(id)),
      merchant_name_(std::move(merchant_name)),
      program_name_(std::move(program_name)),
      program_logo_(std::move(program_logo)),
      loyalty_card_number_(std::move(loyalty_card_number)),
      merchant_domains_(std::move(merchant_domains)) {}

LoyaltyCard::LoyaltyCard(const LoyaltyCard&) = default;
LoyaltyCard::LoyaltyCard(LoyaltyCard&&) = default;

LoyaltyCard::~LoyaltyCard() = default;

bool LoyaltyCard::IsValid() const {
  return !id_->empty() && !loyalty_card_number_.empty() &&
         (program_logo_.is_empty() || program_logo_.is_valid());
}

}  // namespace autofill
