// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PASSES_LOYALTY_CARD_SYNC_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PASSES_LOYALTY_CARD_SYNC_UTIL_H_

#include "components/autofill/core/browser/data_model/passes/loyalty_card.h"
#include "components/sync/protocol/autofill_loyalty_card_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"

namespace autofill {
// For a given `LoyaltyCard`, returns the corresponding
// `sync_pb::AutofillLoyaltyCardSpecifics`.
sync_pb::AutofillLoyaltyCardSpecifics CreateSpecificsFromLoyaltyCard(
    const LoyaltyCard& card);

// Converts the given `loyaltyCard` into a `syncer::EntityData`.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromLoyaltyCard(
    const LoyaltyCard& loyalty_card);

// Converts the given loyalty card `specifics` into an equivalent
// `LoyaltyCard` or returns `nullopt` if specifics are invalid.
std::optional<LoyaltyCard> CreateAutofillLoyaltyCardFromSpecifics(
    const sync_pb::AutofillLoyaltyCardSpecifics& specifics);

// Tests if the loyalty card `specifics` are valid and can be converted into an
// `LoyaltyCard` using `CreateAutofillLoyaltyCardFromSpecifics()`.
bool AreAutofillLoyaltyCardSpecificsValid(
    const sync_pb::AutofillLoyaltyCardSpecifics& specifics);

// Clears all supported fields from `specifics`. Supported
// fields are all fields in the protobuf definition that have already been
// included in the client version.
sync_pb::AutofillLoyaltyCardSpecifics TrimLoyaltyCardSpecificsDataForCaching(
    const sync_pb::AutofillLoyaltyCardSpecifics& specifics);
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PASSES_LOYALTY_CARD_SYNC_UTIL_H_
