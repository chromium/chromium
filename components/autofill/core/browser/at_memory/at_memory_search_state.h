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

// Status of the AtMemory search for the active field. The 0-state (no filter
// entered yet, or the filter was cleared) is represented by the absence of an
// `AtMemorySearchState`.
// TODO(crbug.com/535486238): Consider adding a `kZeroState` value and making
// the persisted search state non-optional, so that the 0-state is modelled
// explicitly instead of by `std::nullopt`.
enum class AtMemorySearchStatus {
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
  AtMemorySearchStatus status = AtMemorySearchStatus::kQueryModified;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_SEARCH_STATE_H_
