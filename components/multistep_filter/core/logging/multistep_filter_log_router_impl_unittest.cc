// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/multistep_filter_log_router_impl.h"

#include "base/barrier_closure.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {
namespace {

constexpr int64_t kTestNavigationId1 = 1;
constexpr int64_t kTestNavigationId2 = 2;
constexpr int64_t kTestNavigationId5 = 5;

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

  LogEntry entry(kTestNavigationId1, LogEventType::kNavigationStarted,
                 "example.com");

  EXPECT_CALL(observer,
              OnLogEntryAdded(testing::Field(&LogEntry::navigation_id,
                                             testing::Eq(kTestNavigationId1))));
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
    LogEntry entry(i, LogEventType::kNavigationStarted, "example.com");
    router_.RouteLogMessage(std::move(entry));
  }

  std::vector<LogEntry> logs = router_.GetBufferedLogs();
  ASSERT_EQ(MultistepFilterLogRouterImpl::kMaxBufferSize, logs.size());

  // First 100 entries should have been evicted. First buffered log should be
  // 100.
  EXPECT_EQ(logs[0].navigation_id, 100);
  EXPECT_EQ(
      logs[MultistepFilterLogRouterImpl::kMaxBufferSize - 1].navigation_id,
      kTotalLogs - 1);
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

  LogEntry entry(kTestNavigationId1, LogEventType::kNavigationStarted,
                 "example.com");

  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      base::BindOnce([](base::RepeatingCallback<void(LogEntry)> cb,
                        LogEntry entry) { cb.Run(std::move(entry)); },
                     callback, std::move(entry)),
      run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_TRUE(router_.GetBufferedLogs().empty());
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

TEST_F(MultistepFilterLogRouterImplTest, LogsIgnoredWhenLoggingDisabled) {
  EXPECT_FALSE(router_.IsLoggingEnabled());
  LogEntry entry(kTestNavigationId2, LogEventType::kNavigationStarted,
                 "example.com");
  router_.RouteLogMessage(std::move(entry));
  EXPECT_TRUE(router_.GetBufferedLogs().empty());
}

TEST_F(MultistepFilterLogRouterImplTest, RemoveObserverDisablesLogging) {
  MockLogRouterObserver observer1;
  MockLogRouterObserver observer2;

  router_.AddObserver(&observer1);
  EXPECT_TRUE(router_.IsLoggingEnabled());

  router_.AddObserver(&observer2);
  EXPECT_TRUE(router_.IsLoggingEnabled());

  router_.RemoveObserver(&observer1);
  EXPECT_TRUE(router_.IsLoggingEnabled());

  router_.RemoveObserver(&observer2);
  EXPECT_FALSE(router_.IsLoggingEnabled());
}

TEST_F(MultistepFilterLogRouterImplTest, ShutdownClearsBuffer) {
  MockLogRouterObserver observer;
  router_.AddObserver(&observer);

  EXPECT_CALL(observer, OnLogEntryAdded(testing::_));
  router_.RouteLogMessage(LogEntry(
      kTestNavigationId2, LogEventType::kNavigationStarted, "test.com"));
  EXPECT_EQ(1u, router_.GetBufferedLogs().size());

  EXPECT_CALL(observer, OnLogRouterShutdown());
  router_.Shutdown();
  EXPECT_TRUE(router_.GetBufferedLogs().empty());
  router_.RemoveObserver(&observer);
}

class RemovingObserver : public MultistepFilterLogRouter::Observer {
 public:
  explicit RemovingObserver(MultistepFilterLogRouter* router)
      : router_(router) {}
  void OnLogEntryAdded(const LogEntry& entry) override {}
  void OnLogRouterShutdown() override { router_->RemoveObserver(this); }

 private:
  raw_ptr<MultistepFilterLogRouter> router_;
};

TEST_F(MultistepFilterLogRouterImplTest, ObserverRemovesItselfOnShutdown) {
  RemovingObserver observer(&router_);
  router_.AddObserver(&observer);
  // This should not crash or trigger use-after-free or iterator invalidation.
  router_.Shutdown();
}

TEST_F(MultistepFilterLogRouterImplTest, LogsFromBackgroundThread) {
  MockLogRouterObserver observer;
  base::ScopedObservation<MultistepFilterLogRouter,
                          MultistepFilterLogRouter::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(&router_);

  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              OnLogEntryAdded(testing::Field(&LogEntry::navigation_id,
                                             testing::Eq(kTestNavigationId5))))
      .WillOnce([&](const LogEntry&) { run_loop.Quit(); });

  base::RepeatingCallback<void(LogEntry)> callback = router_.GetLogCallback();
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::RepeatingCallback<void(LogEntry)> cb) {
                       LogEntry entry(kTestNavigationId5,
                                      LogEventType::kNavigationStarted,
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
                         LogEntry entry(id, LogEventType::kNavigationStarted,
                                        "concurrent.example.com");
                         cb.Run(std::move(entry));
                       },
                       callback, i));
  }

  // Wait for all ThreadPool tasks and their main-thread responses to finish.
  run_loop.Run();

  std::vector<LogEntry> logs = router_.GetBufferedLogs();
  EXPECT_EQ(static_cast<size_t>(kNumConcurrentLogs), logs.size());
}

}  // namespace
}  // namespace multistep_filter
