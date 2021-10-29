// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_H_

#include <string>

#include "base/containers/flat_map.h"

namespace optimization_guide {

// The metadata associated with a single entity.
struct EntityMetadata {
  EntityMetadata();
  EntityMetadata(
      const std::string& entity_id,
      const std::string& human_readable_name,
      const base::flat_map<std::string, float>& human_readable_categories);
  EntityMetadata(const EntityMetadata&);
  ~EntityMetadata();

  // The opaque entity id.
  std::string entity_id;

  // The human-readable name of the entity in the user's locale.
  std::string human_readable_name;

  // A map from human-readable category the entity belongs to in the user's
  // locale to the confidence that the category is related to the entity. Will
  // contain the top 5 entries based on confidence score.
  base::flat_map<std::string, float> human_readable_categories;

  friend bool operator==(const EntityMetadata& lhs, const EntityMetadata& rhs) {
    return lhs.entity_id == rhs.entity_id &&
           lhs.human_readable_name == rhs.human_readable_name &&
           lhs.human_readable_categories == rhs.human_readable_categories;
  }
};

// The metadata with its score as output of the model execution.
struct ScoredEntityMetadata {
  ScoredEntityMetadata();
  ScoredEntityMetadata(float score, const EntityMetadata& md);
  ScoredEntityMetadata(const ScoredEntityMetadata&);
  ~ScoredEntityMetadata();

  // The metadata.
  EntityMetadata metadata;

  // The score.
  float score;

  friend bool operator==(const ScoredEntityMetadata& lhs,
                         const ScoredEntityMetadata& rhs) {
    constexpr const double kScoreTolerance = 1e-6;
    return lhs.metadata == rhs.metadata &&
           abs(lhs.score - rhs.score) <= kScoreTolerance;
  }
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_H_