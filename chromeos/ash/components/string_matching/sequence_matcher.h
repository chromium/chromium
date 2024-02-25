// Copyright 2019 The Chromium Authors
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
constexpr bool kUseTextLengthAgnosticism = false;

}  // namespace

// Performs the calculation of similarity level between 2 strings. This class's
// functionality is inspired by python's difflib.SequenceMatcher library.
// (https://docs.python.org/2/library/difflib.html#difflib.SequenceMatcher)
//
// TODO(crbug.com/1336160): The unused edit distance pathway has been removed.
// Consider coming up with a new edit-distance method having the awareness of
// string structures. (e.g., awareness of block matching and token break
// locations).
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
  // means heavier penalty will be applied to larger number of blocks.
  SequenceMatcher(
      const std::u16string& first_string,
      const std::u16string& second_string,
      double num_matching_blocks_penalty = kNumMatchingBlocksPenalty);

  SequenceMatcher(const SequenceMatcher&) = delete;
  SequenceMatcher& operator=(const SequenceMatcher&) = delete;

  ~SequenceMatcher();

  // Calculates similarity ratio of `first_string_` and `second_string_`.
  //
  // In the actual string matching in launcher searches, we will input with the
  // query as `first_string_` and the text as `second_string_`. As the query is
  // likely to be shorter than the text in most cases, we would like to
  // ignore/lower the influence of the amounts of any remaining unmatched
  // portions of the `second_string_` onto the ratio (i.e., "text-length
  // agnosticism").
  //
  // Thus, We will trim the text length if it is too long and
  // `text_length_agnostic` is true, and it only works for the block
  // matching algorithm.
  double Ratio(bool text_length_agnostic = kUseTextLengthAgnosticism);
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
  double block_matching_ratio_ = -1.0;
  std::vector<Match> matching_blocks_;

  // For each character `c` in `second_string_`, this variable
  // `char_to_positions_` stores all positions where `c` occurs in
  // `second_string_`.
  std::unordered_map<char, std::vector<int>> char_to_positions_;
  // Memory for dynamic programming algorithm used in FindLongestMatch().
  std::vector<int> dp_common_string_;
};

}  // namespace ash::string_matching

#endif  // CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_SEQUENCE_MATCHER_H_
