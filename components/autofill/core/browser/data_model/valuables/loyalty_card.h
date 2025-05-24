// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_VALUABLES_LOYALTY_CARD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_VALUABLES_LOYALTY_CARD_H_

#include <string>

#include "components/autofill/core/browser/data_model/valuables/valuable_types.h"
#include "url/gurl.h"

namespace autofill {

// Represents a loyalty card coming from the Google Wallet.
class LoyaltyCard final {
 public:
  LoyaltyCard(ValuableId loyalty_card_id,
              std::string merchant_name,
              std::string program_name,
              GURL program_logo,
              std::string loyalty_card_number,
              std::vector<GURL> merchant_domains);

  LoyaltyCard(const LoyaltyCard&);
  LoyaltyCard(LoyaltyCard&&);
  LoyaltyCard& operator=(const LoyaltyCard&) = default;
  LoyaltyCard& operator=(LoyaltyCard&&) = default;

  ~LoyaltyCard();

  const ValuableId& id() const { return id_; }
  void set_id(const ValuableId& id) { id_ = id; }

  const std::string& merchant_name() const { return merchant_name_; }
  void set_merchant_name(const std::string& merchant_name) {
    merchant_name_ = merchant_name;
  }

  const std::string& program_name() const { return program_name_; }
  void set_program_name(const std::string& program_name) {
    program_name_ = program_name;
  }

  const GURL& program_logo() const { return program_logo_; }
  void set_program_logo(const GURL& program_logo) {
    program_logo_ = program_logo;
  }

  const std::string& loyalty_card_number() const {
    return loyalty_card_number_;
  }
  void set_loyalty_card_number(const std::string& loyalty_card_number) {
    loyalty_card_number_ = loyalty_card_number;
  }

  const std::vector<GURL>& merchant_domains() const {
    return merchant_domains_;
  }
  void set_merchant_domains(std::vector<GURL> merchant_domains) {
    merchant_domains_ = std::move(merchant_domains);
  }

  // Checks if this loyalty card is valid. A valid loyalty card contains a
  // non-empty loyalty card id and a logo URL which should be either empty or
  // valid.
  bool IsValid() const;

  friend bool operator==(const LoyaltyCard&, const LoyaltyCard&) = default;
  friend auto operator<=>(const LoyaltyCard&, const LoyaltyCard&) = default;

 private:
  // A unique identifier coming from the server, which is used as a primary key
  // for storing loyalty cards in the database.
  ValuableId id_;

  // The merchant name e.g. "Deutsche Bahn".
  std::string merchant_name_;

  // The loyalty card program name e.g. "BahnBonus".
  std::string program_name_;

  // The logo icon URL.
  GURL program_logo_;

  // The loyalty card text code.
  std::string loyalty_card_number_;

  // The list of merchant domains associated to this card.
  std::vector<GURL> merchant_domains_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_VALUABLES_LOYALTY_CARD_H_
