// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_utils/passes_data_test_utils.h"

#include "components/autofill/core/browser/data_model/loyalty_card.h"

namespace autofill::test {

LoyaltyCard CreateLoyaltyCard() {
  return LoyaltyCard(
      /*loyalty_card_id=*/"loyalty_card_id_1",
      /*merchant_name=*/"Deutsche Bahn", /*program_name=*/"BahnBonus",
      /*program_logo=*/"https://empty.url.com", /*loyalty_card_number=*/"1234");
}

LoyaltyCard CreateLoyaltyCard2() {
  return LoyaltyCard(/*loyalty_card_id=*/"loyalty_card_id_2",
                     /*merchant_name=*/"Lidl", /*program_name=*/"CustomerCard",
                     /*program_logo=*/"https://empty.url.com",
                     /*loyalty_card_number=*/"4321");
}

}  // namespace autofill::test
