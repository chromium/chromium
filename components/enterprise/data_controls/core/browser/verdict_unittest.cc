// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/verdict.h"

#include <vector>

#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_controls {

namespace {

constexpr size_t kReportRuleIndex = 0;
constexpr size_t kWarnRuleIndex = 1;
constexpr size_t kBlockRuleIndex = 2;

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
  return Verdict::Report({
      {kReportRuleIndex,
       {.rule_id = kReportRuleID, .rule_name = kReportRuleName}},
  });
}
Verdict Warn() {
  return Verdict::Warn({
      {kWarnRuleIndex, {.rule_id = kWarnRuleID, .rule_name = kWarnRuleName}},
  });
}
Verdict Block() {
  return Verdict::Block({
      {kBlockRuleIndex, {.rule_id = kBlockRuleID, .rule_name = kBlockRuleName}},
  });
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

TEST(DataControlVerdictTest, MergedPasteVerdictsLevel_NotSet) {
  ASSERT_EQ(Verdict::MergePasteVerdicts(NotSet(), NotSet()).level(),
            Rule::Level::kNotSet);
  ASSERT_EQ(Verdict::MergePasteVerdicts(NotSet(), Report()).level(),
            Rule::Level::kReport);
  ASSERT_EQ(Verdict::MergePasteVerdicts(NotSet(), Warn()).level(),
            Rule::Level::kWarn);
  ASSERT_EQ(Verdict::MergePasteVerdicts(NotSet(), Block()).level(),
            Rule::Level::kBlock);
  ASSERT_EQ(Verdict::MergePasteVerdicts(NotSet(), Allow()).level(),
            Rule::Level::kAllow);
}

TEST(DataControlVerdictTest, MergedPasteVerdictsLevel_Report) {
  ASSERT_EQ(Verdict::MergePasteVerdicts(Report(), NotSet()).level(),
            Rule::Level::kReport);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Report(), Report()).level(),
            Rule::Level::kReport);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Report(), Warn()).level(),
            Rule::Level::kWarn);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Report(), Block()).level(),
            Rule::Level::kBlock);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Report(), Allow()).level(),
            Rule::Level::kAllow);
}

TEST(DataControlVerdictTest, MergedPasteVerdictsLevel_Warn) {
  ASSERT_EQ(Verdict::MergePasteVerdicts(Warn(), NotSet()).level(),
            Rule::Level::kWarn);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Warn(), Report()).level(),
            Rule::Level::kWarn);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Warn(), Warn()).level(),
            Rule::Level::kWarn);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Warn(), Block()).level(),
            Rule::Level::kBlock);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Warn(), Allow()).level(),
            Rule::Level::kAllow);
}

TEST(DataControlVerdictTest, MergedPasteVerdictsLevel_Block) {
  ASSERT_EQ(Verdict::MergePasteVerdicts(Block(), NotSet()).level(),
            Rule::Level::kBlock);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Block(), Report()).level(),
            Rule::Level::kBlock);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Block(), Warn()).level(),
            Rule::Level::kBlock);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Block(), Block()).level(),
            Rule::Level::kBlock);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Block(), Allow()).level(),
            Rule::Level::kAllow);
}

TEST(DataControlVerdictTest, MergedPasteVerdictsLevel_Allow) {
  ASSERT_EQ(Verdict::MergePasteVerdicts(Allow(), NotSet()).level(),
            Rule::Level::kAllow);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Allow(), Report()).level(),
            Rule::Level::kAllow);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Allow(), Warn()).level(),
            Rule::Level::kAllow);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Allow(), Block()).level(),
            Rule::Level::kAllow);
  ASSERT_EQ(Verdict::MergePasteVerdicts(Allow(), Allow()).level(),
            Rule::Level::kAllow);
}

TEST(DataControlVerdictTest, MergedCopyWarningVerdictsLevel) {
  ASSERT_EQ(Verdict::MergeCopyWarningVerdicts(Warn(), Block()).level(),
            Rule::Level::kWarn);
  ASSERT_EQ(Verdict::MergeCopyWarningVerdicts(Warn(), Warn()).level(),
            Rule::Level::kWarn);
  ASSERT_EQ(Verdict::MergeCopyWarningVerdicts(Warn(), Report()).level(),
            Rule::Level::kWarn);
  ASSERT_EQ(Verdict::MergeCopyWarningVerdicts(Warn(), Allow()).level(),
            Rule::Level::kWarn);
  ASSERT_EQ(Verdict::MergeCopyWarningVerdicts(Warn(), NotSet()).level(),
            Rule::Level::kWarn);

  // This is enforced through a CHECK in `MergeCopyWarningVerdicts` as a
  // blocking "source-only verdict" shouldn't reach code that calls this
  // function.
  // ASSERT_EQ(Verdict::MergeCopyWarningVerdicts(Block(), Warn()),
  //           Rule::Level::kWarn);
  ASSERT_EQ(Verdict::MergeCopyWarningVerdicts(Warn(), Warn()).level(),
            Rule::Level::kWarn);
  ASSERT_EQ(Verdict::MergeCopyWarningVerdicts(Report(), Warn()).level(),
            Rule::Level::kWarn);
  ASSERT_EQ(Verdict::MergeCopyWarningVerdicts(Allow(), Warn()).level(),
            Rule::Level::kWarn);
  ASSERT_EQ(Verdict::MergeCopyWarningVerdicts(NotSet(), Warn()).level(),
            Rule::Level::kWarn);
}

