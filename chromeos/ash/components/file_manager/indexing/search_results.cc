// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/search_results.h"

#include <algorithm>

namespace ash::file_manager {

SearchResults::SearchResults() : total_matches(0) {}

SearchResults::SearchResults(SearchResults&& other)
    : total_matches(other.total_matches), matches(std::move(other.matches)) {}

SearchResults::~SearchResults() = default;

bool SearchResults::operator==(const SearchResults& other) const {
  return other.total_matches == total_matches &&
         std::equal(matches.begin(), matches.end(), other.matches.begin());
}

}  // namespace ash::file_manager
