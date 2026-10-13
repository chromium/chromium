// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"

namespace autofill {

// Returns true if `old_data` and `new_data` contain different items, ignoring
// the order of the items. Duplicates are taken into account, e.g. {a, a, b} and
// {a, b, b} are different.
//
// Each item is first mapped with `old_proj` or `new_proj`, and the results are
// sorted and compared using `comp`. Two items are considered equal if neither
// is ordered before the other, i.e. `!comp(a, b) && !comp(b, a)`. Separate
// projections allow comparing vectors of different element types, e.g.
// `std::vector<std::unique_ptr<T>>` from the database with `std::vector<T>`
// from sync. Examples:
//
//   // Items with comparison operators:
//   AreAnyItemsDifferent(old_data, new_data);
//
//   // Items with a Compare() method, old items owned by std::unique_ptr:
//   AreAnyItemsDifferent(
//       old_data, new_data,
//       [](const T& a, const T& b) { return a.Compare(b) < 0; },
//       [](const std::unique_ptr<T>& item) -> const T& { return *item; });
//
//   // Protos, compared by their serialized representation:
//   auto serialize = [](const Proto& p) { return p.SerializeAsString(); };
//   AreAnyItemsDifferent(old_data, new_data, {}, serialize, serialize);
template <class OldItem,
          class NewItem,
          class Comp = std::ranges::less,
          class OldProj = std::identity,
          class NewProj = std::identity>
bool AreAnyItemsDifferent(const std::vector<OldItem>& old_data,
                          const std::vector<NewItem>& new_data,
                          Comp comp = {},
                          OldProj old_proj = {},
                          NewProj new_proj = {}) {
  if (old_data.size() != new_data.size()) {
    return true;
  }

  auto old_sorted = base::ToVector(old_data, std::move(old_proj));
  auto new_sorted = base::ToVector(new_data, std::move(new_proj));
  std::ranges::sort(old_sorted, comp);
  std::ranges::sort(new_sorted, comp);
  auto equivalent = [&comp](const auto& a, const auto& b) {
    return !comp(a, b) && !comp(b, a);
  };
  return !std::ranges::equal(old_sorted, new_sorted, equivalent);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_UTIL_H_
