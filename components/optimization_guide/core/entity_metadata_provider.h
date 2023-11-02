// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_PROVIDER_H_

#include "components/optimization_guide/core/entity_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// Callback to inform the caller that the metadata for an entity ID has been
// retrieved.
using EntityMetadataRetrievedCallback =
    base::OnceCallback<void(const absl::optional<EntityMetadata>&)>;

// A class that provides metadata about entities.
class EntityMetadataProvider {
 public:
  // Retrieves the metadata associated with |entity_id|. Invokes |callback|
  // when done.
  virtual void GetMetadataForEntityId(
      const std::string& entity_id,
      EntityMetadataRetrievedCallback callback) = 0;

 protected:
  EntityMetadataProvider() = default;
  virtual ~EntityMetadataProvider() = default;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_METADATA_PROVIDER_H_