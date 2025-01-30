// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_deduplication_metrics.h"

#include <algorithm>
#include <limits>

#include "base/containers/contains.h"
#include "base/containers/span.h"

namespace autofill::autofill_metrics {

int GetDuplicationRank(
    base::span<const DifferingProfileWithTypeSet> min_incompatible_sets) {
  // All elements of `min_incompatible_sets` have the same size.
  return min_incompatible_sets.empty()
             ? std::numeric_limits<int>::max()
             : min_incompatible_sets.back().field_type_set.size();
}

}  // namespace autofill::autofill_metrics
