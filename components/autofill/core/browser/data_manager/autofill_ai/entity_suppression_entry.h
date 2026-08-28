// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_ENTRY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_ENTRY_H_

#include <array>
#include <cstdint>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "crypto/hash.h"

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

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_ENTRY_H_
