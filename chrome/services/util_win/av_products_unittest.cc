// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/av_products.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct Testcase {
  const char* input;
  const char* output;
};

}  // namespace

TEST(AvProductsTest, DISABLED_ResultCodeHistogram) {
  base::win::ScopedCOMInitializer scoped_com_initializer;
  base::HistogramTester histograms;

  // The parameter doesn't matter as only the recorded histogram is checked,
  // which doesn't depend on its value.
  GetAntiVirusProducts(false);

  internal::ResultCode expected_result = internal::ResultCode::kSuccess;
  if (base::win::OSInfo::GetInstance()->version_type() ==
      base::win::SUITE_SERVER) {
    expected_result = internal::ResultCode::kWSCNotAvailable;
  }
  histograms.ExpectUniqueSample("UMA.AntiVirusMetricsProvider.Result",
                                expected_result, 1);
}

TEST(AvProductsTest, StripProductVersion) {
  const Testcase testcases[] = {
      {"", ""},
      {" ", ""},
      {"1.0 AV 2.0", "1.0 AV"},
      {"Anti  Virus", "Anti Virus"},
      {"Windows Defender", "Windows Defender"},
      {"McAfee AntiVirus has a space at the end ",
       "McAfee AntiVirus has a space at the end"},
      {"ESET NOD32 Antivirus 8.0", "ESET NOD32 Antivirus"},
      {"Norton 360", "Norton 360"},
      {"ESET Smart Security 9.0.381.1", "ESET Smart Security"},
      {"Trustwave AV 3_0_2547", "Trustwave AV"},
      {"nProtect Anti-Virus/Spyware V4.0", "nProtect Anti-Virus/Spyware"},
      {"ESET NOD32 Antivirus 9.0.349.15P", "ESET NOD32 Antivirus"}};

  for (const auto& testcase : testcases) {
    auto output = internal::TrimVersionOfAvProductName(testcase.input);
    EXPECT_STREQ(testcase.output, output.c_str());
  }
}
