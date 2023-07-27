// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/blocked_iph_features.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

BASE_FEATURE(kBlockedIphFeaturesTestFeature1,
             "BlockedIphFeaturesTestFeature1",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kBlockedIphFeaturesTestFeature2,
             "BlockedIphFeaturesTestFeature2",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBlockedIphFeaturesTestFeature3,
             "BlockedIphFeaturesTestFeature3",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace

class BlockedIphFeaturesTest : public testing::Test {
 public:
  void SetUp() override { ClearData(); }

  void TearDown() override {
    {
      auto* const allowed = BlockedIphFeatures::GetInstance();
      base::AutoLock lock(allowed->GetLock());
      if (expect_refcount_on_teardown_) {
        EXPECT_GT(allowed->global_block_count_, 0U);
      } else {
        EXPECT_EQ(0U, allowed->global_block_count_);
      }
    }

    // Clear out the map data so it doesn't pollute later tests.
    ClearData();
  }

  void ExpectRefCountOnTeardown() { expect_refcount_on_teardown_ = true; }

  static const char* GetCommandLineSwitch() {
    return BlockedIphFeatures::kPropagateIPHForTestingSwitch;
  }

 private:
  void ClearData() {
    auto* const allowed = BlockedIphFeatures::GetInstance();
    base::AutoLock lock(allowed->GetLock());
    allowed->read_from_command_line_ = false;
    allowed->global_block_count_ = 0U;
    allowed->allow_feature_counts_.clear();
  }

  bool expect_refcount_on_teardown_ = false;
};

TEST_F(BlockedIphFeaturesTest, IsFeatureBlockedDefaultValue) {
  auto* const blocked = BlockedIphFeatures::GetInstance();
  base::AutoLock lock(blocked->GetLock());

  // Allowed should be irrespective of enabled or disabled.
  EXPECT_FALSE(blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature1.name));
  EXPECT_FALSE(blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature2.name));
}

TEST_F(BlockedIphFeaturesTest, IsFeatureBlockedWithEmptyScope) {
  {
    test::ScopedIphFeatureList list;
    list.InitWithNoFeaturesAllowed();

    // Blocked should be irrespective of enabled or disabled.
    auto* const blocked = BlockedIphFeatures::GetInstance();
    base::AutoLock lock(blocked->GetLock());
    EXPECT_TRUE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature1.name));
    EXPECT_TRUE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature2.name));
  }

  {
    // Now no one is blocking IPH.
    auto* const blocked = BlockedIphFeatures::GetInstance();
    base::AutoLock lock(blocked->GetLock());
    EXPECT_FALSE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature1.name));
    EXPECT_FALSE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature2.name));
  }
}

TEST_F(BlockedIphFeaturesTest, IsFeatureBlockedInnerScopeHasException) {
  test::ScopedIphFeatureList list1;
  list1.InitWithNoFeaturesAllowed();
  {
    test::ScopedIphFeatureList list2;
    list2.InitAndEnableFeatures({kBlockedIphFeaturesTestFeature1});

    auto* const blocked = BlockedIphFeatures::GetInstance();
    base::AutoLock lock(blocked->GetLock());

    // This feature is explicitly allowed.
    EXPECT_FALSE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature1.name));
    EXPECT_TRUE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature2.name));
  }

  {
    // Now no features are allowed.
    auto* const blocked = BlockedIphFeatures::GetInstance();
    base::AutoLock lock(blocked->GetLock());
    EXPECT_TRUE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature1.name));
    EXPECT_TRUE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature2.name));
  }
}

