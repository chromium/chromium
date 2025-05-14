// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "components/attribution_reporting/privacy_math.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace attribution_reporting {
namespace {

void GetKCombinationAtIndex(uint32_t combination_index, uint32_t k) {
  internal::GetKCombinationAtIndex(combination_index, k);
}

FUZZ_TEST(PrivacyMathTest, GetKCombinationAtIndex)
    .WithDomains(
        /*combination_index=*/fuzztest::Arbitrary<uint32_t>(),
        /*k=*/fuzztest::InRange<uint32_t>(0, 20));

}  // namespace
}  // namespace attribution_reporting
