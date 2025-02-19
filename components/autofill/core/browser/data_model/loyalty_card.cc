// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/loyalty_card.h"

namespace autofill {

LoyaltyCard::LoyaltyCard(std::string loyalty_card_id,
                         std::string merchant_name,
                         std::string program_name,
                         std::string program_logo,
                         std::string loyalty_card_number)
    : loyalty_card_id(std::move(loyalty_card_id)),
      merchant_name(std::move(merchant_name)),
      program_name(std::move(program_name)),
      program_logo(std::move(program_logo)),
      loyalty_card_number(std::move(loyalty_card_number)) {}

LoyaltyCard::LoyaltyCard(const LoyaltyCard&) = default;
LoyaltyCard::LoyaltyCard(LoyaltyCard&&) = default;

LoyaltyCard::~LoyaltyCard() = default;

}  // namespace autofill
