// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <tuple>

#include "base/time/time.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/fuzz_utils.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/test_utils.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace attribution_reporting {
namespace {

fuzztest::Domain<base::Time> AnyTime() {
  return fuzztest::Map(
      [](int64_t micros) {
        return base::Time::FromDeltaSinceWindowsEpoch(
            base::Microseconds(micros));
      },
      fuzztest::Arbitrary<int64_t>());
}

// Ensures that `FilterData::Matches()` does not crash on the full range of
// inputs.
void Matches(const FilterData& filter_data,
             const mojom::SourceType source_type,
             const base::Time source_time,
             const base::Time trigger_time,
             const FilterPair& filter_pair) {
  std::ignore =
      filter_data.Matches(source_type, source_time, trigger_time, filter_pair);
}

FUZZ_TEST(FilterDataTest, Matches)
    .WithDomains(AnyFilterData(),
                 AnySourceType(),
                 AnyTime(),
                 AnyTime(),
                 AnyFilterPair());

}  // namespace
}  // namespace attribution_reporting
