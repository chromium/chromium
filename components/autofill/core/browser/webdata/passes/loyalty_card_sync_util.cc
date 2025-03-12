// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/passes/loyalty_card_sync_util.h"

#include "base/uuid.h"
#include "components/sync/protocol/autofill_loyalty_card_specifics.pb.h"
#include "url/gurl.h"

namespace autofill {

using sync_pb::AutofillLoyaltyCardSpecifics;

AutofillLoyaltyCardSpecifics CreateSpecificsFromLoyaltyCard(
    const LoyaltyCard& card) {
  AutofillLoyaltyCardSpecifics specifics =
      sync_pb::AutofillLoyaltyCardSpecifics();
  specifics.set_id(card.loyalty_card_id);
  specifics.set_merchant_name(card.merchant_name);
  specifics.set_program_name(card.program_name);
  specifics.set_program_logo(card.program_logo.possibly_invalid_spec());
  specifics.set_loyalty_card_suffix(card.unmasked_loyalty_card_suffix);
  return specifics;
}

bool AreAutofillLoyaltyCardSpecificsValid(
    const AutofillLoyaltyCardSpecifics& specifics) {
  return !specifics.id().empty() && GURL(specifics.program_logo()).is_valid();
}

std::optional<LoyaltyCard> CreateAutofillLoyaltyCardFromSpecifics(
    const AutofillLoyaltyCardSpecifics& specifics) {
  if (!AreAutofillLoyaltyCardSpecificsValid(specifics)) {
    return std::nullopt;
  }
  return LoyaltyCard(specifics.id(), specifics.merchant_name(),
                     specifics.program_name(), GURL(specifics.program_logo()),
                     specifics.loyalty_card_suffix());
}

std::unique_ptr<syncer::EntityData> CreateEntityDataFromLoyaltyCard(
    const LoyaltyCard& loyalty_card) {
  AutofillLoyaltyCardSpecifics card_specifics =
      CreateSpecificsFromLoyaltyCard(loyalty_card);
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();

  entity_data->name = card_specifics.id();
  AutofillLoyaltyCardSpecifics* specifics =
      entity_data->specifics.mutable_autofill_loyalty_card();
  specifics->CopyFrom(card_specifics);

  return entity_data;
}

AutofillLoyaltyCardSpecifics TrimLoyaltyCardSpecificsDataForCaching(
    const AutofillLoyaltyCardSpecifics& specifics) {
  AutofillLoyaltyCardSpecifics trimmed_specifics =
      AutofillLoyaltyCardSpecifics(specifics);
  trimmed_specifics.clear_id();
  trimmed_specifics.clear_merchant_name();
  trimmed_specifics.clear_program_name();
  trimmed_specifics.clear_program_logo();
  trimmed_specifics.clear_loyalty_card_suffix();
  return trimmed_specifics;
}
}  // namespace autofill
