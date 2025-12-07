// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuables_sync_test_utils.h"

#include <string>

#include "components/autofill/core/browser/data_model/valuables/valuable_types.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"
#include "url/gurl.h"

namespace autofill {

LoyaltyCard TestLoyaltyCard(std::string_view id) {
  return LoyaltyCard(ValuableId(std::string(id)), "merchant_name",
                     "program_name", GURL("http://foobar.com/logo.png"),
                     "80974934820245", {GURL("https://domain.example")});
}

sync_pb::AutofillValuableSpecifics TestLoyaltyCardSpecifics(
    std::string_view id,
    std::string_view program_logo,
    std::string_view number) {
  sync_pb::AutofillValuableSpecifics specifics =
      sync_pb::AutofillValuableSpecifics();
  specifics.set_id(std::string(id));

  sync_pb::LoyaltyCard* loyalty_card = specifics.mutable_loyalty_card();
  loyalty_card->set_merchant_name("merchant_name");
  loyalty_card->set_program_name("program_name");
  loyalty_card->set_program_logo(std::string(program_logo));
  loyalty_card->set_loyalty_card_number(number);
  *loyalty_card->add_merchant_domains() = "https://domain.example";
  return specifics;
}

}  // namespace autofill
