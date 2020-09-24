// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/pattern_provider/pattern_provider.h"

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

bool operator==(const MatchingPattern& mp1, const MatchingPattern& mp2) {
  return (mp1.language == mp2.language &&
          mp1.match_field_attributes == mp2.match_field_attributes &&
          mp1.match_field_input_types == mp2.match_field_input_types &&
          mp1.negative_pattern == mp2.negative_pattern &&
          mp1.pattern_identifier == mp2.pattern_identifier &&
          mp1.positive_pattern == mp2.positive_pattern &&
          mp1.positive_score == mp2.positive_score);
}

TEST(AutofillPatternProvider, Single_Match) {
  MatchingPattern kCompanyPatternEn = GetCompanyPatternEn();
  MatchingPattern kCompanyPatternDe = GetCompanyPatternDe();
  PatternProvider* pattern_provider = PatternProvider::getInstance();

  ASSERT_TRUE(pattern_provider->GetMatchPatterns("COMPANY_NAME", "EN").size() >
              0);
  EXPECT_EQ(pattern_provider->GetMatchPatterns("COMPANY_NAME", "EN")[0],
            kCompanyPatternEn);
}

}  // namespace autofill