TEST_F(BlockedIphFeaturesTest, IsFeatureBlockedOuterScopeHasException) {
  test::ScopedIphFeatureList list1;
  list1.InitAndEnableFeatures({kBlockedIphFeaturesTestFeature1});
  {
    test::ScopedIphFeatureList list2;
    list2.InitWithNoFeaturesAllowed();

    auto* const blocked = BlockedIphFeatures::GetInstance();
    base::AutoLock lock(blocked->GetLock());

    // This feature is explicitly allowed.
    EXPECT_FALSE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature1.name));
    EXPECT_TRUE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature2.name));
  }

  {
    // Allowed feature should persist.
    auto* const blocked = BlockedIphFeatures::GetInstance();
    base::AutoLock lock(blocked->GetLock());
    EXPECT_FALSE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature1.name));
    EXPECT_TRUE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature2.name));
  }
}

TEST_F(BlockedIphFeaturesTest, IsFeatureBlockedMultipleExceptions) {
  test::ScopedIphFeatureList list1;
  list1.InitAndEnableFeatures({kBlockedIphFeaturesTestFeature1});
  {
    test::ScopedIphFeatureList list2;
    list2.InitAndEnableFeatures({kBlockedIphFeaturesTestFeature2});

    auto* const blocked = BlockedIphFeatures::GetInstance();
    base::AutoLock lock(blocked->GetLock());

    // Both features are allowed.
    EXPECT_FALSE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature1.name));
    EXPECT_FALSE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature2.name));
  }

  {
    // Outer allowed feature should persist.
    auto* const blocked = BlockedIphFeatures::GetInstance();
    base::AutoLock lock(blocked->GetLock());
    EXPECT_FALSE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature1.name));
    EXPECT_TRUE(
        blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature2.name));
  }
}
TEST_F(BlockedIphFeaturesTest, MaybeWriteToCommandLineWithNoBlocking) {
  auto* const blocked = BlockedIphFeatures::GetInstance();
  base::AutoLock lock(blocked->GetLock());
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  blocked->MaybeWriteToCommandLine(command_line);
  EXPECT_FALSE(command_line.HasSwitch(GetCommandLineSwitch()));
  EXPECT_FALSE(command_line.HasSwitch(switches::kEnableFeatures));
}

TEST_F(BlockedIphFeaturesTest, MaybeWriteToCommandLineBlockingAllIph) {
  test::ScopedIphFeatureList list;
  list.InitWithNoFeaturesAllowed();

  auto* const blocked = BlockedIphFeatures::GetInstance();
  base::AutoLock lock(blocked->GetLock());
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  blocked->MaybeWriteToCommandLine(command_line);
  EXPECT_TRUE(command_line.HasSwitch(GetCommandLineSwitch()));
  EXPECT_TRUE(command_line.GetSwitchValueASCII(GetCommandLineSwitch()).empty());
  EXPECT_FALSE(command_line.HasSwitch(switches::kEnableFeatures));
}

TEST_F(BlockedIphFeaturesTest, MaybeWriteToCommandLineBlockingAllButOneIph) {
  test::ScopedIphFeatureList list;
  list.InitAndEnableFeatures({kBlockedIphFeaturesTestFeature1});

  auto* const blocked = BlockedIphFeatures::GetInstance();
  base::AutoLock lock(blocked->GetLock());
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  blocked->MaybeWriteToCommandLine(command_line);
  EXPECT_TRUE(command_line.HasSwitch(GetCommandLineSwitch()));
  EXPECT_EQ(kBlockedIphFeaturesTestFeature1.name,
            command_line.GetSwitchValueASCII(GetCommandLineSwitch()));
  EXPECT_TRUE(command_line.HasSwitch(switches::kEnableFeatures));
  EXPECT_EQ(kBlockedIphFeaturesTestFeature1.name,
            command_line.GetSwitchValueASCII(switches::kEnableFeatures));
}

