// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/verdict.h"

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_controls {

namespace {

// Helpers to make the tests more concise.
Verdict NotSet() {
  return Verdict::NotSet();
}
Verdict Report() {
  return Verdict::Report(base::DoNothing());
}
Verdict Warn() {
  return Verdict::Warn(base::DoNothing(), base::DoNothing());
}
Verdict Block() {
  return Verdict::Block(base::DoNothing());
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

TEST(DataControlVerdictTest, InitialReport) {
  ASSERT_TRUE(NotSet().TakeInitialReportClosure().is_null());
  ASSERT_TRUE(Allow().TakeInitialReportClosure().is_null());

  base::test::TestFuture<void> report_future;
  auto report = Verdict::Report(report_future.GetCallback());
  auto report_callback = report.TakeInitialReportClosure();
  ASSERT_FALSE(report_callback.is_null());
  std::move(report_callback).Run();
  ASSERT_TRUE(report_future.Wait());

  base::test::TestFuture<void> warn_future;
  auto warn = Verdict::Warn(warn_future.GetCallback(), base::DoNothing());
  auto warn_callback = warn.TakeInitialReportClosure();
  ASSERT_FALSE(warn_callback.is_null());
  std::move(warn_callback).Run();
  ASSERT_TRUE(warn_future.Wait());

  base::test::TestFuture<void> block_future;
  auto block = Verdict::Block(block_future.GetCallback());
  auto block_callback = block.TakeInitialReportClosure();
  ASSERT_FALSE(block_callback.is_null());
  std::move(block_callback).Run();
  ASSERT_TRUE(block_future.Wait());
}

TEST(DataControlVerdictTest, BypassReport) {
  ASSERT_TRUE(NotSet().TakeBypassReportClosure().is_null());
  ASSERT_TRUE(Block().TakeBypassReportClosure().is_null());
  ASSERT_TRUE(Allow().TakeBypassReportClosure().is_null());
  ASSERT_TRUE(Report().TakeBypassReportClosure().is_null());

  base::test::TestFuture<void> warn_future;
  auto warn = Verdict::Warn(base::DoNothing(), warn_future.GetCallback());
  auto warn_callback = warn.TakeBypassReportClosure();
  ASSERT_FALSE(warn_callback.is_null());
  std::move(warn_callback).Run();
  ASSERT_TRUE(warn_future.Wait());
}

TEST(DataControlVerdictTest, MergedCallbacks) {
  base::test::TestFuture<void> source_initial_report_future;
  base::test::TestFuture<void> source_bypass_report_future;
  auto source_verdict =
      Verdict::Warn(source_initial_report_future.GetCallback(),
                    source_bypass_report_future.GetCallback());

  base::test::TestFuture<void> destination_initial_report_future;
  base::test::TestFuture<void> destination_bypass_report_future;
  auto destination_verdict =
      Verdict::Warn(destination_initial_report_future.GetCallback(),
                    destination_bypass_report_future.GetCallback());

  auto merged_verdict =
      Verdict::Merge(std::move(source_verdict), std::move(destination_verdict));
  auto merged_initial_report = merged_verdict.TakeInitialReportClosure();
  ASSERT_TRUE(merged_initial_report);
  std::move(merged_initial_report).Run();
  ASSERT_TRUE(source_initial_report_future.Wait());
  ASSERT_TRUE(destination_initial_report_future.Wait());

  auto merged_bypass_report = merged_verdict.TakeBypassReportClosure();
  ASSERT_TRUE(merged_bypass_report);
  std::move(merged_bypass_report).Run();
  ASSERT_TRUE(source_bypass_report_future.Wait());
  ASSERT_TRUE(destination_bypass_report_future.Wait());
}

}  // namespace data_controls
