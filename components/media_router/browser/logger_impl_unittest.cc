// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/logger_impl.h"

#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/media_router/common/mojom/logger.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

constexpr char kComponent[] = "MyComponent";
constexpr char kMessage[] = "My message";
constexpr char kSinkId[] = "cast:xyzzy";
constexpr char kMediaSource[] = "cast:ABCDEFGH";
constexpr char kSessionId[] = "6789012345";

class LoggerImplTest : public testing::Test {
 protected:
  void LogInfoWithSinkId(const std::string& sink_id) {
    logger_.LogInfo(mojom::LogCategory::kRoute, kComponent, kMessage, sink_id,
                    kMediaSource, kSessionId);
  }
  void LogWarningWithSessionId(const std::string& session_id) {
    logger_.LogWarning(mojom::LogCategory::kRoute, kComponent, kMessage,
                       kSinkId, kMediaSource, session_id);
  }
  void LogErrorWithSource(const std::string& media_source) {
    logger_.LogError(mojom::LogCategory::kRoute, kComponent, kMessage, kSinkId,
                     media_source, kSessionId);
  }

  std::string GetSinkId(const std::string& logs_json) {
    return GetAttributeOfFirstEntry(logs_json, "sinkId");
  }

  std::string GetSessionId(const std::string& logs_json) {
    return GetAttributeOfFirstEntry(logs_json, "sessionId");
  }

  std::string GetMediaSource(const std::string& logs_json) {
    return GetAttributeOfFirstEntry(logs_json, "mediaSource");
  }

  LoggerImpl logger_;

 private:
  std::string GetAttributeOfFirstEntry(const std::string& logs_json,
                                       const std::string& attribute) {
    base::Value logs = base::JSONReader::Read(logs_json).value();
    return *logs.GetList()[0].GetDict().FindString(attribute);
  }
};

TEST_F(LoggerImplTest, RecordAndGetLogs) {
  const base::Time time1 = base::Time::Now();
  const base::Time time2 = time1 + base::Seconds(20);
  const std::string expected_logs =
      R"([
    {
      "severity": "Error",
      "category": "Route",
      "time": ")" +
      base::UTF16ToUTF8(base::TimeFormatTimeOfDayWithMilliseconds(time1)) +
      R"(",
      "component": "MyComponent",
      "message": "My message",
      "sinkId": "cast:xyzz",
      "mediaSource": "cast:ABCDEFGH",
      "sessionId": "678901234"
    },
    {
      "severity": "Info",
      "category": "UI",
      "time": ")" +
      base::UTF16ToUTF8(base::TimeFormatTimeOfDayWithMilliseconds(time2)) +
      R"(",
      "component": "Component 2",
      "message": "Message 2",
      "sinkId": "cast:abcd",
      "mediaSource": "cast:IJKLMNOP",
      "sessionId": "stuvwxyz0"
    }
  ])";

  logger_.Log(LoggerImpl::Severity::kError, mojom::LogCategory::kRoute, time1,
              kComponent, kMessage, kSinkId, kMediaSource, kSessionId);
  logger_.Log(LoggerImpl::Severity::kInfo, mojom::LogCategory::kUi, time2,
              "Component 2", "Message 2", "cast:abcdefgh", "cast:IJKLMNOP",
              "stuvwxyz0123");
  const std::string logs = logger_.GetLogsAsJson();
  EXPECT_EQ(base::JSONReader::Read(logs),
            base::JSONReader::Read(expected_logs));
}

TEST_F(LoggerImplTest, TruncateSinkId) {
  LogInfoWithSinkId(kSinkId);
  const std::string logs = logger_.GetLogsAsJson();
  EXPECT_EQ(GetSinkId(logs), "cast:xyzz");
}

TEST_F(LoggerImplTest, TruncateSessionId) {
  LogWarningWithSessionId(kSessionId);
  const std::string logs = logger_.GetLogsAsJson();
  EXPECT_EQ(GetSessionId(logs), "678901234");
}

TEST_F(LoggerImplTest, HandleMirroringMediaSource) {
  // A mirroring source should get logged as-is.
  const std::string source = "urn:x-org.chromium.media:source:tab:*";
  LogErrorWithSource(source);
  const std::string logs = logger_.GetLogsAsJson();
  EXPECT_EQ(GetMediaSource(logs), source);
}

TEST_F(LoggerImplTest, TrimCastMediaSource) {
  LogErrorWithSource("cast:ABCDEFGH?a=b&c=d");
  const std::string logs = logger_.GetLogsAsJson();
  EXPECT_EQ(GetMediaSource(logs), "cast:ABCDEFGH");
}

TEST_F(LoggerImplTest, TrimPresentationMediaSource) {
  LogErrorWithSource("https://presentation.example.com/receiver.html?a=b");
  const std::string logs = logger_.GetLogsAsJson();
  EXPECT_EQ(GetMediaSource(logs), "https://presentation.example.com/");
}

}  // namespace media_router
