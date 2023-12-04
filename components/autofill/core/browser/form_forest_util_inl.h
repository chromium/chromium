// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_FOREST_UTIL_INL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_FOREST_UTIL_INL_H_

#include <functional>
#include <iterator>

namespace autofill {
namespace internal {

// Determines the difference set of |r1| minus |r2| (i.e., r1 \ r2) (after
// applying the projection |proj| to the members of |r1| and |r2|) and calls
// |fun| for the elements in that difference.
//
// At most |r1| * |r2| comparisons.
// At most |r2| comparisons if |r1| is a subsequence of |r2|.
//
// Unlike for base::ranges::set_difference, |r1| and |r2| do not need to be
// sorted. Aside from the order of the |fun| calls,
//   base::ranges::sort(r1, {}, proj);
//   base::ranges::sort(r2, {}, proj);
//   std::vector<decltype(fun(T()))> diff;
//   base::ranges::set_difference(r1, r2, std::back_inserter(diff), {}, proj);
//   base::ranges::for_each(diff, fun);
// is equivalent to
//   for_each_in_set_difference(r1, r2, fun, proj).
//
// This function is in the header so it can be unittested.
template <typename Range1,
          typename Range2,
          typename Fun,
          typename Proj = std::identity>
void for_each_in_set_difference(Range1&& r1,
                                Range2&& r2,
                                Fun fun,
                                Proj proj = {}) {
  size_t offset = 0;

  // Searches for |x| in |r|. Starts at |offset| and then wraps around.
  // Stores a found position (modulo |size|) in |offset| for the next call.
  auto Contains = [&proj, &offset](const Range2& r, auto&& x) {
    size_t size = std::distance(r.begin(), r.end());
    for (size_t num = 0; num < size; ++num) {
      size_t index = (offset + num) % size;
      auto& y = std::invoke(proj, *(r.begin() + index));
      if (x == y) {
        offset = index + 1;
        return true;
      }
    }
    return false;
  };

  for (auto& x1 : r1) {
    auto& x = std::invoke(proj, x1);
    if (!Contains(r2, x)) {
      std::invoke(fun, x);
    }
  }
}

}  // namespace internal
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_FOREST_UTIL_INL_H_
