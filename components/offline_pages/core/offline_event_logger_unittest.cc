// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_event_logger.h"

#include "base/strings/string_number_conversions.h"
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
  EventLoggerTestClient client;
  OfflineEventLogger logger;
  logger.SetClient(&client);
  EXPECT_TRUE(logger.GetIsLogging());
}

TEST(OfflineEventLoggerTest, SettingClientAndLog) {
  EventLoggerTestClient client;
  OfflineEventLogger logger;
  logger.SetClient(&client);

  logger.SetIsLogging(true);
  for (size_t i = 0; i < kMaxLogCount + 1; ++i)
    logger.RecordActivity(kMessage + base::NumberToString(i));
  std::vector<std::string> log;
  logger.GetLogs(&log);

  EXPECT_EQ(kMaxLogCount, log.size());
  EXPECT_EQ(client.last_log_message(), log[0].substr(kTimeLength));
  EXPECT_EQ(std::string(kMessage) + base::NumberToString(kMaxLogCount),
            client.last_log_message());
}

}  // namespace offline_pages
