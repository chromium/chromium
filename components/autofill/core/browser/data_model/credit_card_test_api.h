// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill {

// Exposes some testing operations for CreditCard.
class CreditCardTestApi {
 public:
  explicit CreditCardTestApi(CreditCard* creditcard) : creditcard_(creditcard) {
    DCHECK(creditcard_);
  }
  void set_network_for_virtual_card(base::StringPiece network) {
    DCHECK_EQ(CreditCard::VIRTUAL_CARD, creditcard_->record_type());
    creditcard_->network_ = std::string(network);
  }

 private:
  raw_ptr<CreditCard> creditcard_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_CREDIT_CARD_TEST_API_H_