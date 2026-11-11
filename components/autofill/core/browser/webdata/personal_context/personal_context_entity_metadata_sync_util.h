// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_PERSONAL_CONTEXT_ENTITY_METADATA_SYNC_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_PERSONAL_CONTEXT_ENTITY_METADATA_SYNC_UTIL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"
#include "components/sync/protocol/autofill_entity_suppression_specifics.pb.h"

namespace syncer {
struct EntityData;
}  // namespace syncer

namespace autofill {

// For a given `guid`, `suppression_hash`, and `creation_time`, returns the
// corresponding `sync_pb::AutofillEntitySuppressionSpecifics` merged with
// `base_specifics`.
sync_pb::AutofillEntitySuppressionSpecifics
CreateSpecificsFromEntitySuppressionHash(
    const std::string& guid,
    EntitySuppressionHash suppression_hash,
    base::Time creation_time,
    const sync_pb::AutofillEntitySuppressionSpecifics& base_specifics);

// Converts the given `guid`, `suppression_hash`, and `creation_time` into a
// `syncer::EntityData` merged with `base_specifics`.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromEntitySuppressionHash(
    const std::string& guid,
    EntitySuppressionHash suppression_hash,
    base::Time creation_time,
    const sync_pb::AutofillEntitySuppressionSpecifics& base_specifics);

// For a given `guid`, `confirmation_hash`, and `creation_time`, returns the
// corresponding `sync_pb::AutofillEntitySuppressionSpecifics` merged with
// `base_specifics`.
sync_pb::AutofillEntitySuppressionSpecifics
CreateSpecificsFromEntityConfirmationHash(
    std::string_view guid,
    EntityConfirmationHash confirmation_hash,
    base::Time creation_time,
    const sync_pb::AutofillEntitySuppressionSpecifics& base_specifics);

// Converts the given `guid`, `confirmation_hash`, and `creation_time` into a
// `syncer::EntityData` merged with `base_specifics`.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromEntityConfirmationHash(
    std::string_view guid,
    EntityConfirmationHash confirmation_hash,
    base::Time creation_time,
    const sync_pb::AutofillEntitySuppressionSpecifics& base_specifics);

// Converts the given `specifics` into a `syncer::EntityData`.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromSpecifics(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

// Converts the given `specifics` into an equivalent `EntitySuppressionHash`.
// Returns `std::nullopt` if the specifics are invalid, not a suppression, or
// contain unknown types.
std::optional<EntitySuppressionHash> CreateEntitySuppressionHashFromSpecifics(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

// Converts the given `specifics` into an equivalent `EntityConfirmationHash`.
// Returns `std::nullopt` if the specifics are invalid or not a confirmation.
std::optional<EntityConfirmationHash> CreateEntityConfirmationHashFromSpecifics(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

// Tests if the entity suppression `specifics` are valid.
bool AreEntitySuppressionSpecificsValid(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

// Tests if the entity confirmation `specifics` are valid.
bool AreEntityConfirmationSpecificsValid(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

// Returns a copy of `specifics` with `suppression.hash` and
// `entity_suppression_hash` populated (preserving `entity_suppression_key` if
// present) if `suppression` is not yet set, or `std::nullopt` if no migration
// is needed.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
std::optional<sync_pb::AutofillEntitySuppressionSpecifics>
MigrateLegacySuppressionSpecificsIfNeeded(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

// Clears all supported fields from `specifics`. Supported fields are all
// fields in the protobuf definition that have already been included in the
// client version.
sync_pb::AutofillEntitySuppressionSpecifics
TrimAutofillEntitySuppressionSpecificsDataForCaching(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_PERSONAL_CONTEXT_ENTITY_METADATA_SYNC_UTIL_H_
