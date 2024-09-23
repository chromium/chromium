// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_TEST_API_H_

#include <string_view>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill {

// Exposes some testing operations for CreditCard.
class CreditCardTestApi {
 public:
  explicit CreditCardTestApi(CreditCard* credit_card)
      : credit_card_(*credit_card) {}

  void set_issuer_id_for_card(std::string_view network) {
    credit_card_->issuer_id_ = std::string(network);
  }

  void set_network_for_card(std::string_view network) {
    credit_card_->network_ = std::string(network);
  }

 private:
  const raw_ref<CreditCard> credit_card_;
};

inline CreditCardTestApi test_api(CreditCard& credit_card) {
  return CreditCardTestApi(&credit_card);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_TEST_API_H_
