// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_event_logger.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const char kMessage[] = "Message is ";
const int kTimeLength = 21;

class EventLoggerTestClient : public OfflineEventLogger::Client {
 public:
  void CustomLog(const std::string& message) override {
    last_log_message_ = message;
  }

  const std::string& last_log_message() { return last_log_message_; }

 private:
  std::string last_log_message_;
};

}  // namespace

TEST(OfflineEventLoggerTest, SettingClientEnableLogging) {
  OfflineEventLogger logger;
  EventLoggerTestClient client;
  logger.SetClient(&client);
  EXPECT_TRUE(logger.GetIsLogging());
}

TEST(OfflineEventLoggerTest, SettingClientAndLog) {
  OfflineEventLogger logger;
  EventLoggerTestClient client;
  logger.SetClient(&client);

  logger.SetIsLogging(true);
  for (size_t i = 0; i < kMaxLogCount + 1; ++i)
    logger.RecordActivity(kMessage + std::to_string(i));
  std::vector<std::string> log;
  logger.GetLogs(&log);

  EXPECT_EQ(kMaxLogCount, log.size());
  EXPECT_EQ(client.last_log_message(), log[0].substr(kTimeLength));
  EXPECT_EQ(std::string(kMessage) + std::to_string(kMaxLogCount),
            client.last_log_message());
}

}  // namespace offline_pages
