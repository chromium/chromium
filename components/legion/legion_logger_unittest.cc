// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/common/legion_logger.h"

#include <string_view>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace legion {
namespace {

using ::testing::_;

class MockObserver : public LegionLogger::Observer {
 public:
  MOCK_METHOD(void, OnLogInfo, (std::string_view message), (override));
  MOCK_METHOD(void, OnLogError, (std::string_view message), (override));
};

TEST(LegionLoggerTest, NotifiesObserversOnLogging) {
  LegionLogger logger;
  MockObserver observer;
  logger.AddObserver(&observer);

  EXPECT_CALL(observer, OnLogInfo("info message"));
  logger.LogInfo("info message");

  EXPECT_CALL(observer, OnLogError("error message"));
  logger.LogError("error message");

  logger.RemoveObserver(&observer);

  // Should not notify after removal.
  EXPECT_CALL(observer, OnLogInfo(_)).Times(0);
  logger.LogInfo("another info message");
}

}  // namespace
}  // namespace legion
