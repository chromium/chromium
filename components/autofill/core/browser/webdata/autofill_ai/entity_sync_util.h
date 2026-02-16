// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_AI_ENTITY_SYNC_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_AI_ENTITY_SYNC_UTIL_H_

#include <optional>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/sync/protocol/entity_data.h"

namespace sync_pb {
class AutofillValuableMetadataSpecifics;
class AutofillValuableSpecifics;
}  // namespace sync_pb

namespace autofill {

class ChromeValuablesMetadata;
class EntityInstance;
class EntityType;

// Serializes metadata related to `EntityInstance` into
// `ChromeValuablesMetadata`.
ChromeValuablesMetadata SerializeChromeValuablesMetadata(
    const EntityInstance& entity);

// Converts the given `entity` into a `syncer::EntityData`.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromEntityInstance(
    const EntityInstance& entity,
    const sync_pb::AutofillValuableSpecifics& base_specifics);

// For a given `EntityInstance`, returns the corresponding
// `sync_pb::AutofillValuableSpecifics`. It is assumed that the entity passed to
// this function is syncable.
sync_pb::AutofillValuableSpecifics CreateSpecificsFromEntityInstance(
    const EntityInstance& entity,
    const sync_pb::AutofillValuableSpecifics& base_specifics);

// Converts the given valuable `specifics` into an equivalent EntityInstance.
std::optional<EntityInstance> CreateEntityInstanceFromSpecifics(
    const sync_pb::AutofillValuableSpecifics& specifics);

// Converts the given `metadata` into a `syncer::EntityData`.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromEntityMetadata(
    const EntityInstance::EntityMetadata& metadata,
    const sync_pb::AutofillValuableMetadataSpecifics::PassType pass_type,
    const sync_pb::AutofillValuableMetadataSpecifics& base_specifics);

// For a given `EntityMetadata`, returns the corresponding
// `sync_pb::AutofillValuableMetadataSpecifics`.
sync_pb::AutofillValuableMetadataSpecifics CreateSpecificsFromEntityMetadata(
    const EntityInstance::EntityMetadata& metadata,
    const sync_pb::AutofillValuableMetadataSpecifics::PassType pass_type,
    const sync_pb::AutofillValuableMetadataSpecifics& base_specifics);

// Converts the given valuable metadata `specifics` into an equivalent
// EntityInstance::EntityMetadata.
EntityInstance::EntityMetadata CreateEntityMetadataFromSpecifics(
    const sync_pb::AutofillValuableMetadataSpecifics& specifics);

// Converts the given `entity_type_name` to the corresponding `PassType`.
std::optional<sync_pb::AutofillValuableMetadataSpecifics::PassType>
EntityTypeToPassType(EntityType entity_type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_AI_ENTITY_SYNC_UTIL_H_
