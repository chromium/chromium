// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/logger_impl.h"

#include "base/command_line.h"
#include "base/time/time.h"
#include "components/data_sharing/public/logger_common.mojom.h"
#include "components/data_sharing/public/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace data_sharing {
namespace {

class MockObserver : public Logger::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void, OnNewLog, (const Logger::Entry& entry), (override));
};

class DataSharingLoggerImplTest : public testing::Test {
 public:
  DataSharingLoggerImplTest() = default;
  ~DataSharingLoggerImplTest() override = default;

  void TearDown() override {
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        data_sharing::kDataSharingDebugLoggingEnabled);
  }
};

TEST_F(DataSharingLoggerImplTest, DisabledWithoutObserver) {
  LoggerImpl logger;
  MockObserver observer;

  Logger::Entry entry1(base::Time::Now(),
                       logger_common::mojom::LogSource::Unknown, "fake_file.cc",
                       123, "fake message");
  Logger::Entry entry2(base::Time::Now(),
                       logger_common::mojom::LogSource::Unknown,
                       "fake_file2.cc", 124, "fake message2");
  Logger::Entry entry3(base::Time::Now(),
                       logger_common::mojom::LogSource::Unknown,
                       "fake_file3.cc", 125, "fake message3");

  EXPECT_FALSE(logger.ShouldEnableDebugLogs());
  EXPECT_CALL(observer, OnNewLog(_)).Times(0);
  logger.Log(entry1.event_time, entry1.log_source, entry1.source_file,
             entry1.source_line, entry1.message);

  logger.AddObserver(&observer);
  EXPECT_TRUE(logger.ShouldEnableDebugLogs());

  EXPECT_CALL(observer, OnNewLog(entry2)).Times(1);
  logger.Log(entry2.event_time, entry2.log_source, entry2.source_file,
             entry2.source_line, entry2.message);

  logger.RemoveObserver(&observer);
  EXPECT_FALSE(logger.ShouldEnableDebugLogs());

  EXPECT_CALL(observer, OnNewLog(_)).Times(0);
  logger.Log(entry3.event_time, entry3.log_source, entry3.source_file,
             entry3.source_line, entry3.message);
  logger.AddObserver(&observer);
}

TEST_F(DataSharingLoggerImplTest, CommandLineAlwaysLogs) {
  MockObserver observer1;
  MockObserver observer2;
  MockObserver observer3;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      data_sharing::kDataSharingDebugLoggingEnabled);
  std::unique_ptr<Logger> logger = std::make_unique<LoggerImpl>();
  EXPECT_TRUE(logger->ShouldEnableDebugLogs());

  Logger::Entry entry1(base::Time::Now(),
                       logger_common::mojom::LogSource::Unknown, "fake_file.cc",
                       123, "fake message");
  Logger::Entry entry2(base::Time::Now(),
                       logger_common::mojom::LogSource::Unknown,
                       "fake_file2.cc", 124, "fake message2");
  Logger::Entry entry3(base::Time::Now(),
                       logger_common::mojom::LogSource::Unknown,
                       "fake_file3.cc", 125, "fake message3");

  EXPECT_CALL(observer1, OnNewLog(_)).Times(0);
  EXPECT_CALL(observer2, OnNewLog(_)).Times(0);
  EXPECT_CALL(observer3, OnNewLog(_)).Times(0);

  logger->Log(entry1.event_time, entry1.log_source, entry1.source_file,
              entry1.source_line, entry1.message);
  logger->Log(entry2.event_time, entry2.log_source, entry2.source_file,
              entry2.source_line, entry2.message);

  EXPECT_CALL(observer1, OnNewLog(entry1)).Times(1);
  EXPECT_CALL(observer1, OnNewLog(entry2)).Times(1);
  EXPECT_CALL(observer1, OnNewLog(entry3)).Times(0);
  EXPECT_CALL(observer2, OnNewLog(_)).Times(0);
  EXPECT_CALL(observer3, OnNewLog(_)).Times(0);
  logger->AddObserver(&observer1);

  EXPECT_CALL(observer1, OnNewLog(entry3)).Times(1);
  EXPECT_CALL(observer2, OnNewLog(_)).Times(0);
  logger->Log(entry3.event_time, entry3.log_source, entry3.source_file,
              entry3.source_line, entry3.message);

  EXPECT_CALL(observer1, OnNewLog(_)).Times(0);
  EXPECT_CALL(observer2, OnNewLog(entry1)).Times(1);
  EXPECT_CALL(observer2, OnNewLog(entry2)).Times(1);
  EXPECT_CALL(observer2, OnNewLog(entry3)).Times(1);
  EXPECT_CALL(observer3, OnNewLog(_)).Times(0);
  logger->AddObserver(&observer2);

  EXPECT_CALL(observer1, OnNewLog(_)).Times(0);
  EXPECT_CALL(observer2, OnNewLog(_)).Times(0);
  EXPECT_CALL(observer3, OnNewLog(entry1)).Times(1);
  EXPECT_CALL(observer3, OnNewLog(entry2)).Times(1);
  EXPECT_CALL(observer3, OnNewLog(entry3)).Times(1);
  logger->RemoveObserver(&observer1);
  logger->AddObserver(&observer3);
}

}  // namespace
}  // namespace data_sharing
