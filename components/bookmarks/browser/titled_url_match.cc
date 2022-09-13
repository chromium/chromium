// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/titled_url_match.h"

#include <string>

#include "base/check_op.h"

namespace bookmarks {

TitledUrlMatch::TitledUrlMatch() : node(nullptr), has_ancestor_match(false) {}

TitledUrlMatch::TitledUrlMatch(const TitledUrlMatch& other) = default;

TitledUrlMatch::~TitledUrlMatch() = default;

// static
std::vector<size_t> TitledUrlMatch::OffsetsFromMatchPositions(
    const MatchPositions& match_positions) {
  std::vector<size_t> offsets;
  for (const auto& match_position : match_positions) {
    offsets.push_back(match_position.first);
    offsets.push_back(match_position.second);
  }
  return offsets;
}

// static
TitledUrlMatch::MatchPositions TitledUrlMatch::ReplaceOffsetsInMatchPositions(
    const MatchPositions& match_positions,
    const std::vector<size_t>& offsets) {
  DCHECK_EQ(2 * match_positions.size(), offsets.size());
  MatchPositions new_match_positions;
  auto offset_iter = offsets.begin();
  for (auto match_iter = match_positions.begin();
       match_iter != match_positions.end(); ++match_iter, ++offset_iter) {
    const size_t begin = *offset_iter;
    ++offset_iter;
    const size_t end = *offset_iter;
    if ((begin != std::u16string::npos) && (end != std::u16string::npos)) {
      const MatchPosition new_match_position(begin, end);
      new_match_positions.push_back(new_match_position);
    }
  }
  return new_match_positions;
}

}  // namespace bookmarks
