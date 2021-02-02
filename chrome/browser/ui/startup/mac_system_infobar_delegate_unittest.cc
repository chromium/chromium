// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/mac_system_infobar_delegate.h"
#include "base/mac/mac_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using CPUType = base::mac::CPUType;

void ForceArchForTest(CPUType type) {
  const char kSwitch[] = "force-mac-system-infobar";
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (type == CPUType::kArm)
    command_line->AppendSwitchASCII(kSwitch, "arm");
  else if (type == CPUType::kTranslatedIntel)
    command_line->AppendSwitchASCII(kSwitch, "rosetta");
  else if (type == CPUType::kIntel)
    command_line->AppendSwitchASCII(kSwitch, "intel");
}

class MacSystemInfoBarDelegateArmTest : public testing::Test {
 public:
  void SetUp() override {
    base::FieldTrialParams params;
    params["enable_arm"] = "true";
    features_.InitAndEnableFeatureWithParameters(
        MacSystemInfoBarDelegate::kMacSystemInfoBar, params);
  }

 private:
  base::test::ScopedFeatureList features_;
};

class MacSystemInfoBarDelegateRosettaTest : public testing::Test {
 public:
  void SetUp() override {
    base::FieldTrialParams params;
    params["enable_rosetta"] = "true";
    features_.InitAndEnableFeatureWithParameters(
        MacSystemInfoBarDelegate::kMacSystemInfoBar, params);
  }

 private:
  base::test::ScopedFeatureList features_;
};

class MacSystemInfoBarDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    features_.InitAndEnableFeature(MacSystemInfoBarDelegate::kMacSystemInfoBar);
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Test that the enable/disable logic works properly on Arm.
TEST_F(MacSystemInfoBarDelegateArmTest, EnabledForArm) {
  ForceArchForTest(CPUType::kArm);
  ASSERT_TRUE(MacSystemInfoBarDelegate::ShouldShow());

  ForceArchForTest(CPUType::kTranslatedIntel);
  EXPECT_FALSE(MacSystemInfoBarDelegate::ShouldShow());

  ForceArchForTest(CPUType::kIntel);
  EXPECT_FALSE(MacSystemInfoBarDelegate::ShouldShow());
}

// Test that the enable/disable logic works properly on Rosetta.
TEST_F(MacSystemInfoBarDelegateRosettaTest, EnabledForRosetta) {
  ForceArchForTest(CPUType::kTranslatedIntel);
  ASSERT_TRUE(MacSystemInfoBarDelegate::ShouldShow());

  ForceArchForTest(CPUType::kArm);
  EXPECT_FALSE(MacSystemInfoBarDelegate::ShouldShow());

  ForceArchForTest(CPUType::kIntel);
  EXPECT_FALSE(MacSystemInfoBarDelegate::ShouldShow());
}

// Test that nothing is enabled when the feature is disabled.
TEST_F(MacSystemInfoBarDelegateTest, NotEnabledAnywhere) {
  ForceArchForTest(CPUType::kTranslatedIntel);
  EXPECT_FALSE(MacSystemInfoBarDelegate::ShouldShow());

  ForceArchForTest(CPUType::kArm);
  EXPECT_FALSE(MacSystemInfoBarDelegate::ShouldShow());

  ForceArchForTest(CPUType::kIntel);
  EXPECT_FALSE(MacSystemInfoBarDelegate::ShouldShow());
}

}  // namespace
