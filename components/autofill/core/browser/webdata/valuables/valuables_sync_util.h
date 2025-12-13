// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLES_SYNC_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLES_SYNC_UTIL_H_

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"

namespace autofill {
// For a given `LoyaltyCard`, returns the corresponding
// `sync_pb::AutofillValuableSpecifics`.
sync_pb::AutofillValuableSpecifics CreateSpecificsFromLoyaltyCard(
    const LoyaltyCard& card);

// Converts the given `loyalty_card` into a `syncer::EntityData`.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromLoyaltyCard(
    const LoyaltyCard& loyalty_card);

// Converts the given `entity` into a `syncer::EntityData`.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromEntityInstance(
    EntityInstance entity);

// Converts the given valuable `specifics` into an equivalent LoyaltyCard
// instance.
LoyaltyCard CreateAutofillLoyaltyCardFromSpecifics(
    const sync_pb::AutofillValuableSpecifics& specifics);

// Clears all supported fields from `specifics`. Supported
// fields are all fields in the protobuf definition that have already been
// included in the client version.
sync_pb::AutofillValuableSpecifics TrimAutofillValuableSpecificsDataForCaching(
    const sync_pb::AutofillValuableSpecifics& specifics);

// Clears all supported fields from `specifics`. Supported
// fields are all fields in the protobuf definition that have already been
// included in the client version.
sync_pb::AutofillValuableMetadataSpecifics
TrimAutofillValuableMetadataSpecificsDataForCaching(
    const sync_pb::AutofillValuableMetadataSpecifics& specifics);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLES_SYNC_UTIL_H_
