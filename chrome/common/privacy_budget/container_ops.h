// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRIVACY_BUDGET_CONTAINER_OPS_H_
#define CHROME_COMMON_PRIVACY_BUDGET_CONTAINER_OPS_H_

#include <algorithm>
#include <cstddef>
#include <vector>

#include "base/containers/contains.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"

namespace internal {
// Note that these implementations aren't meant for to be used outside of the
// narrow usage within this directory.

// Moves a random subset of |num| elements from |from| to |to|. The type |T|
// must satisfy |UnorderedAssociativeContainer|.
//
// Returns true if any elements were moved.
template <typename T, typename V = typename T::value_type>
bool ExtractRandomSubset(T* from, T* to, size_t num) {
  if (num == 0 || from->empty())
    return false;
  std::vector<V> copy(from->begin(), from->end());
  if (num < copy.size())
    base::RandomShuffle(copy.begin(), copy.end());
  for (auto to_remove_it = copy.begin(); num > 0 && to_remove_it != copy.end();
       ++to_remove_it, --num) {
    from->erase(*to_remove_it);
    to->insert(*to_remove_it);
  }
  return true;
}

// Extracts elements from |from| that matches predicate |pred| and places them
// in |to|.
//
// Returns true if any elements were moved.
template <typename T, typename Predicate, typename V = typename T::value_type>
bool ExtractIf(T* from, T* to, Predicate predicate) {
  std::vector<V> to_be_moved;
  to_be_moved.reserve(from->size());
  std::copy_if(from->begin(), from->end(),
               std::inserter(to_be_moved, to_be_moved.end()), predicate);
  for (const auto& v : to_be_moved)
    from->erase(v);
  to->insert(to_be_moved.begin(), to_be_moved.end());
  return !to_be_moved.empty();
}

// Removes elements from |right| that also exist in |left|.
//
// Returns true if |right| was modified.
//
// Different from std::set_difference in that it does not assume any ordering of
// elements.
template <typename T, typename V = typename T::value_type>
bool SubtractLeftFromRight(const T& left, T* right) {
  const auto previous_size = right->size();
  base::EraseIf(*right, [&](const V& v) { return base::Contains(left, v); });
  return previous_size != right->size();
}

// Determine if the intersection of |left| and |right| is non-empty. Notably
// unlike std::set_intersection<> does not assume any ordering of elements.
template <typename T, typename V = typename T::value_type>
bool Intersects(const T& left, const T& right) {
  if (left.size() <= right.size()) {
    return std::any_of(left.begin(), left.end(), [&](const V& candidate) {
      return base::Contains(right, candidate);
    });
  } else {
    return Intersects(right, left);
  }
}

}  // namespace internal

#endif  // CHROME_COMMON_PRIVACY_BUDGET_CONTAINER_OPS_H_
