// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"

#include "base/uuid.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"
#include "url/gurl.h"

namespace autofill {

using sync_pb::AutofillValuableSpecifics;

AutofillValuableSpecifics CreateSpecificsFromLoyaltyCard(
    const LoyaltyCard& card) {
  AutofillValuableSpecifics specifics = sync_pb::AutofillValuableSpecifics();
  specifics.set_id(card.id().value());
  sync_pb::AutofillValuableSpecifics::LoyaltyCard* loyalty_card =
      specifics.mutable_loyalty_card();
  loyalty_card->set_merchant_name(card.merchant_name());
  loyalty_card->set_program_name(card.program_name());
  loyalty_card->set_program_logo(card.program_logo().possibly_invalid_spec());
  loyalty_card->set_loyalty_card_suffix(card.loyalty_card_suffix());
  return specifics;
}

std::optional<LoyaltyCard> CreateAutofillLoyaltyCardFromSpecifics(
    const AutofillValuableSpecifics& specifics) {
  if (!AreAutofillLoyaltyCardSpecificsValid(specifics)) {
    return std::nullopt;
  }
  return LoyaltyCard(ValuableId(specifics.id()),
                     specifics.loyalty_card().merchant_name(),
                     specifics.loyalty_card().program_name(),
                     GURL(specifics.loyalty_card().program_logo()),
                     specifics.loyalty_card().loyalty_card_suffix());
}

std::unique_ptr<syncer::EntityData> CreateEntityDataFromLoyaltyCard(
    const LoyaltyCard& loyalty_card) {
  AutofillValuableSpecifics card_specifics =
      CreateSpecificsFromLoyaltyCard(loyalty_card);
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();

  entity_data->name = card_specifics.id();
  AutofillValuableSpecifics* specifics =
      entity_data->specifics.mutable_autofill_valuable();
  specifics->CopyFrom(card_specifics);

  return entity_data;
}

bool AreAutofillLoyaltyCardSpecificsValid(
    const AutofillValuableSpecifics& specifics) {
  return !specifics.id().empty() && specifics.has_loyalty_card() &&
         GURL(specifics.loyalty_card().program_logo()).is_valid();
}

AutofillValuableSpecifics TrimAutofillValuableSpecificsDataForCaching(
    const AutofillValuableSpecifics& specifics) {
  AutofillValuableSpecifics trimmed_specifics =
      AutofillValuableSpecifics(specifics);
  trimmed_specifics.clear_id();
  trimmed_specifics.mutable_loyalty_card()->clear_merchant_name();
  trimmed_specifics.mutable_loyalty_card()->clear_program_name();
  trimmed_specifics.mutable_loyalty_card()->clear_program_logo();
  trimmed_specifics.mutable_loyalty_card()->clear_loyalty_card_suffix();
  trimmed_specifics.clear_valuable_data();
  return trimmed_specifics;
}
}  // namespace autofill
