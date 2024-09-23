// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/browsing_data_counter.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {

namespace {

static const char* kTestingDatatypePref = "counter.testing.datatype";

class MockBrowsingDataCounter : public BrowsingDataCounter {
 public:
  MockBrowsingDataCounter() = default;
  ~MockBrowsingDataCounter() override = default;

  // There are two overloaded ReportResult methods. We need to disambiguate
  // between them to be able to bind one of them in base::BindOnce().
  using ReportResultType =
      void(MockBrowsingDataCounter::*)(BrowsingDataCounter::ResultInt);

  // BrowsingDataCounter implementation:
  const char* GetPrefName() const override {
    return kTestingDatatypePref;
  }

  void Count() override {
    if (delay_ms_ < 0)
      return;

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(static_cast<ReportResultType>(
                           &MockBrowsingDataCounter::ReportResult),
                       base::Unretained(this),
                       static_cast<BrowsingDataCounter::ResultInt>(0)),
        base::Milliseconds(delay_ms_));
  }

  void OnInitialized() override {}

  void DoReportResult(std::unique_ptr<BrowsingDataCounter::Result> result)
      override {
    BrowsingDataCounter::DoReportResult(std::move(result));
    run_loop_->Quit();
  }

  // Blocks the UI thread until the counter has finished.
  void WaitForResult() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void set_counting_delay(int delay_ms) { delay_ms_ = delay_ms; }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  int delay_ms_ = 0;
};

}  // namespace

class BrowsingDataCounterTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterBooleanPref(kTestingDatatypePref, true);
    pref_service_->registry()->RegisterIntegerPref(prefs::kDeleteTimePeriod, 0);

    counter_ = std::make_unique<MockBrowsingDataCounter>();
    counter_->Init(pref_service_.get(),
                   browsing_data::ClearBrowsingDataTab::ADVANCED,
                   base::DoNothing());

    counter_no_period_ = std::make_unique<MockBrowsingDataCounter>();
    counter_no_period_->InitWithoutPeriodPref(
        pref_service_.get(), browsing_data::ClearBrowsingDataTab::ADVANCED,
        base::Time::Min(), base::DoNothing());
  }

  void TearDown() override {
    counter_.reset();
    counter_no_period_.reset();
    pref_service_.reset();
    testing::Test::TearDown();
  }

 protected:
  void UpdatePeriodPref() {
    int current = pref_service_->GetInteger(prefs::kDeleteTimePeriod);
    pref_service_->SetInteger(prefs::kDeleteTimePeriod, current ? 0 : 1);
  }

  MockBrowsingDataCounter* counter() { return counter_.get(); }

  MockBrowsingDataCounter* counter_no_period() {
    return counter_no_period_.get();
  }

 private:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<MockBrowsingDataCounter> counter_;
  std::unique_ptr<MockBrowsingDataCounter> counter_no_period_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(BrowsingDataCounterTest, NoResponse) {
  counter()->set_counting_delay(-1 /* never */);
  counter()->Restart();

  const std::vector<BrowsingDataCounter::State>& state_transitions =
      counter()->GetStateTransitionsForTesting();
  std::vector<BrowsingDataCounter::State> expected = {
      BrowsingDataCounter::State::RESTARTED};
  EXPECT_EQ(expected, state_transitions);
}

TEST_F(BrowsingDataCounterTest, ImmediateResponse) {
  counter()->set_counting_delay(0 /* ms */);
  counter()->Restart();
  counter()->WaitForResult();

  const std::vector<BrowsingDataCounter::State>& state_transitions =
      counter()->GetStateTransitionsForTesting();
  std::vector<BrowsingDataCounter::State> expected = {
      BrowsingDataCounter::State::RESTARTED, BrowsingDataCounter::State::IDLE};
  EXPECT_EQ(expected, state_transitions);
}

