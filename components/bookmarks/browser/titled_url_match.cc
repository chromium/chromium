// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/titled_url_match.h"

#include "base/logging.h"
#include "base/strings/string16.h"

namespace bookmarks {

TitledUrlMatch::TitledUrlMatch() : node(nullptr) {}

TitledUrlMatch::TitledUrlMatch(const TitledUrlMatch& other) = default;

TitledUrlMatch::~TitledUrlMatch() {}

// static
std::vector<size_t> TitledUrlMatch::OffsetsFromMatchPositions(
    const MatchPositions& match_positions) {
  std::vector<size_t> offsets;
  for (auto i = match_positions.begin(); i != match_positions.end(); ++i) {
    offsets.push_back(i->first);
    offsets.push_back(i->second);
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
    if ((begin != base::string16::npos) && (end != base::string16::npos)) {
      const MatchPosition new_match_position(begin, end);
      new_match_positions.push_back(new_match_position);
    }
  }
  return new_match_positions;
}

}  // namespace bookmarks
