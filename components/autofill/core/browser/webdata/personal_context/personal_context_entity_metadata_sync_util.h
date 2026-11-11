// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_PERSONAL_CONTEXT_ENTITY_METADATA_SYNC_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_PERSONAL_CONTEXT_ENTITY_METADATA_SYNC_UTIL_H_

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/sync/protocol/autofill_entity_suppression_specifics.pb.h"
#include "crypto/hash.h"

namespace syncer {
struct EntityData;
}  // namespace syncer

namespace autofill {

class EntityInstance;

// Strong alias representing a SHA-256 hash of an entity attribute value.
using AttributeValueHash =
    base::StrongAlias<struct AttributeValueHashTag,
                      std::array<uint8_t, crypto::hash::kSha256Size>>;

// Represents a suppression entry for a merge constraint set with non-empty
// attributes.
struct EntitySuppressionEntry {
  EntityType type;
  base::flat_map<AttributeType, AttributeValueHash> attribute_hashes;

  friend auto operator<=>(const EntitySuppressionEntry&,
                          const EntitySuppressionEntry&) = default;

  template <typename H>
  friend H AbslHashValue(H h, const EntitySuppressionEntry& entry) {
    return H::combine(std::move(h), entry.type, entry.attribute_hashes);
  }
};

// Constructs suppression entries for all merge constraint sets of `entity`
// whose attributes are all non-empty.
std::vector<EntitySuppressionEntry> GetEntitySuppressionEntries(
    const EntityInstance& entity);

// The leading 20 bits of a SHA-256 hash of an `EntitySuppressionEntry`.
using EntitySuppressionHash =
    base::StrongAlias<struct EntitySuppressionHashTag, uint32_t>;

// The leading 20 bits of a SHA-256 hash of a confirmed `EntityInstance`.
using EntityConfirmationHash =
    base::StrongAlias<struct EntityConfirmationHashTag, uint32_t>;

// Returns the hash of the entity type and the attribute value hashes of
// `entry`.
EntitySuppressionHash GetEntitySuppressionHash(
    const EntitySuppressionEntry& entry);

// Returns the hash of the entity type and all non-empty attribute types and
// normalized value hashes of `entity`, or `std::nullopt` if `entity` has no
// non-empty attributes after normalization.
std::optional<EntityConfirmationHash> GetEntityConfirmationHash(
    const EntityInstance& entity);

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
