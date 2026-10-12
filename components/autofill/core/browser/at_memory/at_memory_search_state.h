// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_SEARCH_STATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_SEARCH_STATE_H_

#include <string>
#include <vector>

#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"

namespace autofill {

// Whether stopping a search happens because its final response was received
// (as opposed to the search being cancelled).
using AtMemoryQueryCompleted =
    base::StrongAlias<struct AtMemoryQueryCompletedTag, bool>;

// Status of the AtMemory search for the active field.
enum class AtMemorySearchStatus {
  // The initial state: no filter was entered yet, or the filter was cleared.
  // The filter and the suggestions are empty in this state.
  kZeroState,
  // The user entered or modified the filter but has not submitted it, or the
  // search for the submitted filter was cancelled.
  kQueryModified,
  // The filter was submitted and the search is in progress.
  kSearching,
  // The filter was submitted and the final response of the search (with
  // results or an error) was received.
  kResponseReceived,
};

// Stores search state for AtMemory that persists across popup lifecycles.
struct AtMemorySearchState {
  std::vector<Suggestion> suggestions;
  std::u16string filter;
  AtMemorySearchStatus status = AtMemorySearchStatus::kZeroState;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_SEARCH_STATE_H_
