// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_SEARCH_RESULTS_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_SEARCH_RESULTS_H_

#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/components/file_manager/indexing/match.h"

namespace ash::file_manager {

// Represents search results. Search results is a combination of search matches
// and additional information. The additional information indicates what was the
// total number of files matched vs the number of returned matches (the size of
// the `matches` vector.
struct COMPONENT_EXPORT(FILE_MANAGER) SearchResults {
  // Creates empty search results.
  SearchResults();
  // Move constructor for search results. Defined so that we can return search
  // results to the caller of the Search() method in the FileIndex interface.
  SearchResults(SearchResults&&);

  ~SearchResults();

  SearchResults(const SearchResults&) = delete;
  SearchResults& operator=(const SearchResults&) = delete;

  // Returns whether this search results matches the `other` result.
  bool operator==(const SearchResults& other) const;

  // The total number of matches. This may be more than matches.size() if some
  // matches were discarded.
  int32_t total_matches;

  // A list of returned matches.
  std::vector<Match> matches;
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_SEARCH_RESULTS_H_
