// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_MEMORY_SEARCH_RESULT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_MEMORY_SEARCH_RESULT_H_

#include <string>

namespace autofill {

// Represents a single search result from @memory, intended to be displayed
// as a suggestion in the UI.
struct MemorySearchResult {
  MemorySearchResult();
  MemorySearchResult(const MemorySearchResult&);
  MemorySearchResult& operator=(const MemorySearchResult&);
  MemorySearchResult(MemorySearchResult&&);
  MemorySearchResult& operator=(MemorySearchResult&&);
  ~MemorySearchResult();

  // Text to be inserted into an editable field.
  std::u16string value;
  // Primary label in the suggestion.
  std::u16string title;
  // Secondary label in the suggestion.
  std::u16string description;
  // Relevance affecting ordering, the higher the better.
  double ranking_score = 0.0;

  bool operator==(const MemorySearchResult& other) const = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_MEMORY_SEARCH_RESULT_H_
