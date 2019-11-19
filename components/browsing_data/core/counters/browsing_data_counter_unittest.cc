// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/browsing_data_counter.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {

namespace {

static const char* kTestingDatatypePref = "counter.testing.datatype";

void IgnoreResult(std::unique_ptr<BrowsingDataCounter::Result> result) {
}

class MockBrowsingDataCounter : public BrowsingDataCounter {
 public:
  MockBrowsingDataCounter() {}
  ~MockBrowsingDataCounter() override {}

  // There are two overloaded ReportResult methods. We need to disambiguate
  // between them to be able to bind one of them in base::Bind().
  using ReportResultType =
      void(MockBrowsingDataCounter::*)(BrowsingDataCounter::ResultInt);

  // BrowsingDataCounter implementation:
  const char* GetPrefName() const override {
    return kTestingDatatypePref;
  }

  void Count() override {
    if (delay_ms_ < 0)
      return;

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(static_cast<ReportResultType>(
                           &MockBrowsingDataCounter::ReportResult),
                       base::Unretained(this),
                       static_cast<BrowsingDataCounter::ResultInt>(0)),
        base::TimeDelta::FromMilliseconds(delay_ms_));
  }

  void OnInitialized() override {}

  void DoReportResult(std::unique_ptr<BrowsingDataCounter::Result> result)
      override {
    BrowsingDataCounter::DoReportResult(std::move(result));
    run_loop_->Quit();
  }

  // Blocks the UI thread until the counter has finished.
  void WaitForResult() {
    run_loop_.reset(new base::RunLoop());
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
    pref_service_.reset(new TestingPrefServiceSimple());
    pref_service_->registry()->RegisterBooleanPref(kTestingDatatypePref, true);
    pref_service_->registry()->RegisterIntegerPref(prefs::kDeleteTimePeriod, 0);

    counter_.reset(new MockBrowsingDataCounter());
    counter_->Init(pref_service_.get(),
                   browsing_data::ClearBrowsingDataTab::ADVANCED,
                   base::Bind(&IgnoreResult));
  }

  void TearDown() override {
    counter_.reset();
    pref_service_.reset();
    testing::Test::TearDown();
  }

  MockBrowsingDataCounter* counter() { return counter_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<MockBrowsingDataCounter> counter_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(BrowsingDataCounterTest, NoResponse) {
  counter()->set_counting_delay(-1 /* never */);
  counter()->Restart();

  const std::vector<BrowsingDataCounter::State>& state_transitions =
      counter()->GetStateTransitionsForTesting();
  DCHECK_EQ(1u, state_transitions.size());
  DCHECK_EQ(BrowsingDataCounter::State::RESTARTED, state_transitions[0]);
}

TEST_F(BrowsingDataCounterTest, ImmediateResponse) {
  counter()->set_counting_delay(0 /* ms */);
  counter()->Restart();
  counter()->WaitForResult();

  const std::vector<BrowsingDataCounter::State>& state_transitions =
      counter()->GetStateTransitionsForTesting();
  DCHECK_EQ(2u, state_transitions.size());
  DCHECK_EQ(BrowsingDataCounter::State::RESTARTED, state_transitions[0]);
  DCHECK_EQ(BrowsingDataCounter::State::IDLE, state_transitions[1]);
}

TEST_F(BrowsingDataCounterTest, ResponseWhileCalculatingIsShown) {
  counter()->set_counting_delay(500 /* ms */);
  counter()->Restart();
  counter()->WaitForResult();

  const std::vector<BrowsingDataCounter::State>& state_transitions =
      counter()->GetStateTransitionsForTesting();
  DCHECK_EQ(4u, state_transitions.size());
  DCHECK_EQ(BrowsingDataCounter::State::RESTARTED, state_transitions[0]);
  DCHECK_EQ(BrowsingDataCounter::State::SHOW_CALCULATING, state_transitions[1]);
  DCHECK_EQ(BrowsingDataCounter::State::REPORT_STAGED_RESULT,
            state_transitions[2]);
  DCHECK_EQ(BrowsingDataCounter::State::IDLE, state_transitions[3]);
}

TEST_F(BrowsingDataCounterTest, LateResponse) {
  counter()->set_counting_delay(2000 /* ms */);
  counter()->Restart();
  counter()->WaitForResult();

  const std::vector<BrowsingDataCounter::State>& state_transitions =
      counter()->GetStateTransitionsForTesting();
  DCHECK_EQ(4u, state_transitions.size());
  DCHECK_EQ(BrowsingDataCounter::State::RESTARTED, state_transitions[0]);
  DCHECK_EQ(BrowsingDataCounter::State::SHOW_CALCULATING, state_transitions[1]);
  DCHECK_EQ(BrowsingDataCounter::State::READY_TO_REPORT_RESULT,
            state_transitions[2]);
  DCHECK_EQ(BrowsingDataCounter::State::IDLE, state_transitions[3]);
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
  DCHECK_EQ(4u, state_transitions.size());
  DCHECK_EQ(BrowsingDataCounter::State::RESTARTED, state_transitions[0]);
  DCHECK_EQ(BrowsingDataCounter::State::SHOW_CALCULATING, state_transitions[1]);
  DCHECK_EQ(BrowsingDataCounter::State::READY_TO_REPORT_RESULT,
            state_transitions[2]);
  DCHECK_EQ(BrowsingDataCounter::State::IDLE, state_transitions[3]);
}

TEST_F(BrowsingDataCounterTest, RestartingDoesntBreak) {
  counter()->set_counting_delay(10 /* ms */);
  counter()->Restart();
  counter()->Restart();
  counter()->Restart();
  counter()->WaitForResult();

  const std::vector<BrowsingDataCounter::State>& state_transitions =
      counter()->GetStateTransitionsForTesting();
  DCHECK_EQ(2u, state_transitions.size());
  DCHECK_EQ(BrowsingDataCounter::State::RESTARTED, state_transitions[0]);
  DCHECK_EQ(BrowsingDataCounter::State::IDLE, state_transitions[1]);
}

}  // namespace browsing_data
