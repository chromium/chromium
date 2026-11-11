// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/personal_context/personal_context_entity_metadata_sync_util.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/i18n/case_conversion.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/sync/protocol/autofill_entity_suppression_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "crypto/hash.h"

namespace autofill {

namespace {

// Changing this breaks matching against previously synced hashes.
constexpr int kEntityMetadataHashBitLength = 20;

// Computes the SHA-256 hash of the complete raw info of the given attribute.
AttributeValueHash HashAttributeValue(const AttributeInstance& attribute) {
  return AttributeValueHash(
      crypto::hash::Sha256(base::UTF16ToUTF8(attribute.GetCompleteRawInfo())));
}

// LINT.IfChange(TrimEntitySuppressionKey)
void TrimEntitySuppressionKey(sync_pb::EntitySuppressionKey& key) {
  key.clear_entity_type_name();
  key.clear_attributes();
}
// LINT.ThenChange(//components/sync/protocol/autofill_entity_suppression_specifics.proto:EntitySuppressionKey)

}  // namespace

std::vector<EntitySuppressionEntry> GetEntitySuppressionEntries(
    const EntityInstance& entity) {
  auto are_attributes_non_empty =
      [&](const DenseSet<AttributeType>& constraint_set) {
        return std::ranges::all_of(
            constraint_set, [&](AttributeType attr_type) {
              return entity.attribute(attr_type).has_value();
            });
      };

  std::vector<EntitySuppressionEntry> entries;
  for (const DenseSet<AttributeType>& constraint_set :
       entity.type().merge_constraints()) {
    if (!are_attributes_non_empty(constraint_set)) {
      continue;
    }

    entries.push_back(EntitySuppressionEntry{
        .type = entity.type(),
        .attribute_hashes =
            base::MakeFlatMap<AttributeType, AttributeValueHash>(
                constraint_set, {},
                [&](AttributeType at) {
                  return std::make_pair(
                      at, HashAttributeValue(*entity.attribute(at)));
                }),
    });
  }
  return entries;
}

EntitySuppressionHash GetEntitySuppressionHash(
    const EntitySuppressionEntry& entry) {
  // Sorted, so that the hash doesn't depend on the order of the attribute types
  // within the merge constraint, which follows the schema.
  std::vector<AttributeValueHash> value_hashes;
  for (const auto& [attribute_type, value_hash] : entry.attribute_hashes) {
    value_hashes.push_back(value_hash);
  }
  std::ranges::sort(value_hashes);
  crypto::hash::Hasher hasher(crypto::hash::kSha256);
  hasher.Update(entry.type.name_as_string());
  for (const AttributeValueHash& value_hash : value_hashes) {
    hasher.Update(value_hash.value());
  }
  std::array<uint8_t, crypto::hash::kSha256Size> digest;
  hasher.Finish(digest);
  return EntitySuppressionHash(
      base::U32FromBigEndian(base::span(digest).first<4>()) >>
      (32 - kEntityMetadataHashBitLength));
}

std::optional<EntityConfirmationHash> GetEntityConfirmationHash(
    const EntityInstance& entity) {
  // Normalize only case (locale-independently) and whitespace so trivial
  // re-extraction differences don't invalidate confirmations, while still
  // catching subtle punctuation, diacritic, or character changes. Skip
  // attributes whose normalized value is empty so unpopulated schema fields
  // don't affect the hash.
  std::vector<std::pair<std::string_view, AttributeValueHash>> attributes;
  attributes.reserve(entity.attributes().size());
  for (const AttributeInstance& attribute : entity.attributes()) {
    std::u16string normalized_value = base::i18n::FoldCase(
        base::CollapseWhitespace(attribute.GetCompleteRawInfo(),
                                 /*trim_sequences_with_line_breaks=*/false));
    if (normalized_value.empty()) {
      continue;
    }
    attributes.emplace_back(attribute.type().name_as_string(),
                            AttributeValueHash(crypto::hash::Sha256(
                                base::UTF16ToUTF8(normalized_value))));
  }
  if (attributes.empty()) {
    return std::nullopt;
  }
  // Sorted by attribute type name, so that the hash doesn't depend on the order
  // of the attribute types in `entity.attributes()`, which follows the schema.
  std::ranges::sort(attributes, {},
                    &std::pair<std::string_view, AttributeValueHash>::first);
  crypto::hash::Hasher hasher(crypto::hash::kSha256);
  hasher.Update(entity.type().name_as_string());
  for (const auto& [attribute_name, value_hash] : attributes) {
    hasher.Update(attribute_name);
    hasher.Update(value_hash.value());
  }
  std::array<uint8_t, crypto::hash::kSha256Size> digest;
  hasher.Finish(digest);
  return EntityConfirmationHash(
      base::U32FromBigEndian(base::span(digest).first<4>()) >>
      (32 - kEntityMetadataHashBitLength));
}

sync_pb::AutofillEntitySuppressionSpecifics
CreateSpecificsFromEntitySuppressionHash(
    const std::string& guid,
    EntitySuppressionHash suppression_hash,
    base::Time creation_time,
    const sync_pb::AutofillEntitySuppressionSpecifics& base_specifics) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics = base_specifics;
  specifics.set_guid(guid);
  specifics.set_creation_time_windows_epoch_micros(
      creation_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  specifics.mutable_suppression()->set_hash(suppression_hash.value());
  specifics.set_entity_suppression_hash(suppression_hash.value());
  return specifics;
}

std::unique_ptr<syncer::EntityData> CreateEntityDataFromEntitySuppressionHash(
    const std::string& guid,
    EntitySuppressionHash suppression_hash,
    base::Time creation_time,
    const sync_pb::AutofillEntitySuppressionSpecifics& base_specifics) {
  return CreateEntityDataFromSpecifics(CreateSpecificsFromEntitySuppressionHash(
      guid, suppression_hash, creation_time, base_specifics));
}

sync_pb::AutofillEntitySuppressionSpecifics
CreateSpecificsFromEntityConfirmationHash(
    std::string_view guid,
    EntityConfirmationHash confirmation_hash,
    base::Time creation_time,
    const sync_pb::AutofillEntitySuppressionSpecifics& base_specifics) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics = base_specifics;
  specifics.set_guid(guid);
  specifics.set_creation_time_windows_epoch_micros(
      creation_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.mutable_confirmation()->set_hash(confirmation_hash.value());
  return specifics;
}

std::unique_ptr<syncer::EntityData> CreateEntityDataFromEntityConfirmationHash(
    std::string_view guid,
    EntityConfirmationHash confirmation_hash,
    base::Time creation_time,
    const sync_pb::AutofillEntitySuppressionSpecifics& base_specifics) {
  return CreateEntityDataFromSpecifics(
      CreateSpecificsFromEntityConfirmationHash(guid, confirmation_hash,
                                                creation_time, base_specifics));
}

std::unique_ptr<syncer::EntityData> CreateEntityDataFromSpecifics(
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

  switch (specifics.metadata_entry_case()) {
    case sync_pb::AutofillEntitySuppressionSpecifics::kSuppression:
      return specifics.suppression().has_hash();
    case sync_pb::AutofillEntitySuppressionSpecifics::kConfirmation:
      return false;
    case sync_pb::AutofillEntitySuppressionSpecifics::METADATA_ENTRY_NOT_SET:
      break;
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

bool AreEntityConfirmationSpecificsValid(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics) {
  return !specifics.guid().empty() && specifics.has_confirmation() &&
         specifics.confirmation().has_hash();
}

std::optional<sync_pb::AutofillEntitySuppressionSpecifics>
MigrateLegacySuppressionSpecificsIfNeeded(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics) {
  if (specifics.has_suppression()) {
    return std::nullopt;
  }
  std::optional<EntitySuppressionHash> hash =
      CreateEntitySuppressionHashFromSpecifics(specifics);
  if (!hash) {
    return std::nullopt;
  }
  sync_pb::AutofillEntitySuppressionSpecifics migrated = specifics;
  migrated.mutable_suppression()->set_hash(hash->value());
  migrated.set_entity_suppression_hash(hash->value());
  return migrated;
}

std::optional<EntitySuppressionHash> CreateEntitySuppressionHashFromSpecifics(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics) {
  if (!AreEntitySuppressionSpecificsValid(specifics)) {
    return std::nullopt;
  }

  switch (specifics.metadata_entry_case()) {
    case sync_pb::AutofillEntitySuppressionSpecifics::kSuppression:
      return EntitySuppressionHash(specifics.suppression().hash());
    case sync_pb::AutofillEntitySuppressionSpecifics::kConfirmation:
      return std::nullopt;
    case sync_pb::AutofillEntitySuppressionSpecifics::METADATA_ENTRY_NOT_SET:
      break;
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

  return GetEntitySuppressionHash(EntitySuppressionEntry{
      .type = *entity_type,
      .attribute_hashes = base::flat_map<AttributeType, AttributeValueHash>(
          std::move(attribute_hashes)),
  });
}

std::optional<EntityConfirmationHash> CreateEntityConfirmationHashFromSpecifics(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics) {
  if (!AreEntityConfirmationSpecificsValid(specifics)) {
    return std::nullopt;
  }
  return EntityConfirmationHash(specifics.confirmation().hash());
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
  trimmed_specifics.clear_creation_time_windows_epoch_micros();
  trimmed_specifics.clear_entity_suppression_hash();
  switch (trimmed_specifics.metadata_entry_case()) {
    case sync_pb::AutofillEntitySuppressionSpecifics::kSuppression: {
      sync_pb::AutofillEntitySuppressionSpecifics::Suppression& suppression =
          *trimmed_specifics.mutable_suppression();
      suppression.clear_hash();
      if (suppression.ByteSizeLong() == 0) {
        trimmed_specifics.clear_suppression();
      }
      break;
    }
    case sync_pb::AutofillEntitySuppressionSpecifics::kConfirmation: {
      sync_pb::AutofillEntitySuppressionSpecifics::Confirmation& confirmation =
          *trimmed_specifics.mutable_confirmation();
      confirmation.clear_hash();
      if (confirmation.ByteSizeLong() == 0) {
        trimmed_specifics.clear_confirmation();
      }
      break;
    }
    case sync_pb::AutofillEntitySuppressionSpecifics::METADATA_ENTRY_NOT_SET:
      break;
  }
  return trimmed_specifics;
}
// LINT.ThenChange(//components/sync/protocol/autofill_entity_suppression_specifics.proto:AutofillEntitySuppressionSpecifics)

}  // namespace autofill
