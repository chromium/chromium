// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/personal_context/entity_suppression_sync_util.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/sync/protocol/entity_data.h"
#include "crypto/hash.h"

namespace autofill {

namespace {

// LINT.IfChange(TrimEntitySuppressionKey)
void TrimEntitySuppressionKey(sync_pb::EntitySuppressionKey& key) {
  key.clear_entity_type_name();
  key.clear_attributes();
}
// LINT.ThenChange(//components/sync/protocol/autofill_entity_suppression_specifics.proto:EntitySuppressionKey)

}  // namespace

sync_pb::AutofillEntitySuppressionSpecifics
CreateSpecificsFromEntitySuppressionKey(
    const std::string& guid,
    const EntitySuppressionKey& suppression_key,
    base::Time creation_time,
    const sync_pb::AutofillEntitySuppressionSpecifics& base_specifics) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics = base_specifics;
  specifics.set_guid(guid);
  specifics.set_creation_time_windows_epoch_micros(
      creation_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  // TODO(crbug.com/501036619): Set `schema_version`.

  if (const EntitySuppressionHash* hash =
          std::get_if<EntitySuppressionHash>(&suppression_key)) {
    specifics.set_entity_suppression_hash(hash->value());
    return specifics;
  }

  // TODO(crbug.com/501036619): Remove once legacy entries are no longer
  // supported.
  const EntitySuppressionEntry& entry =
      std::get<LegacyEntitySuppressionEntry>(suppression_key).value();
  sync_pb::EntitySuppressionKey* key =
      specifics.mutable_entity_suppression_key();
  key->set_entity_type_name(std::string(entry.type.name_as_string()));
  key->clear_attributes();
  for (const auto& [attr_type, hash] : entry.attribute_hashes) {
    sync_pb::EntitySuppressionKey::Attribute* attr = key->add_attributes();
    attr->set_name(std::string(attr_type.name_as_string()));
    attr->set_value_hash(hash.value().data(), hash.value().size());
  }
  return specifics;
}

std::unique_ptr<syncer::EntityData> CreateEntityDataFromEntitySuppressionKey(
    const std::string& guid,
    const EntitySuppressionKey& suppression_key,
    base::Time creation_time,
    const sync_pb::AutofillEntitySuppressionSpecifics& base_specifics) {
  return CreateEntityDataFromEntitySuppressionSpecifics(
      CreateSpecificsFromEntitySuppressionKey(guid, suppression_key,
                                              creation_time, base_specifics));
}

std::unique_ptr<syncer::EntityData>
CreateEntityDataFromEntitySuppressionSpecifics(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->name = specifics.guid();
  *entity_data->specifics.mutable_autofill_entity_suppression() = specifics;
  return entity_data;
}

bool AreEntitySuppressionSpecificsValid(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics) {
  if (specifics.guid().empty()) {
    return false;
  }

  if (specifics.has_entity_suppression_hash()) {
    return true;
  }

  // TODO(crbug.com/501036619): Remove once legacy entries are no longer
  // supported.
  if (!specifics.has_entity_suppression_key()) {
    return false;
  }

  const sync_pb::EntitySuppressionKey& key = specifics.entity_suppression_key();
  std::optional<EntityType> entity_type =
      StringToEntityType(key.entity_type_name());
  if (!entity_type || key.attributes().empty()) {
    return false;
  }

  DenseSet<AttributeType> seen_attributes;
  for (const sync_pb::EntitySuppressionKey::Attribute& attr :
       key.attributes()) {
    if (attr.name().empty() ||
        attr.value_hash().size() != crypto::hash::kSha256Size) {
      return false;
    }
    std::optional<AttributeType> attr_type =
        StringToAttributeType(*entity_type, attr.name());
    if (!attr_type || !seen_attributes.insert(*attr_type).second) {
      return false;
    }
  }

  return true;
}

std::optional<EntitySuppressionKey> CreateEntitySuppressionKeyFromSpecifics(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics) {
  if (!AreEntitySuppressionSpecificsValid(specifics)) {
    return std::nullopt;
  }

  if (specifics.has_entity_suppression_hash()) {
    return EntitySuppressionHash(specifics.entity_suppression_hash());
  }

  // TODO(crbug.com/501036619): Remove once legacy entries are no longer
  // supported.
  const sync_pb::EntitySuppressionKey& key = specifics.entity_suppression_key();
  std::optional<EntityType> entity_type =
      StringToEntityType(key.entity_type_name());
  CHECK(entity_type.has_value());

  auto attribute_hashes = base::ToVector(
      key.attributes(),
      [&entity_type](const sync_pb::EntitySuppressionKey::Attribute& attr) {
        std::optional<AttributeType> attr_type =
            StringToAttributeType(*entity_type, attr.name());
        CHECK(attr_type.has_value());

        std::array<uint8_t, crypto::hash::kSha256Size> hash_array;
        base::span(hash_array).copy_from(base::as_byte_span(attr.value_hash()));
        return std::make_pair(*attr_type, AttributeValueHash(hash_array));
      });

  return LegacyEntitySuppressionEntry(EntitySuppressionEntry{
      .type = *entity_type,
      .attribute_hashes = base::flat_map<AttributeType, AttributeValueHash>(
          std::move(attribute_hashes)),
  });
}

// LINT.IfChange(TrimAutofillEntitySuppressionSpecificsDataForCaching)
sync_pb::AutofillEntitySuppressionSpecifics
TrimAutofillEntitySuppressionSpecificsDataForCaching(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics) {
  sync_pb::AutofillEntitySuppressionSpecifics trimmed_specifics(specifics);
  trimmed_specifics.clear_guid();
  if (trimmed_specifics.has_entity_suppression_key()) {
    TrimEntitySuppressionKey(
        *trimmed_specifics.mutable_entity_suppression_key());
    if (trimmed_specifics.entity_suppression_key().ByteSizeLong() == 0) {
      trimmed_specifics.clear_entity_suppression_key();
    }
  }
  trimmed_specifics.clear_schema_version();
  trimmed_specifics.clear_creation_time_windows_epoch_micros();
  trimmed_specifics.clear_entity_suppression_hash();
  return trimmed_specifics;
}
// LINT.ThenChange(//components/sync/protocol/autofill_entity_suppression_specifics.proto:AutofillEntitySuppressionSpecifics)

}  // namespace autofill
