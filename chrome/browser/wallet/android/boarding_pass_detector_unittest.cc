// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/wallet/android/boarding_pass_detector.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {
class BoardingPassDetectorTest : public ::testing::Test {
 public:
  void SetAllowList(const std::string& allowlist) {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kBoardingPassDetector,
        {{features::kBoardingPassDetectorUrlParam.name, allowlist}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BoardingPassDetectorTest, ShouldDetect) {
  SetAllowList("https://aa.com/adc, https://www.google.com/boarding");

  EXPECT_TRUE(BoardingPassDetector::ShouldDetect("https://aa.com/adc"));
  EXPECT_TRUE(BoardingPassDetector::ShouldDetect(
      "https://www.google.com/boarding/abc"));
}

TEST_F(BoardingPassDetectorTest, ShouldNotDetect) {
  SetAllowList("https://aa.com/adc, https://www.google.com/boarding");

  EXPECT_FALSE(BoardingPassDetector::ShouldDetect("https://aa.com/"));
  EXPECT_FALSE(
      BoardingPassDetector::ShouldDetect("https://www.google.com/abc"));
}
}  // namespace wallet
