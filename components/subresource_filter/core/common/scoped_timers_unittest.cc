// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/scoped_timers.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockExportFunctor {
 public:
  int number_of_calls() const { return number_of_calls_; }
  void operator()(base::TimeDelta) { ++number_of_calls_; }

 private:
  int number_of_calls_ = 0;
};

template <typename TimerFactory>
void ExpectFunctorIsCalledOnceOnDestruction() {
  MockExportFunctor export_functor;
  {
    auto scoped_timer = TimerFactory::Start(export_functor);
    EXPECT_EQ(0, export_functor.number_of_calls());
  }
  const int expected_number_of_calls = TimerFactory::IsSupported() ? 1 : 0;
  EXPECT_EQ(expected_number_of_calls, export_functor.number_of_calls());
}

template <typename TimerFactory>
void ExpectStoredLambdaIsInvokedOnceOnDestruction() {
  bool export_is_called = false;
  auto export_functor = [&export_is_called](base::TimeDelta) {
    EXPECT_FALSE(export_is_called);
    export_is_called = true;
  };

  {
    auto scoped_timer = TimerFactory::Start(export_functor);
    EXPECT_FALSE(export_is_called);
  }
  EXPECT_EQ(TimerFactory::IsSupported(), export_is_called);
}

template <typename TimerFactory>
void ExpectInlineLambdaIsInvokedOnceOnDestruction() {
  bool export_is_called = false;
  {
    auto scoped_timer =
        TimerFactory::Start([&export_is_called](base::TimeDelta) {
          EXPECT_FALSE(export_is_called);
          export_is_called = true;
        });
    EXPECT_FALSE(export_is_called);
  }
  EXPECT_EQ(TimerFactory::IsSupported(), export_is_called);
}

template <typename TimerFactory>
void ExpectWellBehavedStartIf(bool condition) {
  bool export_is_called = false;
  auto export_functor = [&export_is_called](base::TimeDelta) {
    EXPECT_FALSE(export_is_called);
    export_is_called = true;
  };

  {
    auto scoped_timer = TimerFactory::StartIf(condition, export_functor);
    EXPECT_FALSE(export_is_called);
  }
  EXPECT_EQ(condition && TimerFactory::IsSupported(), export_is_called);
}

template <typename TimerFactory>
void ExpectWellBehavedMoveContructor() {
  MockExportFunctor export_functor;
  const int expected_number_of_calls = TimerFactory::IsSupported() ? 1 : 0;
  {
    auto scoped_timer = TimerFactory::Start(export_functor);
    EXPECT_EQ(0, export_functor.number_of_calls());
    {
      auto another_scoped_timer = std::move(scoped_timer);
      EXPECT_EQ(0, export_functor.number_of_calls());
    }
    // |another_scoped_timer| should have called |export_functor|.
    EXPECT_EQ(expected_number_of_calls, export_functor.number_of_calls());
  }
  // But |scoped_timer| should have not since then.
  EXPECT_EQ(expected_number_of_calls, export_functor.number_of_calls());
}

template <typename TimerFactory>
void ExpectWellBehavedMoveAssignment() {
  MockExportFunctor export_functor;
  const int expected_number_of_calls = TimerFactory::IsSupported() ? 1 : 0;
  {
    auto scoped_timer = TimerFactory::Start(export_functor);
    EXPECT_EQ(0, export_functor.number_of_calls());
    {
      auto another_scoped_timer = std::move(scoped_timer);
      scoped_timer = std::move(another_scoped_timer);
      EXPECT_EQ(0, export_functor.number_of_calls());
    }
    // |another_scoped_timer| shouldn't have called |export_functor|.
    EXPECT_EQ(0, export_functor.number_of_calls());
  }
  // But |scoped_timer| should have because it owns the measurement.
  EXPECT_EQ(expected_number_of_calls, export_functor.number_of_calls());
}

}  // namespace

namespace subresource_filter {

TEST(ScopedTimersTest, CallsFunctor) {
  ExpectFunctorIsCalledOnceOnDestruction<ScopedTimers>();
  ExpectFunctorIsCalledOnceOnDestruction<ScopedThreadTimers>();
}

TEST(ScopedTimersTest, CallsStoredLambdaFunctor) {
  ExpectStoredLambdaIsInvokedOnceOnDestruction<ScopedTimers>();
  ExpectStoredLambdaIsInvokedOnceOnDestruction<ScopedThreadTimers>();
}

TEST(ScopedTimersTest, CallsInlineLambdaFunctor) {
  ExpectInlineLambdaIsInvokedOnceOnDestruction<ScopedTimers>();
  ExpectInlineLambdaIsInvokedOnceOnDestruction<ScopedThreadTimers>();
}

TEST(ScopedTimersTest, StartIf) {
  ExpectWellBehavedStartIf<ScopedTimers>(false);
  ExpectWellBehavedStartIf<ScopedTimers>(true);

  ExpectWellBehavedStartIf<ScopedThreadTimers>(false);
  ExpectWellBehavedStartIf<ScopedThreadTimers>(true);
}

TEST(ScopedTimersTest, MoveConstructTimer) {
  ExpectWellBehavedMoveContructor<ScopedTimers>();
  ExpectWellBehavedMoveContructor<ScopedThreadTimers>();
}

TEST(ScopedTimersTest, MoveAssignTimer) {
  ExpectWellBehavedMoveAssignment<ScopedTimers>();
  ExpectWellBehavedMoveAssignment<ScopedThreadTimers>();
}

// Below are tests for macros in "time_measurements.h" -------------------------
// TODO(pkalinnikov): Move these to a separate file?

TEST(ScopedTimersTest, ScopedUmaHistogramMacros) {
  base::HistogramTester tester;
  {
    SCOPED_UMA_HISTOGRAM_THREAD_TIMER("ScopedTimers.ThreadTimer");
    SCOPED_UMA_HISTOGRAM_MICRO_THREAD_TIMER("ScopedTimers.MicroThreadTimer");
    SCOPED_UMA_HISTOGRAM_MICRO_TIMER("ScopedTimers.MicroTimer");

    tester.ExpectTotalCount("ScopedTimers.ThreadTimer", 0);
    tester.ExpectTotalCount("ScopedTimers.MicroThreadTimer", 0);
    tester.ExpectTotalCount("ScopedTimers.MicroTimer", 0);
  }

  int expected_count = ScopedTimers::IsSupported() ? 1 : 0;
  tester.ExpectTotalCount("ScopedTimers.MicroTimer", expected_count);

  expected_count = ScopedThreadTimers::IsSupported() ? 1 : 0;
  tester.ExpectTotalCount("ScopedTimers.ThreadTimer", expected_count);
  tester.ExpectTotalCount("ScopedTimers.MicroThreadTimer", expected_count);
}

TEST(ScopedTimersTest, UmaHistogramMicroTimesFromExportFunctor) {
  base::HistogramTester tester;
  auto export_functor = [](base::TimeDelta delta) {
    UMA_HISTOGRAM_MICRO_TIMES("ScopedTimers.MicroTimes", delta);
  };
  {
    auto scoped_timer = ScopedTimers::Start(export_functor);
    tester.ExpectTotalCount("ScopedTimers.MicroTimes", 0);
  }
  tester.ExpectTotalCount("ScopedTimers.MicroTimes", 1);
}

}  // namespace subresource_filter
