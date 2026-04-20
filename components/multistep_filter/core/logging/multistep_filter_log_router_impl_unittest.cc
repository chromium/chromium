// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/multistep_filter_log_router_impl.h"

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

class MockLogRouterObserver : public MultistepFilterLogRouter::Observer {
 public:
  MOCK_METHOD(void, OnLogEntryAdded, (const LogEntry& entry), (override));
  MOCK_METHOD(void, OnLogRouterShutdown, (), (override));
};

class MultistepFilterLogRouterImplTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  MultistepFilterLogRouterImpl router_;
};

TEST_F(MultistepFilterLogRouterImplTest, LoggingEnabledBasedOnObservers) {
  EXPECT_FALSE(router_.IsLoggingEnabled());

  MockLogRouterObserver observer;
  base::ScopedObservation<MultistepFilterLogRouter,
                          MultistepFilterLogRouter::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(&router_);
  EXPECT_TRUE(router_.IsLoggingEnabled());

  scoped_observation.Reset();
  EXPECT_FALSE(router_.IsLoggingEnabled());
}

TEST_F(MultistepFilterLogRouterImplTest, ObserversNotifiedOnLogAdded) {
  MockLogRouterObserver observer;
  base::ScopedObservation<MultistepFilterLogRouter,
                          MultistepFilterLogRouter::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(&router_);

  LogEntry entry("test-id", LogEventType::kNavigationStarted, "example.com");

  EXPECT_CALL(observer, OnLogEntryAdded(testing::Field(
                            &LogEntry::navigation_id, testing::Eq("test-id"))));
  router_.RouteLogMessage(std::move(entry));
}

TEST_F(MultistepFilterLogRouterImplTest, BufferManagement) {
  MockLogRouterObserver observer;
  base::ScopedObservation<MultistepFilterLogRouter,
                          MultistepFilterLogRouter::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(&router_);

  // Add more logs than the limit
  const int kTotalLogs = MultistepFilterLogRouterImpl::kMaxBufferSize + 100;
  EXPECT_CALL(observer, OnLogEntryAdded(testing::_)).Times(kTotalLogs);

  for (int i = 0; i < kTotalLogs; ++i) {
    LogEntry entry(base::NumberToString(i), LogEventType::kNavigationStarted,
                   "example.com");
    router_.RouteLogMessage(std::move(entry));
  }

  base::ListValue logs = router_.GetBufferedLogs();
  ASSERT_EQ(logs.size(), MultistepFilterLogRouterImpl::kMaxBufferSize);

  // First 100 entries should have been evicted. First buffered log should be
  // 100.
  const std::string* first_nav_id =
      logs[0].GetDict().FindString("navigation_id");
  ASSERT_TRUE(first_nav_id);
  EXPECT_EQ(*first_nav_id, "100");

  const std::string* last_nav_id =
      logs[MultistepFilterLogRouterImpl::kMaxBufferSize - 1]
          .GetDict()
          .FindString("navigation_id");
  ASSERT_TRUE(last_nav_id);
  EXPECT_EQ(*last_nav_id, base::NumberToString(kTotalLogs - 1));
}

TEST_F(MultistepFilterLogRouterImplTest, CallbackSafeAfterShutdown) {
  MockLogRouterObserver observer;
  base::ScopedObservation<MultistepFilterLogRouter,
                          MultistepFilterLogRouter::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(&router_);

  base::RepeatingCallback<void(LogEntry)> callback = router_.GetLogCallback();

  EXPECT_CALL(observer, OnLogRouterShutdown());
  router_.Shutdown();

  EXPECT_CALL(observer, OnLogEntryAdded(testing::_)).Times(0);

  LogEntry entry("post-shutdown", LogEventType::kNavigationStarted,
                 "example.com");

  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      base::BindOnce([](base::RepeatingCallback<void(LogEntry)> cb,
                        LogEntry entry) { cb.Run(std::move(entry)); },
                     callback, std::move(entry)),
      run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_EQ(router_.GetBufferedLogs().size(), 0u);
}

TEST_F(MultistepFilterLogRouterImplTest, ShutdownNotifiesObservers) {
  MockLogRouterObserver observer;
  base::ScopedObservation<MultistepFilterLogRouter,
                          MultistepFilterLogRouter::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(&router_);

  EXPECT_CALL(observer, OnLogRouterShutdown());
  router_.Shutdown();
}

TEST_F(MultistepFilterLogRouterImplTest, LogsFromBackgroundThread) {
  MockLogRouterObserver observer;
  base::ScopedObservation<MultistepFilterLogRouter,
                          MultistepFilterLogRouter::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(&router_);

  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLogEntryAdded(testing::Field(&LogEntry::navigation_id,
                                                       testing::Eq("bg-id"))))
      .WillOnce([&](const LogEntry&) { run_loop.Quit(); });

  base::RepeatingCallback<void(LogEntry)> callback = router_.GetLogCallback();
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::RepeatingCallback<void(LogEntry)> cb) {
                       LogEntry entry("bg-id", LogEventType::kNavigationStarted,
                                      "bg.example.com");
                       cb.Run(std::move(entry));
                     },
                     callback));

  run_loop.Run();
}

TEST_F(MultistepFilterLogRouterImplTest, ConcurrentLogsFromMultipleThreads) {
  MockLogRouterObserver observer;
  base::ScopedObservation<MultistepFilterLogRouter,
                          MultistepFilterLogRouter::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(&router_);

  const int kNumConcurrentLogs = 50;

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(kNumConcurrentLogs, run_loop.QuitClosure());

  EXPECT_CALL(observer, OnLogEntryAdded(testing::_))
      .Times(kNumConcurrentLogs)
      .WillRepeatedly(
          [barrier_closure](const LogEntry&) { barrier_closure.Run(); });

  base::RepeatingCallback<void(LogEntry)> callback = router_.GetLogCallback();
  for (int i = 0; i < kNumConcurrentLogs; ++i) {
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::RepeatingCallback<void(LogEntry)> cb, int id) {
                         LogEntry entry(base::NumberToString(id),
                                        LogEventType::kNavigationStarted,
                                        "concurrent.example.com");
                         cb.Run(std::move(entry));
                       },
                       callback, i));
  }

  // Wait for all ThreadPool tasks and their main-thread responses to finish.
  run_loop.Run();

  base::ListValue logs = router_.GetBufferedLogs();
  EXPECT_EQ(logs.size(), static_cast<size_t>(kNumConcurrentLogs));
}

}  // namespace multistep_filter