TEST_F(BlockedIphFeaturesTest, MaybeWriteToCommandLineBlockingAllButTwoIph) {
  test::ScopedIphFeatureList list;
  list.InitAndEnableFeatures(
      {kBlockedIphFeaturesTestFeature1, kBlockedIphFeaturesTestFeature2});

  auto* const blocked = BlockedIphFeatures::GetInstance();
  base::AutoLock lock(blocked->GetLock());
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  blocked->MaybeWriteToCommandLine(command_line);
  const std::string expected =
      base::JoinString({kBlockedIphFeaturesTestFeature1.name,
                        kBlockedIphFeaturesTestFeature2.name},
                       ",");
  EXPECT_TRUE(command_line.HasSwitch(GetCommandLineSwitch()));
  EXPECT_EQ(expected, command_line.GetSwitchValueASCII(GetCommandLineSwitch()));
  EXPECT_TRUE(command_line.HasSwitch(switches::kEnableFeatures));
  EXPECT_EQ(expected,
            command_line.GetSwitchValueASCII(switches::kEnableFeatures));
}

TEST_F(BlockedIphFeaturesTest,
       MaybeWriteToCommandLinePreviouslyHadEnabledFeatures) {
  test::ScopedIphFeatureList list;
  list.InitAndEnableFeatures({kBlockedIphFeaturesTestFeature2});

  auto* const blocked = BlockedIphFeatures::GetInstance();
  base::AutoLock lock(blocked->GetLock());
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  // Add feature 1 to the general enabled list.
  command_line.AppendSwitchASCII(switches::kEnableFeatures,
                                 kBlockedIphFeaturesTestFeature1.name);
  blocked->MaybeWriteToCommandLine(command_line);

  // Only the explicitly allowed IPH should be present in the BlockedIphFeatures
  // command-line switch.
  EXPECT_TRUE(command_line.HasSwitch(GetCommandLineSwitch()));
  EXPECT_EQ(kBlockedIphFeaturesTestFeature2.name,
            command_line.GetSwitchValueASCII(GetCommandLineSwitch()));

  // Both values should be in the final enabled features argument.
  EXPECT_TRUE(command_line.HasSwitch(switches::kEnableFeatures));
  const std::string expected =
      base::JoinString({kBlockedIphFeaturesTestFeature1.name,
                        kBlockedIphFeaturesTestFeature2.name},
                       ",");
  EXPECT_EQ(expected,
            command_line.GetSwitchValueASCII(switches::kEnableFeatures));
}

TEST_F(BlockedIphFeaturesTest, ReadFromCommandLineBlockAll) {
  ExpectRefCountOnTeardown();

  // Append a switch that indicates blocked IPH with no exceptions.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(GetCommandLineSwitch());

  auto* const blocked = BlockedIphFeatures::GetInstance();
  base::AutoLock lock(blocked->GetLock());
  EXPECT_TRUE(blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature1.name));
  EXPECT_TRUE(blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature2.name));
}

TEST_F(BlockedIphFeaturesTest, ReadFromCommandLineAllowOneIph) {
  ExpectRefCountOnTeardown();

  // Append a switch that indicates blocked IPH with one exceptions.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      GetCommandLineSwitch(), kBlockedIphFeaturesTestFeature1.name);

  auto* const blocked = BlockedIphFeatures::GetInstance();
  base::AutoLock lock(blocked->GetLock());
  EXPECT_FALSE(blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature1.name));
  EXPECT_TRUE(blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature2.name));
}

TEST_F(BlockedIphFeaturesTest, ReadFromCommandLineAllowTwoIph) {
  ExpectRefCountOnTeardown();

  // Append a switch that indicates blocked IPH with two exceptions.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      GetCommandLineSwitch(),
      base::JoinString({kBlockedIphFeaturesTestFeature1.name,
                        kBlockedIphFeaturesTestFeature2.name},
                       ","));

  auto* const blocked = BlockedIphFeatures::GetInstance();
  base::AutoLock lock(blocked->GetLock());
  EXPECT_FALSE(blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature1.name));
  EXPECT_FALSE(blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature2.name));

  // An unrelated IPH should still be blocked.
  EXPECT_TRUE(blocked->IsFeatureBlocked(kBlockedIphFeaturesTestFeature3.name));
}

}  // namespace feature_engagement
