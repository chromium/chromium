// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/verdict.h"

#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_controls {

TEST(DataControlVerdictTest, Level) {
  ASSERT_EQ(Verdict::NotSet().level(), Rule::Level::kNotSet);
  ASSERT_EQ(Verdict::Report(base::DoNothing()).level(), Rule::Level::kReport);
  ASSERT_EQ(Verdict::Warn(base::DoNothing(), base::DoNothing()).level(),
            Rule::Level::kWarn);
  ASSERT_EQ(Verdict::Block(base::DoNothing()).level(), Rule::Level::kBlock);
  ASSERT_EQ(Verdict::Allow().level(), Rule::Level::kAllow);
}

TEST(DataControlVerdictTest, InitialReport) {
  ASSERT_TRUE(Verdict::NotSet().TakeInitialReportClosure().is_null());
  ASSERT_TRUE(Verdict::Allow().TakeInitialReportClosure().is_null());

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
  ASSERT_TRUE(Verdict::NotSet().TakeBypassReportClosure().is_null());
  ASSERT_TRUE(
      Verdict::Block(base::DoNothing()).TakeBypassReportClosure().is_null());
  ASSERT_TRUE(Verdict::Allow().TakeBypassReportClosure().is_null());
  ASSERT_TRUE(
      Verdict::Report(base::DoNothing()).TakeBypassReportClosure().is_null());

  base::test::TestFuture<void> warn_future;
  auto warn = Verdict::Warn(base::DoNothing(), warn_future.GetCallback());
  auto warn_callback = warn.TakeBypassReportClosure();
  ASSERT_FALSE(warn_callback.is_null());
  std::move(warn_callback).Run();
  ASSERT_TRUE(warn_future.Wait());
}

}  // namespace data_controls
