// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PASSES_LOYALTY_CARD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PASSES_LOYALTY_CARD_H_

#include <string>

#include "url/gurl.h"

namespace autofill {

// Represents a loaylty card coming from the Google Wallet.
class LoyaltyCard final {
 public:
  LoyaltyCard(std::string loyalty_card_id,
              std::string merchant_name,
              std::string program_name,
              GURL program_logo,
              std::string unmasked_loyalty_card_suffix);

  LoyaltyCard(const LoyaltyCard&);
  LoyaltyCard(LoyaltyCard&&);
  LoyaltyCard& operator=(const LoyaltyCard&) = default;
  LoyaltyCard& operator=(LoyaltyCard&&) = default;

  ~LoyaltyCard();

  // Checks if this loyalty card is valid. A valid loyalty card contains a
  // non-empty loyalty card id and a logo URL which should be either empty or
  // valid.
  bool IsValid() const;

  friend bool operator==(const LoyaltyCard&, const LoyaltyCard&) = default;

  // A unique identifier coming from the server, which is used as a primary key
  // for storing loyalty cards in the database.
  std::string loyalty_card_id;
  // The merchant name e.g. "Deutsche Bahn".
  std::string merchant_name;
  // The loyalty card program name e.g. "BahnBonus".
  std::string program_name;
  // The logo icon URL.
  GURL program_logo;
  // The unmasked part of the  loyalty card issuer text code. The full number is
  // not available on the client.
  std::string unmasked_loyalty_card_suffix;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PASSES_LOYALTY_CARD_H_
