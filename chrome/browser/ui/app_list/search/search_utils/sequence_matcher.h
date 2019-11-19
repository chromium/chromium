// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTILS_SEQUENCE_MATCHER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTILS_SEQUENCE_MATCHER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"

namespace app_list {

// Performs the calculation of similarity level between 2 strings. This class's
// functionality is inspired by python's difflib.SequenceMatcher library.
// (https://docs.python.org/2/library/difflib.html#difflib.SequenceMatcher)
class SequenceMatcher {
 public:
  // Representing a common substring between |first_string_| and
  // |second_string_|.
  struct Match {
    Match();
    Match(int pos_first, int pos_second, int len);
    // Starting position of the common substring in |first_string_|.
    int pos_first_string;
    // Starting position of the common substring in |second_string_|.
    int pos_second_string;
    // Length of the common substring.
    int length;
  };
  SequenceMatcher(const base::string16& first_string,
                  const base::string16& second_string);

  ~SequenceMatcher() = default;

  // Calculates similarity ratio of |first_string_| and |second_string_|.
  double Ratio();
  // Calculates the Damerau–Levenshtein distance between |first_string_| and
  // |second_string_|.
  // See https://en.wikipedia.org/wiki/Damerau–Levenshtein_distance for more
  // details.
  int EditDistance();
  // Finds the longest common substring between
  // |first_string_[first_start:first_end]| and
  // |second_string_[second_start:second_end]|.
  Match FindLongestMatch(int first_start,
                         int first_end,
                         int second_start,
                         int second_end);
  // Get all matching blocks of |first_string_| and |second_string_|.
  // All blocks will be sorted by starting position of them in the
  // |first_string_|. The last matching block will always be
  // Match(first_string_.size(), second_string_.size(), 0).
  std::vector<Match> GetMatchingBlocks();

 private:
  base::string16 first_string_;
  base::string16 second_string_;
  double edit_distance_ratio_ = -1.0;
  double block_matching_ratio_ = -1.0;
  std::vector<Match> matching_blocks_;

  // Controls whether to use edit distance to calculate ratio.
  bool use_edit_distance_;
  int edit_distance_ = -1;
  // For each character |c| in |second_string_|, this variable
  // |char_to_positions_| stores all positions where |c| occurs in
  // |second_string_|.
  std::unordered_map<char, std::vector<int>> char_to_positions_;
  // Memory for dynamic programming algorithm used in FindLongestMatch().
  std::vector<int> dp_common_string_;
  DISALLOW_COPY_AND_ASSIGN(SequenceMatcher);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTILS_SEQUENCE_MATCHER_H_
