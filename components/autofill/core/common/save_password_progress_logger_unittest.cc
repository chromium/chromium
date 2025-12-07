// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/save_password_progress_logger.h"

#include <stddef.h>

#include <limits>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using base::UTF8ToUTF16;

const char kTestString[] = "Message";  // Corresponds to STRING_MESSAGE.

class TestLogger : public SavePasswordProgressLogger {
 public:
  bool LogsContainSubstring(const std::string& substring) {
    return accumulated_log_.find(substring) != std::string::npos;
  }

  std::string accumulated_log() { return accumulated_log_; }

 private:
  void SendLog(const std::string& log) override {
    accumulated_log_.append(log);
  }

  std::string accumulated_log_;
};

TEST(SavePasswordProgressLoggerTest, LogHTMLForm) {
  TestLogger logger;
  logger.LogHTMLForm(SavePasswordProgressLogger::STRING_MESSAGE,
                     "form_name",
                     GURL("http://example.org/verysecret?verysecret"));
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_TRUE(logger.LogsContainSubstring("form_name"));
  EXPECT_TRUE(logger.LogsContainSubstring("http://example.org"));
  EXPECT_FALSE(logger.LogsContainSubstring("verysecret"));
}

TEST(SavePasswordProgressLoggerTest, LogURL) {
  TestLogger logger;
  logger.LogURL(SavePasswordProgressLogger::STRING_MESSAGE,
                GURL("http://example.org/verysecret?verysecret"));
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_TRUE(logger.LogsContainSubstring("http://example.org"));
  EXPECT_FALSE(logger.LogsContainSubstring("verysecret"));
}

TEST(SavePasswordProgressLoggerTest, LogBooleanTrue) {
  TestLogger logger;
  logger.LogBoolean(SavePasswordProgressLogger::STRING_MESSAGE, true);
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_TRUE(logger.LogsContainSubstring("true"));
}

TEST(SavePasswordProgressLoggerTest, LogBooleanFalse) {
  TestLogger logger;
  logger.LogBoolean(SavePasswordProgressLogger::STRING_MESSAGE, false);
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_TRUE(logger.LogsContainSubstring("false"));
}

TEST(SavePasswordProgressLoggerTest, LogSignedNumber) {
  TestLogger logger;
  int signed_number = -12345;
  logger.LogNumber(SavePasswordProgressLogger::STRING_MESSAGE, signed_number);
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_TRUE(logger.LogsContainSubstring("-12345"));
}

TEST(SavePasswordProgressLoggerTest, LogUnsignedNumber) {
  TestLogger logger;
  size_t unsigned_number = 654321;
  logger.LogNumber(SavePasswordProgressLogger::STRING_MESSAGE, unsigned_number);
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
  EXPECT_TRUE(logger.LogsContainSubstring("654321"));
}

TEST(SavePasswordProgressLoggerTest, LogMessage) {
  TestLogger logger;
  logger.LogMessage(SavePasswordProgressLogger::STRING_MESSAGE);
  SCOPED_TRACE(testing::Message() << "Log string = ["
                                  << logger.accumulated_log() << "]");
  EXPECT_TRUE(logger.LogsContainSubstring(kTestString));
}

// Test that none of the strings associated to string IDs contain the '.'
// character.
TEST(SavePasswordProgressLoggerTest, NoFullStops) {
  for (int id = 0; id < SavePasswordProgressLogger::STRING_MAX; ++id) {
    TestLogger logger;
    logger.LogMessage(static_cast<SavePasswordProgressLogger::StringID>(id));
    EXPECT_FALSE(logger.LogsContainSubstring("."))
        << "Log string = [" << logger.accumulated_log() << "]";
  }
}

}  // namespace
}  // namespace autofill
