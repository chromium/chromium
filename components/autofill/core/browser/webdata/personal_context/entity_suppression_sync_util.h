// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_ENTITY_SUPPRESSION_SYNC_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_ENTITY_SUPPRESSION_SYNC_UTIL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"
#include "components/sync/protocol/autofill_entity_suppression_specifics.pb.h"

namespace syncer {
struct EntityData;
}  // namespace syncer

namespace autofill {

// For a given `guid`, `suppression_key`, and `creation_time`, returns the
// corresponding `sync_pb::AutofillEntitySuppressionSpecifics` merged with
// `base_specifics`.
sync_pb::AutofillEntitySuppressionSpecifics
CreateSpecificsFromEntitySuppressionKey(
    const std::string& guid,
    const EntitySuppressionKey& suppression_key,
    base::Time creation_time,
    const sync_pb::AutofillEntitySuppressionSpecifics& base_specifics);

// Converts the given `guid`, `suppression_key`, and `creation_time` into a
// `syncer::EntityData` merged with `base_specifics`.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromEntitySuppressionKey(
    const std::string& guid,
    const EntitySuppressionKey& suppression_key,
    base::Time creation_time,
    const sync_pb::AutofillEntitySuppressionSpecifics& base_specifics);

// Converts the given `specifics` into a `syncer::EntityData`.
std::unique_ptr<syncer::EntityData>
CreateEntityDataFromEntitySuppressionSpecifics(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

// Converts the given `specifics` into an equivalent `EntitySuppressionKey`.
// Returns `std::nullopt` if the specifics are invalid or contain unknown types.
std::optional<EntitySuppressionKey> CreateEntitySuppressionKeyFromSpecifics(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

// Tests if the entity suppression `specifics` are valid.
bool AreEntitySuppressionSpecificsValid(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

// Clears all supported fields from `specifics`. Supported fields are all
// fields in the protobuf definition that have already been included in the
// client version.
sync_pb::AutofillEntitySuppressionSpecifics
TrimAutofillEntitySuppressionSpecificsDataForCaching(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_ENTITY_SUPPRESSION_SYNC_UTIL_H_
