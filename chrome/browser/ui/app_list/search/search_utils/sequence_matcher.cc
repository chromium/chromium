// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_utils/sequence_matcher.h"

#include <algorithm>
#include <queue>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/metrics/field_trial_params.h"

namespace app_list {

namespace {
constexpr bool kDefaultUseEditDistance = false;
using Match = SequenceMatcher::Match;
using Matches = std::vector<Match>;

bool CompareMatches(const Match& m1, const Match& m2) {
  return m1.pos_first_string < m2.pos_first_string;
}
}  // namespace

SequenceMatcher::Match::Match() {}
SequenceMatcher::Match::Match(int pos_first, int pos_second, int len)
    : pos_first_string(pos_first), pos_second_string(pos_second), length(len) {
  DCHECK_GE(pos_first_string, 0);
  DCHECK_GE(pos_second_string, 0);
  DCHECK_GE(length, 0);
}

SequenceMatcher::SequenceMatcher(const base::string16& first_string,
                                 const base::string16& second_string)
    : first_string_(first_string),
      second_string_(second_string),
      dp_common_string_(second_string.size() + 1, 0) {
  DCHECK(!first_string_.empty() || !second_string_.empty());

  for (size_t i = 0; i < second_string_.size(); i++) {
    char_to_positions_[second_string_[i]].emplace_back(i);
  }
  use_edit_distance_ = base::GetFieldTrialParamByFeatureAsBool(
      app_list_features::kEnableFuzzyAppSearch, "use_edit_distance",
      kDefaultUseEditDistance);
}

Match SequenceMatcher::FindLongestMatch(int first_start,
                                        int first_end,
                                        int second_start,
                                        int second_end) {
  Match match(first_start, second_start, 0);

  // These two vectors are used to do "fast update".
  // |dp_values_to_erase| contains the values should be erased from
  // |dp_common_string_|.
  // |dp_values_to_affect| contains the values should be updated from
  // |dp_common_string_|.
  std::vector<std::pair<int, int>> dp_values_to_erase;
  std::vector<std::pair<int, int>> dp_values_to_affect;

  for (int i = first_start; i < first_end; i++) {
    dp_values_to_affect.clear();
    for (auto j : char_to_positions_[first_string_[i]]) {
      if (j < second_start) {
        continue;
      }
      if (j >= second_end) {
        break;
      }
      // dp_commong_string_[j + 1] is length of longest common substring
      // ends at first_string_[i] and second_string_[j + 1]
      const int length = dp_common_string_[j] + 1;
      dp_values_to_affect.emplace_back(j + 1, length);
      if (length > match.length) {
        match.pos_first_string = i - length + 1;
        match.pos_second_string = j - length + 1;
        match.length = length;
      }
    }
    // Updates dp_common_string_
    for (auto const& element : dp_values_to_erase) {
      dp_common_string_[element.first] = 0;
    }
    for (auto const& element : dp_values_to_affect) {
      dp_common_string_[element.first] = element.second;
    }
    std::swap(dp_values_to_erase, dp_values_to_affect);
  }
  // Erases all updated value for the next call.
  std::fill(dp_common_string_.begin(), dp_common_string_.end(), 0);

  return match;
}

Matches SequenceMatcher::GetMatchingBlocks() {
  if (!matching_blocks_.empty()) {
    return matching_blocks_;
  }

  // This queue contains a tuple of 4 integers that represent 2 substrings to
  // find the longest match in the following order: first_start, first_end,
  // second_start, second_end.
  std::queue<std::tuple<int, int, int, int>> queue_block;
  queue_block.emplace(0, first_string_.size(), 0, second_string_.size());

  // Find all matching blocks recursively.
  while (!queue_block.empty()) {
    int first_start, first_end, second_start, second_end;
    std::tie(first_start, first_end, second_start, second_end) =
        queue_block.front();
    queue_block.pop();
    const Match match =
        FindLongestMatch(first_start, first_end, second_start, second_end);
    if (match.length > 0) {
      if (first_start < match.pos_first_string &&
          second_start < match.pos_second_string) {
        queue_block.emplace(first_start, match.pos_first_string, second_start,
                            match.pos_second_string);
      }
      if (match.pos_first_string + match.length < first_end &&
          match.pos_second_string + match.length < second_end) {
        queue_block.emplace(match.pos_first_string + match.length, first_end,
                            match.pos_second_string + match.length, second_end);
      }
      matching_blocks_.push_back(match);
    }
  }

  matching_blocks_.push_back(
      Match(first_string_.size(), second_string_.size(), 0));
  sort(matching_blocks_.begin(), matching_blocks_.end(), CompareMatches);
  return matching_blocks_;
}

int SequenceMatcher::EditDistance() {
  // If edit distance is already calculated
  if (edit_distance_ >= 0) {
    return edit_distance_;
  }

  const int len_first = first_string_.size();
  const int len_second = second_string_.size();
  if (len_first == 0 || len_second == 0) {
    edit_distance_ = std::max(len_first, len_second);
    return edit_distance_;
  }

  // Memory for the dynamic programming:
  // dp[i + 1][j + 1] is the edit distane of first |i| characters of
  // |first_string_| and first |j| characters of |second_string_|
  int dp[len_first + 1][len_second + 1];

  // Initialize memory
  for (int i = 0; i < len_first + 1; i++) {
    dp[i][0] = i;
  }
  for (int j = 0; j < len_second + 1; j++) {
    dp[0][j] = j;
  }

  // Calculate the edit distance
  for (int i = 1; i < len_first + 1; i++) {
    for (int j = 1; j < len_second + 1; j++) {
      const int cost = first_string_[i - 1] == second_string_[j - 1] ? 0 : 1;
      // Insertion and deletion
      dp[i][j] = std::min(dp[i - 1][j], dp[i][j - 1]) + 1;
      // Substitution
      dp[i][j] = std::min(dp[i][j], dp[i - 1][j - 1] + cost);
      // Transposition
      if (i > 1 && j > 1 && first_string_[i - 2] == second_string_[j - 1] &&
          first_string_[i - 1] == second_string_[j - 2]) {
        dp[i][j] = std::min(dp[i][j], dp[i - 2][j - 2] + cost);
      }
    }
  }
  edit_distance_ = dp[len_first][len_second];
  return edit_distance_;
}

double SequenceMatcher::Ratio() {
  if (use_edit_distance_) {
    if (edit_distance_ratio_ < 0) {
      const int edit_distance = EditDistance();
      edit_distance_ratio_ =
          1.0 - static_cast<double>(edit_distance) /
                    (first_string_.size() + second_string_.size());
    }
    return edit_distance_ratio_;
  }

  // Uses block matching to calculate ratio.
  if (block_matching_ratio_ < 0) {
    int sum_match = 0;
    const int sum_length = first_string_.size() + second_string_.size();
    DCHECK_NE(sum_length, 0);
    for (const auto& match : GetMatchingBlocks()) {
      sum_match += match.length;
    }
    block_matching_ratio_ = 2.0 * sum_match / sum_length;
  }
  return block_matching_ratio_;
}

}  // namespace app_list
