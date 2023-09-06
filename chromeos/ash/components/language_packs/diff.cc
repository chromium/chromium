// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/diff.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/containers/flat_set.h"

namespace ash::language_packs {

StringsDiff::StringsDiff(base::flat_set<std::string> remove,
                         base::flat_set<std::string> add)
    : remove(std::move(remove)), add(std::move(add)) {}
StringsDiff::StringsDiff(StringsDiff&&) = default;
StringsDiff::~StringsDiff() = default;

StringsDiff ComputeStringsDiff(base::flat_set<std::string> current,
                               base::flat_set<std::string> target) {
  std::vector<std::string> remove;
  std::vector<std::string> add;

  // As we plan on `std::move`ing things out of `current` and `target`, we
  // cannot maintain the sorted invariant of the `flat_set`, as `std::move`
  // mutates the moved value to be in an unspecified state.
  // As we already have ownership over `current` and `target`, we can extract
  // the underlying vector and mutate it as we wish.
  // Doing so allows users of this function to pass in an already
  // sorted-and-uniqued container without the overhead of doing another
  // sort-and-unique pass if needed.
  std::vector<std::string> current_set = std::move(current).extract();
  std::vector<std::string> target_set = std::move(target).extract();

  auto current_it = current_set.begin();
  const auto current_end = current_set.end();
  auto target_it = target_set.begin();
  const auto target_end = target_set.end();

  while (current_it != current_end && target_it != target_end) {
    if (*current_it < *target_it) {
      // `*target_it` could still be in `current`, but `*current_it` is
      // definitely not in `target`.
      // We should remove it.
      remove.push_back(std::move(*current_it));
      current_it++;
    } else if (*current_it > *target_it) {
      // `*current_it` could still be in `target`, but `*target_it` is
      // definitely not in `current`.
      // We should add it.
      add.push_back(std::move(*target_it));
      target_it++;
    } else {
      CHECK_EQ(*current_it, *target_it);
      current_it++;
      target_it++;
    }
  }

  // At least one of `current` and `target` are empty.
  while (current_it != current_end) {
    // We have things in `current` which aren't in `target`. Remove them.
    remove.push_back(std::move(*current_it));
    current_it++;
  }
  while (target_it != target_end) {
    // We have things in `target` which aren't in `current`. Add them.
    add.push_back(std::move(*target_it));
    target_it++;
  }

  // Both `add` and `remove` are guaranteed to be sorted and unique as we
  // processed them in order from sorted and unique sets.
  return {{base::sorted_unique, std::move(remove)},
          {base::sorted_unique, std::move(add)}};
}

}  // namespace ash::language_packs
