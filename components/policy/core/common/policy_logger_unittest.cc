// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_logger.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::Property;

namespace policy {

namespace {

class MockObserver : public policy::PolicyLogger::Observer {
 public:
  MOCK_METHOD(void,
              OnLogsChanged,
              (const std::vector<policy::PolicyLogger::Log>& logs),
              (override));
};

void AddLogs(const std::string& message, PolicyLogger* policy_logger) {
  policy_logger->AddLog(policy::PolicyLogger::Log(
      policy::PolicyLogger::Log::LogSource::kPolicyFetching, message,
      FROM_HERE));
}

}  // namespace

TEST(PolicyLoggerTest, ObserverRegistered) {
  PolicyLogger policy_logger;
  MockObserver mock_observer;

  // Ensure OnLogsChanged is called when the observer is added to the logger.
  EXPECT_CALL(mock_observer,
              OnLogsChanged(ElementsAre(
                  Property(&PolicyLogger::Log::message,
                           Eq("Element Added Before Observer Creation")))))
      .Times(1);

  AddLogs("Element Added Before Observer Creation", &policy_logger);

  policy_logger.AddObserver(&mock_observer);

  // Ensure OnLogsChanged is called when a log is added after registration.
  EXPECT_CALL(mock_observer,
              OnLogsChanged(ElementsAre(
                  Property(&PolicyLogger::Log::message,
                           Eq("Element Added Before Observer Creation")),
                  Property(&PolicyLogger::Log::message,
                           Eq("Element Added After Observer Creation")))))
      .Times(1);
  AddLogs("Element Added After Observer Creation", &policy_logger);

  // Ensure OnLogsChanged is not called when observer is removed.
  EXPECT_CALL(mock_observer, OnLogsChanged(_)).Times(0);
  policy_logger.RemoveObserver(&mock_observer);
  AddLogs("Element Added After Observer Removal", &policy_logger);
}

}  // namespace policy