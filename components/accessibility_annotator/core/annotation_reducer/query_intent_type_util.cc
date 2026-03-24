// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type_util.h"

#include "base/notimplemented.h"

namespace accessibility_annotator {

MemorySearchResult CreateResultFromEntity(QueryIntentType intent_type,
                                          const Entity& entity) {
  // TODO(crbug.com/493849593): Implement.
  NOTIMPLEMENTED();
  return MemorySearchResult(QueryIntentType::kUnknown, std::u16string(),
                            std::u16string());
}

EntityTypeEnumSet GetEntityTypesForQueryIntentType(
    QueryIntentType intent_type) {
  // TODO(crbug.com/493849593): Implement.
  NOTIMPLEMENTED();
  return EntityTypeEnumSet();
}

}  // namespace accessibility_annotator
