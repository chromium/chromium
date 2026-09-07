// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_ENTITY_SUPPRESSION_SYNC_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_ENTITY_SUPPRESSION_SYNC_UTIL_H_

#include <memory>
#include <optional>
#include <string>

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"
#include "components/sync/protocol/autofill_entity_suppression_specifics.pb.h"

namespace syncer {
struct EntityData;
}  // namespace syncer

namespace autofill {

// For a given `guid` and `entry`, returns the corresponding
// `sync_pb::AutofillEntitySuppressionSpecifics`.
sync_pb::AutofillEntitySuppressionSpecifics
CreateSpecificsFromEntitySuppressionEntry(const std::string& guid,
                                          const EntitySuppressionEntry& entry);

// Converts the given `guid` and `entry` into a `syncer::EntityData`.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromEntitySuppressionEntry(
    const std::string& guid,
    const EntitySuppressionEntry& entry);

// Converts the given `specifics` into a `syncer::EntityData`.
std::unique_ptr<syncer::EntityData>
CreateEntityDataFromEntitySuppressionSpecifics(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

// Converts the given `specifics` into an equivalent `EntitySuppressionEntry`.
// Returns `std::nullopt` if the specifics are invalid or contain unknown types.
std::optional<EntitySuppressionEntry> CreateEntitySuppressionEntryFromSpecifics(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

// Tests if the entity suppression `specifics` are valid.
bool AreEntitySuppressionSpecificsValid(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_ENTITY_SUPPRESSION_SYNC_UTIL_H_