TEST_F(BrowsingDataCounterTest, ResponseWhileCalculatingIsShown) {
  counter()->set_counting_delay(500 /* ms */);
  counter()->Restart();
  counter()->WaitForResult();

  const std::vector<BrowsingDataCounter::State>& state_transitions =
      counter()->GetStateTransitionsForTesting();
  std::vector<BrowsingDataCounter::State> expected = {
      BrowsingDataCounter::State::RESTARTED,
      BrowsingDataCounter::State::SHOW_CALCULATING,
      BrowsingDataCounter::State::REPORT_STAGED_RESULT,
      BrowsingDataCounter::State::IDLE};
  EXPECT_EQ(expected, state_transitions);
}

TEST_F(BrowsingDataCounterTest, LateResponse) {
  counter()->set_counting_delay(2000 /* ms */);
  counter()->Restart();
  counter()->WaitForResult();

  const std::vector<BrowsingDataCounter::State>& state_transitions =
      counter()->GetStateTransitionsForTesting();
  std::vector<BrowsingDataCounter::State> expected = {
      BrowsingDataCounter::State::RESTARTED,
      BrowsingDataCounter::State::SHOW_CALCULATING,
      BrowsingDataCounter::State::READY_TO_REPORT_RESULT,
      BrowsingDataCounter::State::IDLE};
  EXPECT_EQ(expected, state_transitions);
}

TEST_F(BrowsingDataCounterTest, MultipleRuns) {
  counter()->set_counting_delay(0 /* ms */);
  counter()->Restart();
  counter()->WaitForResult();

  counter()->set_counting_delay(500 /* ms */);
  counter()->Restart();
  counter()->WaitForResult();

  counter()->set_counting_delay(1500 /* ms */);
  counter()->Restart();
  counter()->WaitForResult();

  const std::vector<BrowsingDataCounter::State>& state_transitions =
      counter()->GetStateTransitionsForTesting();
  std::vector<BrowsingDataCounter::State> expected = {
      BrowsingDataCounter::State::RESTARTED,
      BrowsingDataCounter::State::SHOW_CALCULATING,
      BrowsingDataCounter::State::READY_TO_REPORT_RESULT,
      BrowsingDataCounter::State::IDLE};
  EXPECT_EQ(expected, state_transitions);
}

TEST_F(BrowsingDataCounterTest, RestartingDoesntBreak) {
  counter()->set_counting_delay(10 /* ms */);
  counter()->Restart();
  counter()->Restart();
  counter()->Restart();
  counter()->WaitForResult();

  const std::vector<BrowsingDataCounter::State>& state_transitions =
      counter()->GetStateTransitionsForTesting();
  std::vector<BrowsingDataCounter::State> expected = {
      BrowsingDataCounter::State::RESTARTED, BrowsingDataCounter::State::IDLE};
  EXPECT_EQ(expected, state_transitions);
}

TEST_F(BrowsingDataCounterTest, InitWithoutPeriodPref) {
  const std::vector<BrowsingDataCounter::State>& state_transitions =
      counter()->GetStateTransitionsForTesting();
  const std::vector<BrowsingDataCounter::State>& state_transitions_no_period =
      counter_no_period()->GetStateTransitionsForTesting();

  // Changing the time period pref restarts the counter initialized with a time
  // period, but not one initialized without a time period.
  UpdatePeriodPref();

  counter()->WaitForResult();
  std::vector<BrowsingDataCounter::State> expected = {
      BrowsingDataCounter::State::RESTARTED, BrowsingDataCounter::State::IDLE};
  EXPECT_EQ(expected, state_transitions);

  EXPECT_TRUE(state_transitions_no_period.empty());

  // Instead, a counter with no time period pref restarts when |SetBeginTime|
  // is called to update the period.
  counter_no_period()->SetBeginTime(base::Time::Now());
  counter_no_period()->WaitForResult();

  expected = {BrowsingDataCounter::State::RESTARTED,
              BrowsingDataCounter::State::IDLE};
  EXPECT_EQ(expected, state_transitions_no_period);
}

}  // namespace browsing_data