TEST(DataControlVerdictTest, TriggeredRules) {
  ASSERT_TRUE(NotSet().triggered_rules().empty());
  ASSERT_TRUE(Allow().triggered_rules().empty());

  auto report = Report();
  EXPECT_EQ(report.triggered_rules().size(), 1u);
  EXPECT_TRUE(report.triggered_rules().count(kReportRuleIndex));
  EXPECT_EQ(report.triggered_rules().at(kReportRuleIndex).rule_id,
            kReportRuleID);
  EXPECT_EQ(report.triggered_rules().at(kReportRuleIndex).rule_name,
            kReportRuleName);

  auto warn = Warn();
  EXPECT_EQ(warn.triggered_rules().size(), 1u);
  EXPECT_TRUE(warn.triggered_rules().count(kWarnRuleIndex));
  EXPECT_EQ(warn.triggered_rules().at(kWarnRuleIndex).rule_id, kWarnRuleID);
  EXPECT_EQ(warn.triggered_rules().at(kWarnRuleIndex).rule_name, kWarnRuleName);

  auto block = Block();
  EXPECT_EQ(block.triggered_rules().size(), 1u);
  EXPECT_TRUE(block.triggered_rules().count(kBlockRuleIndex));
  EXPECT_EQ(block.triggered_rules().at(kBlockRuleIndex).rule_id, kBlockRuleID);
  EXPECT_EQ(block.triggered_rules().at(kBlockRuleIndex).rule_name,
            kBlockRuleName);
}

TEST(DataControlVerdictTest, MergedPasteVerdictsTriggeredRules) {
  // Two verdicts with the same triggered rule merge correctly and don't
  // internally duplicate the rule in two.
  auto merged_warnings = Verdict::MergePasteVerdicts(Warn(), Warn());
  EXPECT_EQ(merged_warnings.triggered_rules().size(), 1u);
  EXPECT_TRUE(merged_warnings.triggered_rules().count(kWarnRuleIndex));
  EXPECT_EQ(merged_warnings.triggered_rules().at(kWarnRuleIndex).rule_id,
            kWarnRuleID);
  EXPECT_EQ(merged_warnings.triggered_rules().at(kWarnRuleIndex).rule_name,
            kWarnRuleName);

  // Merging three verdicts with different rules should rules in a verdict with
  // only the destination triggered rules.
  auto all_merged = Verdict::MergePasteVerdicts(
      Warn(), Verdict::MergePasteVerdicts(Report(), Block()));
  EXPECT_EQ(all_merged.triggered_rules().size(), 1u);
  EXPECT_TRUE(all_merged.triggered_rules().count(kBlockRuleIndex));
  EXPECT_EQ(all_merged.triggered_rules().at(kBlockRuleIndex).rule_id,
            kBlockRuleID);
  EXPECT_EQ(all_merged.triggered_rules().at(kBlockRuleIndex).rule_name,
            kBlockRuleName);
}

TEST(DataControlVerdictTest, MergedCopyWarningVerdictsTriggeredRules) {
  // Two verdicts with the same triggered rule merge correctly and don't
  // internally duplicate the rule in two.
  auto merged_warnings = Verdict::MergePasteVerdicts(Warn(), Warn());
  EXPECT_EQ(merged_warnings.triggered_rules().size(), 1u);
  EXPECT_TRUE(merged_warnings.triggered_rules().count(kWarnRuleIndex));
  EXPECT_EQ(merged_warnings.triggered_rules().at(kWarnRuleIndex).rule_id,
            kWarnRuleID);
  EXPECT_EQ(merged_warnings.triggered_rules().at(kWarnRuleIndex).rule_name,
            kWarnRuleName);

  // Triggered rules in the OS clipboard verdict are only kept if it's a
  // warning.
  auto report_and_warn = Verdict::MergeCopyWarningVerdicts(Report(), Warn());
  EXPECT_EQ(report_and_warn.triggered_rules().size(), 2u);
  EXPECT_TRUE(report_and_warn.triggered_rules().count(kReportRuleIndex));
  EXPECT_TRUE(report_and_warn.triggered_rules().count(kWarnRuleIndex));
  EXPECT_EQ(report_and_warn.triggered_rules().at(kReportRuleIndex).rule_id,
            kReportRuleID);
  EXPECT_EQ(report_and_warn.triggered_rules().at(kReportRuleIndex).rule_name,
            kReportRuleName);
  EXPECT_EQ(report_and_warn.triggered_rules().at(kWarnRuleIndex).rule_id,
            kWarnRuleID);
  EXPECT_EQ(report_and_warn.triggered_rules().at(kWarnRuleIndex).rule_name,
            kWarnRuleName);

  auto warn_and_block = Verdict::MergeCopyWarningVerdicts(Warn(), Block());
  EXPECT_EQ(warn_and_block.triggered_rules().size(), 1u);
  EXPECT_TRUE(warn_and_block.triggered_rules().count(kWarnRuleIndex));
  EXPECT_EQ(warn_and_block.triggered_rules().at(kWarnRuleIndex).rule_id,
            kWarnRuleID);
  EXPECT_EQ(warn_and_block.triggered_rules().at(kWarnRuleIndex).rule_name,
            kWarnRuleName);

  auto warn_and_report = Verdict::MergeCopyWarningVerdicts(Warn(), Report());
  EXPECT_EQ(warn_and_report.triggered_rules().size(), 1u);
  EXPECT_TRUE(warn_and_report.triggered_rules().count(kWarnRuleIndex));
  EXPECT_EQ(warn_and_report.triggered_rules().at(kWarnRuleIndex).rule_id,
            kWarnRuleID);
  EXPECT_EQ(warn_and_report.triggered_rules().at(kWarnRuleIndex).rule_name,
            kWarnRuleName);
}

}  // namespace data_controls
