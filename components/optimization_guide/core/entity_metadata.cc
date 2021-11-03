// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/entity_metadata.h"

namespace optimization_guide {

EntityMetadata::EntityMetadata() = default;
EntityMetadata::EntityMetadata(
    const std::string& entity_id,
    const std::string& human_readable_name,
    const base::flat_map<std::string, float>& human_readable_categories)
    : entity_id(entity_id),
      human_readable_name(human_readable_name),
      human_readable_categories(human_readable_categories) {}
EntityMetadata::EntityMetadata(const EntityMetadata&) = default;
EntityMetadata::~EntityMetadata() = default;

ScoredEntityMetadata::ScoredEntityMetadata() = default;
ScoredEntityMetadata::ScoredEntityMetadata(float score,
                                           const EntityMetadata& md)
    : metadata(md), score(score) {}
ScoredEntityMetadata::ScoredEntityMetadata(const ScoredEntityMetadata&) =
    default;
ScoredEntityMetadata::~ScoredEntityMetadata() = default;

}  // namespace optimization_guide
