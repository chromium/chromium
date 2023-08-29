// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/diff.h"

#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"

namespace ash::language_packs {

StringsDiff::StringsDiff(base::flat_set<std::string> remove,
                         base::flat_set<std::string> add)
    : remove(std::move(remove)), add(std::move(add)) {}
StringsDiff::StringsDiff(StringsDiff&&) = default;
StringsDiff::~StringsDiff() = default;

StringsDiff ComputeStringsDiff(base::span<const std::string> current,
                               base::span<const std::string> target) {
  std::vector<std::string> remove;
  std::vector<std::string> add;

  // Sort, unique and copy the input strings.
  // These copied strings will be moved to `add` and `remove` as necessary.
  //
  // As we plan on `std::move`ing things out of this container, we cannot use
  // a `base::flat_set` here as `std::move`ing the entries of the set will
  // invalidate the sorted invariant of the set. However, we still use
  // `base::flat_set` here as a clean way of sorting, uniquing and copying the
  // input strings.
  std::vector<std::string> current_set =
      base::flat_set<std::string>(current.begin(), current.end()).extract();
  std::vector<std::string> target_set =
      base::flat_set<std::string>(target.begin(), target.end()).extract();

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
