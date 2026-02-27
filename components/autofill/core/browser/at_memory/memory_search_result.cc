// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/memory_search_result.h"

namespace autofill {

MemorySearchResult::MemorySearchResult() = default;
MemorySearchResult::MemorySearchResult(const MemorySearchResult&) = default;
MemorySearchResult& MemorySearchResult::operator=(const MemorySearchResult&) =
    default;
MemorySearchResult::MemorySearchResult(MemorySearchResult&&) = default;
MemorySearchResult& MemorySearchResult::operator=(MemorySearchResult&&) =
    default;
MemorySearchResult::~MemorySearchResult() = default;

}  // namespace autofill
