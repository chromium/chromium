// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ENTRY_TYPE_UTIL_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ENTRY_TYPE_UTIL_H_

#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"

namespace accessibility_annotator {

// Maps an Entity and a EntryType to a MemorySearchResult.
// Returns an empty MemorySearchResult if the mapping is not supported.
MemorySearchResult CreateResultFromEntity(EntryType entry_type,
                                          const Entity& entity);

// Maps a EntryType to a set of EntityTypes.
EntityTypeEnumSet GetEntityTypesForEntryType(EntryType entry_type);

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ENTRY_TYPE_UTIL_H_
