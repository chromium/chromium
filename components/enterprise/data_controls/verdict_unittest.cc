// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/verdict.h"

#include <vector>

#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_controls {

namespace {

constexpr char kReportRuleID[] = "report_rule_id";
constexpr char kWarnRuleID[] = "warn_rule_id";
constexpr char kBlockRuleID[] = "block_rule_id";

constexpr char kReportRuleName[] = "report_rule_name";
constexpr char kWarnRuleName[] = "warn_rule_name";
constexpr char kBlockRuleName[] = "block_rule_name";

// Helpers to make the tests more concise.
Verdict NotSet() {
  return Verdict::NotSet();
}
Verdict Report() {
  return Verdict::Report({{kReportRuleID, kReportRuleName}});
}
Verdict Warn() {
  return Verdict::Warn({{kWarnRuleID, kWarnRuleName}});
}
Verdict Block() {
  return Verdict::Block({{kBlockRuleID, kBlockRuleName}});
}
Verdict Allow() {
  return Verdict::Allow();
}

}  // namespace

TEST(DataControlVerdictTest, Level) {
  ASSERT_EQ(NotSet().level(), Rule::Level::kNotSet);
  ASSERT_EQ(Report().level(), Rule::Level::kReport);
  ASSERT_EQ(Warn().level(), Rule::Level::kWarn);
  ASSERT_EQ(Block().level(), Rule::Level::kBlock);
  ASSERT_EQ(Allow().level(), Rule::Level::kAllow);
}

TEST(DataControlVerdictTest, MergedLevel_NotSet) {
  ASSERT_EQ(Verdict::Merge(NotSet(), NotSet()).level(), Rule::Level::kNotSet);
  ASSERT_EQ(Verdict::Merge(NotSet(), Report()).level(), Rule::Level::kReport);
  ASSERT_EQ(Verdict::Merge(NotSet(), Warn()).level(), Rule::Level::kWarn);
  ASSERT_EQ(Verdict::Merge(NotSet(), Block()).level(), Rule::Level::kBlock);
  ASSERT_EQ(Verdict::Merge(NotSet(), Allow()).level(), Rule::Level::kAllow);
}

TEST(DataControlVerdictTest, MergedLevel_Report) {
  ASSERT_EQ(Verdict::Merge(Report(), NotSet()).level(), Rule::Level::kReport);
  ASSERT_EQ(Verdict::Merge(Report(), Report()).level(), Rule::Level::kReport);
  ASSERT_EQ(Verdict::Merge(Report(), Warn()).level(), Rule::Level::kWarn);
  ASSERT_EQ(Verdict::Merge(Report(), Block()).level(), Rule::Level::kBlock);
  ASSERT_EQ(Verdict::Merge(Report(), Allow()).level(), Rule::Level::kAllow);
}

TEST(DataControlVerdictTest, MergedLevel_Warn) {
  ASSERT_EQ(Verdict::Merge(Warn(), NotSet()).level(), Rule::Level::kWarn);
  ASSERT_EQ(Verdict::Merge(Warn(), Report()).level(), Rule::Level::kWarn);
  ASSERT_EQ(Verdict::Merge(Warn(), Warn()).level(), Rule::Level::kWarn);
  ASSERT_EQ(Verdict::Merge(Warn(), Block()).level(), Rule::Level::kBlock);
  ASSERT_EQ(Verdict::Merge(Warn(), Allow()).level(), Rule::Level::kAllow);
}

TEST(DataControlVerdictTest, MergedLevel_Block) {
  ASSERT_EQ(Verdict::Merge(Block(), NotSet()).level(), Rule::Level::kBlock);
  ASSERT_EQ(Verdict::Merge(Block(), Report()).level(), Rule::Level::kBlock);
  ASSERT_EQ(Verdict::Merge(Block(), Warn()).level(), Rule::Level::kBlock);
  ASSERT_EQ(Verdict::Merge(Block(), Block()).level(), Rule::Level::kBlock);
  ASSERT_EQ(Verdict::Merge(Block(), Allow()).level(), Rule::Level::kAllow);
}

TEST(DataControlVerdictTest, MergedLevel_Allow) {
  ASSERT_EQ(Verdict::Merge(Allow(), NotSet()).level(), Rule::Level::kAllow);
  ASSERT_EQ(Verdict::Merge(Allow(), Report()).level(), Rule::Level::kAllow);
  ASSERT_EQ(Verdict::Merge(Allow(), Warn()).level(), Rule::Level::kAllow);
  ASSERT_EQ(Verdict::Merge(Allow(), Block()).level(), Rule::Level::kAllow);
  ASSERT_EQ(Verdict::Merge(Allow(), Allow()).level(), Rule::Level::kAllow);
}

TEST(DataControlVerdictTest, TriggeredRules) {
  ASSERT_TRUE(NotSet().triggered_rules().empty());
  ASSERT_TRUE(Allow().triggered_rules().empty());

  auto report = Report();
  EXPECT_EQ(report.triggered_rules().size(), 1u);
  EXPECT_TRUE(report.triggered_rules().count(kReportRuleID));
  EXPECT_EQ(report.triggered_rules().at(kReportRuleID), kReportRuleName);

  auto warn = Warn();
  EXPECT_EQ(warn.triggered_rules().size(), 1u);
  EXPECT_TRUE(warn.triggered_rules().count(kWarnRuleID));
  EXPECT_EQ(warn.triggered_rules().at(kWarnRuleID), kWarnRuleName);

  auto block = Block();
  EXPECT_EQ(block.triggered_rules().size(), 1u);
  EXPECT_TRUE(block.triggered_rules().count(kBlockRuleID));
  EXPECT_EQ(block.triggered_rules().at(kBlockRuleID), kBlockRuleName);
}

TEST(DataControlVerdictTest, MergedTriggeredRules) {
  // Two verdicts with the same triggered rule merge correctly and don't
  // internally duplicate the rule in two.
  auto merged_warnings = Verdict::Merge(Warn(), Warn());
  EXPECT_EQ(merged_warnings.triggered_rules().size(), 1u);
  EXPECT_TRUE(merged_warnings.triggered_rules().count(kWarnRuleID));
  EXPECT_EQ(merged_warnings.triggered_rules().at(kWarnRuleID), kWarnRuleName);

  // Merging three verdicts with different rules should rules in a verdict with
  // all three rules present.
  auto all_merged = Verdict::Merge(Warn(), Verdict::Merge(Report(), Block()));
  EXPECT_EQ(all_merged.triggered_rules().size(), 3u);
  EXPECT_TRUE(all_merged.triggered_rules().count(kReportRuleID));
  EXPECT_EQ(all_merged.triggered_rules().at(kReportRuleID), kReportRuleName);
  EXPECT_TRUE(all_merged.triggered_rules().count(kWarnRuleID));
  EXPECT_EQ(all_merged.triggered_rules().at(kWarnRuleID), kWarnRuleName);
  EXPECT_TRUE(all_merged.triggered_rules().count(kBlockRuleID));
  EXPECT_EQ(all_merged.triggered_rules().at(kBlockRuleID), kBlockRuleName);
}

}  // namespace data_controls
