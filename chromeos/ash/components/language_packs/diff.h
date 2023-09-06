// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_DIFF_H_
#define CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_DIFF_H_

#include <string>

#include "base/containers/flat_set.h"

namespace ash::language_packs {

struct StringsDiff {
  base::flat_set<std::string> remove;
  base::flat_set<std::string> add;

  StringsDiff(base::flat_set<std::string> remove,
              base::flat_set<std::string> add);
  StringsDiff(StringsDiff&&);
  ~StringsDiff();
};

// Returns the set of strings that need to be added and removed from the set
// `current` to obtain the set `target`.
StringsDiff ComputeStringsDiff(base::flat_set<std::string> current,
                               base::flat_set<std::string> target);

}  // namespace ash::language_packs

#endif  // CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_DIFF_H_
