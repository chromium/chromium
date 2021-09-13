// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_H_

namespace optimization_guide {

// The metadata associated with a single entity.
struct EntityMetadata {
  // The human-readable name of the entity in the user's locale.
  std::string human_readable_name;

  // TODO(crbug/1234578): Add broader topics when the format of that is
  // finalized.
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_H_