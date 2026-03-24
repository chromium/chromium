// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"

namespace accessibility_annotator {

MemorySearchResult::MemorySearchResult() = default;
MemorySearchResult::MemorySearchResult(const MemorySearchResult&) = default;
MemorySearchResult& MemorySearchResult::operator=(const MemorySearchResult&) =
    default;
MemorySearchResult::MemorySearchResult(MemorySearchResult&&) = default;
MemorySearchResult& MemorySearchResult::operator=(MemorySearchResult&&) =
    default;
MemorySearchResult::~MemorySearchResult() = default;

}  // namespace accessibility_annotator
