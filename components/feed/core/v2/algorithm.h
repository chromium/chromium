// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_ALGORITHM_H_
#define COMPONENTS_FEED_CORE_V2_ALGORITHM_H_

namespace feed {

// Compares two sorted ranges. Iterates each range, calling |fn()| for all
// items. |fn()| takes the parameters (left_pointer, right_pointer). A null
// value is passed in for items not appearing in either the left or right range.
// For each call to |fn|, at least one parameter will be non-null.
template <typename ITER_A, typename ITER_B, typename VISITOR>
void DiffSortedRange(ITER_A left_begin,
                     ITER_A left_end,
                     ITER_B right_begin,
                     ITER_B right_end,
                     VISITOR fn) {
  auto left = left_begin;
  auto right = right_begin;
  decltype(&*left) kLeftNull = nullptr;
  decltype(&*right) kRightNull = nullptr;
  for (;;) {
    bool left_at_end = left == left_end, right_at_end = right == right_end;
    if (left_at_end && right_at_end)
      break;

    if (!left_at_end && (right_at_end || *left < *right)) {
      fn(&*left, kRightNull);
      ++left;
      continue;
    }
    if (!right_at_end && (left_at_end || *right < *left)) {
      fn(kLeftNull, &*right);
      ++right;
      continue;
    }

    fn(&*left, &*right);
    ++left;
    ++right;
  }
}

}  // namespace feed
#endif  // COMPONENTS_FEED_CORE_V2_ALGORITHM_H_
