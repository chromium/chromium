// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_regex_provider.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Tests the caching of a compiled regular expression.
TEST(AutofillStructuredAddressRegExProvider, IsRegExCached) {
  auto* g_pattern_provider = StructuredAddressesRegExProvider::Instance();
  EXPECT_FALSE(g_pattern_provider->IsCachedForTesting(RegEx::kSingleWord));

  ASSERT_TRUE(StructuredAddressesRegExProvider::Instance()
                  ->GetRegEx(RegEx::kSingleWord)
                  ->ok());
  EXPECT_TRUE(g_pattern_provider->IsCachedForTesting(RegEx::kSingleWord));
}

// Builds all expressions and verifes that the result is not a nullptr.
TEST(AutofillStructuredAddressRegExProvider, BuildAllRegExs) {
  for (int i = 0; i <= static_cast<int>(RegEx::kLastRegEx); i++) {
    auto* g_pattern_provider = StructuredAddressesRegExProvider::Instance();
    EXPECT_NE(g_pattern_provider->GetRegEx(static_cast<RegEx>(i)), nullptr);
  }
}

}  // namespace autofill
