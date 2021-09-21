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
  ~EntityMetadata();
  EntityMetadata(const EntityMetadata&);

  // The human-readable name of the entity in the user's locale.
  std::string human_readable_name;

  // A map from human-readable category the entity belongs to in the user's
  // locale to the confidence that the category is related to the entity. Will
  // contain the top 5 entries based on confidence score.
  base::flat_map<std::string, float> human_readable_categories;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_H_