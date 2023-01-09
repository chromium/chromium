// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/scoped_timed_task_logger.h"

#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

class ScopedTimedTaskLoggerTest : public testing::Test {
 public:
  // Intercepts all log messages.
  static bool LogMessageHandler(int severity,
                                const char* file,
                                int line,
                                size_t message_start,
                                const std::string& str) {
    DCHECK(active_logging_messages_);
    active_logging_messages_->push_back(str);
    return false;
  }

  bool LoggingMessagesContain(const std::string& sub_string) {
    std::vector<std::string>::const_iterator line = logging_messages_.begin();
    for (; line != logging_messages_.end(); ++line) {
      if (StringContainsCaseInsensitive(*line, sub_string))
        return true;
    }
    return false;
  }

  void FlushMessages() { logging_messages_.clear(); }

  void SetUp() override {
    DCHECK(active_logging_messages_ == nullptr);
    active_logging_messages_ = &logging_messages_;
    logging::SetLogMessageHandler(&LogMessageHandler);
  }

  void TearDown() override {
    logging::SetLogMessageHandler(nullptr);
    logging_messages_.clear();
    DCHECK(active_logging_messages_ == &logging_messages_);
    active_logging_messages_ = nullptr;
  }

 private:
  std::vector<std::string> logging_messages_;
  static std::vector<std::string>* active_logging_messages_;
};

std::vector<std::string>* ScopedTimedTaskLoggerTest::active_logging_messages_ =
    nullptr;

}  // namespace

TEST_F(ScopedTimedTaskLoggerTest, CallbackCalled) {
  bool callback_called = false;
  {
    ScopedTimedTaskLogger timer(base::BindLambdaForTesting(
        [&](const base::TimeDelta&) { callback_called = true; }));
  }
  EXPECT_TRUE(callback_called);
}

TEST_F(ScopedTimedTaskLoggerTest, NoLog) {
  static const char kNoShow[] = "Should not show up";
  {
    ScopedTimedTaskLogger no_logs(base::BindOnce(
        ScopedTimedTaskLogger::LogIfExceedThreshold, kNoShow, base::Days(1)));
  }
  EXPECT_FALSE(LoggingMessagesContain(kNoShow));
}

TEST_F(ScopedTimedTaskLoggerTest, Log) {
  static const char kShow[] = "Should show up";
  {
    ScopedTimedTaskLogger logs(
        base::BindOnce(ScopedTimedTaskLogger::LogIfExceedThreshold, kShow,
                       base::Milliseconds(0)));
    ::Sleep(2);
  }
  EXPECT_TRUE(LoggingMessagesContain(kShow));
}

}  // namespace chrome_cleaner
