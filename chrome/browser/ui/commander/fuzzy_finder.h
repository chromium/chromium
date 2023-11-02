// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_FUZZY_FINDER_H_
#define CHROME_BROWSER_UI_COMMANDER_FUZZY_FINDER_H_

#include <string>
#include <vector>

#include "ui/gfx/range/range.h"

namespace commander {

class FuzzyFinder {
 public:
  explicit FuzzyFinder(const std::u16string& needle);
  ~FuzzyFinder();
  FuzzyFinder(const FuzzyFinder& other) = delete;
  FuzzyFinder& operator=(const FuzzyFinder& other) = delete;

  // Returns a score from 0 to 1 based on how well |needle_| matches |haystack|.
  // 0 means no match. |matched_ranges| will be filled with the ranges of
  // |haystack| that match |needle| so they can be highlighted in the UI; see
  // comment on commander::CommandItem |matched_ranges| for a worked example.
  double Find(const std::u16string& haystack,
              std::vector<gfx::Range>* matched_ranges);

 private:
  // Implementation of the O(mn) matching algorithm. Only run if:
  // - `needle` is smaller than `haystack`
  // - `needle` is longer than a single character
  // - `needle` is not an exact prefix of `haystack`
  // - every code unit in `needle` is present in haystack, in the order that
  //   they appear in `needle`.
  // - `needle` and `haystack` are not longer than some maximum size (subject to
  //    change but currently 16 for `needle` and `1024` for haystack).
  // See fuzzy_finder.md for full details.
  double MatrixMatch(const std::u16string& needle,
                     const std::u16string& haystack,
                     std::vector<gfx::Range>* matched_ranges);
  // Case-folded input string.
  std::u16string needle_;
  // Scratch space for MatrixMatch().
  std::vector<int> score_matrix_;
  std::vector<int> consecutive_matrix_;
  std::vector<bool> word_boundaries_;
  std::vector<size_t> codepoint_to_offset_;
};

}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_FUZZY_FINDER_H_
