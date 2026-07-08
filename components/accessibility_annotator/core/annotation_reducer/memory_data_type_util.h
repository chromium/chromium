// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_MEMORY_DATA_TYPE_UTIL_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_MEMORY_DATA_TYPE_UTIL_H_

#include <string_view>

#include "base/containers/span.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/personal_context/proto/features/common_data.pb.h"

namespace accessibility_annotator {

// Maps an Entity and a MemoryDataType to a MemorySearchResult.
// Returns an empty MemorySearchResult if the mapping is not supported.
MemorySearchResult CreateResultFromEntity(MemoryDataType memory_data_type,
                                          const Entity& entity);

// Maps a MemoryDataType to a set of EntityTypes.
EntityTypeEnumSet GetEntityTypesForMemoryDataType(
    MemoryDataType memory_data_type);

// Returns true if the given `type` is considered sensitive personal
// information.
bool IsSpiiMemoryDataType(MemoryDataType type);

// Converts a set of memory entry values into `personal_context::proto::Entity`.
// `value` is the primary value of the memory entry corresponding to the
// `memory_data_type` (for example, the actual passport number if the type is
// `kPassportNumber`). `metadata_list` contains the associated attributes (e.g.,
// expiration date, issuing country) for the memory entry.
personal_context::proto::Entity ToPersonalContextEntity(
    std::u16string_view value,
    MemoryDataType memory_data_type,
    base::span<const EntryMetadata> metadata_list);

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_MEMORY_DATA_TYPE_UTIL_H_
