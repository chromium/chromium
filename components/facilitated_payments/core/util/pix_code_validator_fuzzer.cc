// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/facilitated_payments/core/util/pix_code_validator.h"
#include "components/facilitated_payments/core/util/pix_code_validator_fuzzer_util.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace payments::facilitated {

void IsValidPixCodeCanParseAnyString(const std::string& input) {
  PixCodeValidator::IsValidPixCode(input);
}

FUZZ_TEST(IsValidPixCodeTest, IsValidPixCodeCanParseAnyString)
    .WithDomains(fuzztest::InRegexp(kPixCodeValidatorFuzzerDomainRegexPattern))
    .WithSeeds({{""},
                {"000201260063041D3D"},
                {"00020126030014br.gov.bcb.pix63041D3D"},
                {"00020126030014BR.GOV.BCB.PIX63041D3D"},
                {"00020126180014br.gov.bcb.pix620063041D3D"},
                {"00020126180014br.gov.bcb.pix630"},
                {"00020126180014br.gov.bcb.pix63041D3"},
                {"00020126180014br.gov.bcb.pix63041D3D"},
                {"00020126180014br.gov.bcb.PIX63041D3D"},
                {"00020126180014br.gov.bcb.pix63051D3D"},
                {"00020126180014br.gov.bcb.pix64041D3D"},
                {"000201261801020063041D3D"},
                {"00020163041D3D"},
                {"000A0126180014br.gov.bcb.pix63041D3D"},
                {"01020126180014br.gov.bcb.pix63041D3D"}});

}  // namespace payments::facilitated
