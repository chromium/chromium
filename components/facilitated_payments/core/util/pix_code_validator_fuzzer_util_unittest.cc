// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/util/pix_code_validator_fuzzer_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace payments::facilitated {
namespace {

TEST(PixCodeValidatorFuzzerUtilTest, ValidRegexPattern) {
  EXPECT_TRUE(re2::RE2::FullMatch("00020126180014br.gov.bcb.pix63041D3D",
                                  kPixCodeValidatorFuzzerDomainRegexPattern));
}

TEST(PixCodeValidatorFuzzerUtilTest, ValidRegexPatternWithUpperCase) {
  EXPECT_TRUE(re2::RE2::FullMatch("00020126180014BR.GOV.BCB.PIX63041D3D",
                                  kPixCodeValidatorFuzzerDomainRegexPattern));
}

TEST(PixCodeValidatorFuzzerUtilTest, InValidRegexPattern) {
  EXPECT_FALSE(re2::RE2::FullMatch("A0020126180014br.gov.bcb.pix63041D3D",
                                   kPixCodeValidatorFuzzerDomainRegexPattern));
}

}  // namespace
}  // namespace payments::facilitated
