// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/expired_histograms_checker.h"

#include "base/metrics/metrics_hashes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

TEST(ExpiredHistogramsCheckerTests, BasicTest) {
  uint32_t expired_hashes[] = {1, 2, 3};
  std::string allowlist_str = "";
  ExpiredHistogramsChecker checker(expired_hashes, allowlist_str);

  EXPECT_TRUE(checker.ShouldRecord(0));
  EXPECT_FALSE(checker.ShouldRecord(3));
}

TEST(ExpiredHistogramsCheckerTests, AllowlistTest) {
  std::string hist1 = "hist1";
  std::string hist2 = "hist2";
  std::string hist3 = "hist3";
  std::string hist4 = "hist4";

  uint32_t expired_hashes[] = {base::HashMetricNameAs32Bits(hist1),
                               base::HashMetricNameAs32Bits(hist2)};
  std::string allowlist_str = hist2 + "," + hist4;
  ExpiredHistogramsChecker checker(expired_hashes, allowlist_str);

  EXPECT_FALSE(checker.ShouldRecord(base::HashMetricNameAs32Bits(hist1)));
  EXPECT_TRUE(checker.ShouldRecord(base::HashMetricNameAs32Bits(hist2)));
  EXPECT_TRUE(checker.ShouldRecord(base::HashMetricNameAs32Bits(hist3)));
  EXPECT_TRUE(checker.ShouldRecord(base::HashMetricNameAs32Bits(hist4)));
}

}  // namespace metrics
