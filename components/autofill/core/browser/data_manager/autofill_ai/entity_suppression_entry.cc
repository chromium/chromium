// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/i18n/case_conversion.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
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

}  // namespace autofill
