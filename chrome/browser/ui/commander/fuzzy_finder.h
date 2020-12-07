// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_FUZZY_FINDER_H_
#define CHROME_BROWSER_UI_COMMANDER_FUZZY_FINDER_H_

#include <vector>

#include "base/strings/string16.h"
#include "ui/gfx/range/range.h"

namespace commander {

class FuzzyFinder {
 public:
  explicit FuzzyFinder(const base::string16& needle);
  ~FuzzyFinder() = default;
  FuzzyFinder(const FuzzyFinder& other) = delete;
  FuzzyFinder& operator=(const FuzzyFinder& other) = delete;

  // Returns a score from 0 to 1 based on how well |needle_| matches |haystack|.
  // 0 means no match. |matched_ranges| will be filled with the ranges of
  // |haystack| that match |needle| so they can be highlighted in the UI; see
  // comment on commander::CommandItem |matched_ranges| for a worked example.
  // TODO(lgrey): This currently uses an algorithm which is not guaranteed to
  // return the optimal match. Update this to use a more comprehensive method
  // when inputs are small enough.
  double Find(const base::string16& haystack,
              std::vector<gfx::Range>* matched_ranges);

 private:
  // Case-folded input string.
  base::string16 needle_;
};

}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_FUZZY_FINDER_H_
