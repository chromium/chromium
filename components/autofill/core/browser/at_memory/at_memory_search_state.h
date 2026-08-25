// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_SEARCH_STATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_SEARCH_STATE_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/suggestions/suggestion.h"

namespace autofill {

// Stores search state for AtMemory that persists across popup lifecycles.
struct AtMemorySearchState {
  std::vector<Suggestion> suggestions;
  std::u16string filter;
  bool is_searching = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_SEARCH_STATE_H_
