// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/common/private_ai_logger.h"

#include <string_view>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {
namespace {

using ::testing::_;

class MockObserver : public PrivateAiLogger::Observer {
 public:
  MOCK_METHOD(void,
              OnLogInfo,
              (const base::Location& location, std::string_view message),
              (override));
  MOCK_METHOD(void,
              OnLogError,
              (const base::Location& location, std::string_view message),
              (override));
};

TEST(PrivateAiLoggerTest, NotifiesObserversOnLogging) {
  PrivateAiLogger logger;
  MockObserver observer;
  logger.AddObserver(&observer);

  EXPECT_CALL(observer, OnLogInfo(_, "info message"));
  logger.LogInfo(FROM_HERE, "info message");

  EXPECT_CALL(observer, OnLogError(_, "error message"));
  logger.LogError(FROM_HERE, "error message");

  logger.RemoveObserver(&observer);

  // Should not notify after removal.
  EXPECT_CALL(observer, OnLogInfo(_, _)).Times(0);
  logger.LogInfo(FROM_HERE, "another info message");
}

}  // namespace
}  // namespace private_ai
