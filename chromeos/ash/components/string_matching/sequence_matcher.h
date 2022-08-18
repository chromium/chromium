// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_SEQUENCE_MATCHER_H_
#define CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_SEQUENCE_MATCHER_H_

#include <string>
#include <unordered_map>
#include <vector>

namespace ash::string_matching {

namespace {

constexpr double kNumMatchingBlocksPenalty = 0.1;
constexpr bool kUseEditDistance = false;

}  // namespace

// Performs the calculation of similarity level between 2 strings. This class's
// functionality is inspired by python's difflib.SequenceMatcher library.
// (https://docs.python.org/2/library/difflib.html#difflib.SequenceMatcher)
//
// TODO(crbug.com/1336160): This class contains two mutually exclusive
// pathways (edit distance and block matching), with distinct algorithms
// and ratio calculations. The edit distance pathway is currently unused.
// Consider removing / refactoring.
class SequenceMatcher {
 public:
  // Representing a common substring between `first_string_` and
  // `second_string_`.
  struct Match {
    Match();
    Match(int pos_first, int pos_second, int len);
    // Starting position of the common substring in `first_string_`.
    int pos_first_string;
    // Starting position of the common substring in `second_string_`.
    int pos_second_string;
    // Length of the common substring.
    int length;
  };

  // `num_matching_blocks_penalty` is used to penalize too many small matching
  // blocks. For the same number of matching characters, we prefer fewer
  // matching blocks. Value equal to 0 means no penalty. Values greater than 0
  // means heavier penalty will be applied to larger number of blocks. This is
  // only appled if `use_edit_distance` is false.
  SequenceMatcher(
      const std::u16string& first_string,
      const std::u16string& second_string,
      double num_matching_blocks_penalty = kNumMatchingBlocksPenalty,
      bool use_edit_distance = kUseEditDistance);

  SequenceMatcher(const SequenceMatcher&) = delete;
  SequenceMatcher& operator=(const SequenceMatcher&) = delete;

  ~SequenceMatcher() = default;

  // Calculates similarity ratio of `first_string_` and `second_string_`.
  double Ratio();
  // Calculates the Damerau–Levenshtein restricted edit distance between
  // `first_string_` and `second_string_`. Also known as the "optimal string
  // alignment distance".
  //
  // The algorithm considers the following edit operations: insertion, deletion,
  // substitution, and two-character transposition. It does not consider
  // multiple adjacent transpositions. See
  // https://en.wikipedia.org/wiki/Damerau–Levenshtein_distance for more
  // details.
  int EditDistance();
  // Finds the longest common substring between
  // `first_string_[first_start:first_end]` and
  // `second_string_[second_start:second_end]`. Used by
  // GetMatchingBlocks().
  Match FindLongestMatch(int first_start,
                         int first_end,
                         int second_start,
                         int second_end);
  // Get all matching blocks of `first_string_` and `second_string_`.
  // All blocks will be sorted by their starting position within
  // `first_string_`.
  //
  // The last matching block will always be:
  //
  //   Match(first_string_.size(), second_string_.size(), 0).
  //
  // This is to cover the case where two strings have no matching blocks,
  // so that we have something to store in `matching_blocks_` to indicate
  // that matching has taken place.
  std::vector<Match> GetMatchingBlocks();

 private:
  std::u16string first_string_;
  std::u16string second_string_;
  double num_matching_blocks_penalty_;
  double edit_distance_ratio_ = -1.0;
  double block_matching_ratio_ = -1.0;
  std::vector<Match> matching_blocks_;

  // Controls whether to use edit distance to calculate ratio.
  bool use_edit_distance_;
  int edit_distance_ = -1;
  // For each character `c` in `second_string_`, this variable
  // `char_to_positions_` stores all positions where `c` occurs in
  // `second_string_`.
  std::unordered_map<char, std::vector<int>> char_to_positions_;
  // Memory for dynamic programming algorithm used in FindLongestMatch().
  std::vector<int> dp_common_string_;
};

}  // namespace ash::string_matching

#endif  // CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_SEQUENCE_MATCHER_H_
