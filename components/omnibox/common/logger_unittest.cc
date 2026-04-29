// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/logger.h"

#include <optional>
#include <string>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"

namespace omnibox {

class MockLoggerObserver : public Logger::Observer {
 public:
  MOCK_METHOD(void,
              OnLogMessageAdded,
              (base::Time event_time,
               const std::string& tag,
               const std::string& source_file,
               uint32_t source_line,
               const std::string& message,
               const std::optional<std::string>& proto_type,
               const std::optional<std::string>& proto_base64),
              (override));
};

class LoggerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(LoggerTest, LogMessageWithProto) {
  Logger logger;
  MockLoggerObserver observer;
  logger.AddObserver(&observer);

  lens::ClientToAimMessage proto;
  proto.mutable_open_threads_view()->mutable_payload();

  EXPECT_CALL(
      observer,
      OnLogMessageAdded(
          testing::_, testing::Eq("TestTag"), testing::_, testing::_,
          testing::Eq("Test message"),
          testing::Optional(std::string("lens.chrome.ClientToAimMessage")),
          testing::Ne(std::nullopt)))
      .Times(1);

  logger.OnLogMessageAdded(base::Time::Now(), "TestTag", "test_file.cc", 123,
                           "Test message", "lens.chrome.ClientToAimMessage",
                           "base64data");

  logger.RemoveObserver(&observer);
}

TEST_F(LoggerTest, LogMessageBuilderWithProto) {
  Logger logger;
  MockLoggerObserver observer;
  logger.AddObserver(&observer);

  lens::ClientToAimMessage proto;
  proto.mutable_open_threads_view()->mutable_payload();

  EXPECT_CALL(
      observer,
      OnLogMessageAdded(
          testing::_, testing::Eq("BuilderTag"), testing::_, testing::_,
          testing::Eq("Hello world"),
          testing::Optional(std::string("lens.chrome.ClientToAimMessage")),
          testing::Ne(std::nullopt)))
      .Times(1);

  std::string type_name = "lens.chrome.ClientToAimMessage";
  {
    Logger::LogMessageBuilder("BuilderTag", "file.cc", 1, &logger)
            .WithProto(proto, type_name)
        << "Hello " << "world";
  }

  logger.RemoveObserver(&observer);
}

TEST_F(LoggerTest, LogMessageBuilderWithProtoDefaultType) {
  Logger* logger = Logger::GetInstance();
  MockLoggerObserver observer;
  logger->AddObserver(&observer);

  lens::ClientToAimMessage proto;
  proto.mutable_open_threads_view()->mutable_payload();

  // lens::ClientToAimMessage's type name is "lens.ClientToAimMessage"
  EXPECT_CALL(observer,
              OnLogMessageAdded(
                  testing::_, testing::Eq("MacroTag"), testing::_, testing::_,
                  testing::Eq("Macro message"),
                  testing::Optional(std::string("lens.ClientToAimMessage")),
                  testing::Ne(std::nullopt)))
      .Times(1);

  {
    OMNIBOX_LOG_WITH_PROTO("MacroTag", proto) << "Macro message";
  }

  logger->RemoveObserver(&observer);
}

}  // namespace omnibox
