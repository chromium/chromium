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
  loyalty_card->set_loyalty_card_number(card.loyalty_card_number());
  for (const GURL& merchant_domain : card.merchant_domains()) {
    *loyalty_card->add_merchant_domains() = merchant_domain.spec();
  }

  return specifics;
}

LoyaltyCard CreateAutofillLoyaltyCardFromSpecifics(
    const AutofillValuableSpecifics& specifics) {
  // Since the specifics are guaranteed to be valid by `IsEntityDataValid()`,
  // the conversion will succeed.
  const auto& repeated_domains = specifics.loyalty_card().merchant_domains();
  std::vector<GURL> domains(repeated_domains.begin(), repeated_domains.end());
  return LoyaltyCard(
      ValuableId(specifics.id()), specifics.loyalty_card().merchant_name(),
      specifics.loyalty_card().program_name(),
      GURL(specifics.loyalty_card().program_logo()),
      specifics.loyalty_card().loyalty_card_number(), std::move(domains));
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
  const auto HasEmptyOrValidProgramLogo =
      [](const AutofillValuableSpecifics& specifics) {
        return !specifics.loyalty_card().has_program_logo() ||
               specifics.loyalty_card().program_logo().empty() ||
               GURL(specifics.loyalty_card().program_logo()).is_valid();
      };

  return !specifics.id().empty() && specifics.has_loyalty_card() &&
         !specifics.loyalty_card().loyalty_card_number().empty() &&
         HasEmptyOrValidProgramLogo(specifics);
}

AutofillValuableSpecifics TrimAutofillValuableSpecificsDataForCaching(
    const AutofillValuableSpecifics& specifics) {
  AutofillValuableSpecifics trimmed_specifics =
      AutofillValuableSpecifics(specifics);
  trimmed_specifics.clear_id();
  trimmed_specifics.mutable_loyalty_card()->clear_merchant_name();
  trimmed_specifics.mutable_loyalty_card()->clear_program_name();
  trimmed_specifics.mutable_loyalty_card()->clear_program_logo();
  trimmed_specifics.mutable_loyalty_card()->clear_loyalty_card_number();
  trimmed_specifics.mutable_loyalty_card()->clear_merchant_domains();
  trimmed_specifics.clear_valuable_data();
  return trimmed_specifics;
}
}  // namespace autofill
