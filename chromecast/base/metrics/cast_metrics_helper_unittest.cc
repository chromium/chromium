// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/metrics/cast_metrics_helper.h"

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::SaveArg;
using ::testing::_;

namespace chromecast {
namespace metrics {

namespace {

constexpr char kAppId1[] = "APP_ID_1";
constexpr char kAppId2[] = "APP_ID_2";
constexpr char kAppId3[] = "APP_ID_3";
constexpr char kEvent[] = "EVENT";
constexpr char kSdkVersion[] = "SDK_VERSION";
constexpr char kSessionId1[] = "SESSION_ID_1";
constexpr char kSessionId2[] = "SESSION_ID_2";
constexpr char kSessionId3[] = "SESSION_ID_3";
constexpr int kValue = 123;

constexpr base::TimeDelta kAppLoadTimeout = base::Minutes(5);

MATCHER_P2(HasDouble, key, value, "") {
  const std::optional<base::Value> v = base::JSONReader::Read(arg);
  if (!v || !v->is_dict()) {
    return false;
  }

  const base::Value::Dict& dict = v->GetDict();
  return dict.FindDouble(key) == value;
}

MATCHER_P2(HasInt, key, value, "") {
  const std::optional<base::Value> v = base::JSONReader::Read(arg);
  if (!v || !v->is_dict()) {
    return false;
  }

  const base::Value::Dict& dict = v->GetDict();
  return dict.FindInt(key) == value;
}

MATCHER_P2(HasString, key, value, "") {
  const std::optional<base::Value> v = base::JSONReader::Read(arg);
  if (!v || !v->is_dict()) {
    return false;
  }

  const base::Value::Dict& dict = v->GetDict();
  const std::string* stored_value = dict.FindString(key);
  return stored_value && (*stored_value == value);
}

}  // namespace

class MockMetricsSink : public CastMetricsHelper::MetricsSink {
 public:
  MOCK_METHOD1(OnAction, void(const std::string&));
  MOCK_METHOD3(OnEnumerationEvent, void(const std::string&, int, int));
  MOCK_METHOD5(OnTimeEvent,
               void(const std::string&,
                    base::TimeDelta,
                    base::TimeDelta,
                    base::TimeDelta,
                    int));
};

class CastMetricsHelperTest : public ::testing::Test {
 public:
  CastMetricsHelperTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        metrics_helper_(task_environment_.GetMainThreadTaskRunner(),
                        task_environment_.GetMockTickClock()) {
    metrics_helper_.SetMetricsSink(&metrics_sink_);
  }

  base::test::TaskEnvironment task_environment_;
  MockMetricsSink metrics_sink_;
  CastMetricsHelper metrics_helper_;
};

TEST_F(CastMetricsHelperTest, RecordEventWithValue) {
  const auto expected_time = task_environment_.NowTicks();

  EXPECT_CALL(
      metrics_sink_,
      OnAction(
          AllOf(HasString("name", kEvent),
                HasDouble("time",
                          (expected_time - base::TimeTicks()).InMicroseconds()),
                HasInt("value", kValue))));
  metrics_helper_.RecordApplicationEventWithValue(kEvent, kValue);
}

TEST_F(CastMetricsHelperTest, RecordApplicationEvent) {
  metrics_helper_.DidStartLoad(kAppId1);
  metrics_helper_.DidCompleteLoad(kAppId1, kSessionId1);
  metrics_helper_.UpdateSDKInfo(kSdkVersion);

  const auto expected_time = task_environment_.NowTicks();

  EXPECT_CALL(
      metrics_sink_,
      OnAction(AllOf(
          HasString("name", kEvent),
          HasDouble("time",
                    (expected_time - base::TimeTicks()).InMicroseconds()),
          HasString("app_id", kAppId1), HasString("session_id", kSessionId1),
          HasString("sdk_version", kSdkVersion))));
  metrics_helper_.RecordApplicationEvent(kEvent);
}

TEST_F(CastMetricsHelperTest, RecordApplicationEventWithValue) {
  metrics_helper_.DidStartLoad(kAppId1);
  metrics_helper_.DidCompleteLoad(kAppId1, kSessionId1);
  metrics_helper_.UpdateSDKInfo(kSdkVersion);

  const auto expected_time = task_environment_.NowTicks();

  EXPECT_CALL(
      metrics_sink_,
      OnAction(AllOf(
          HasString("name", kEvent),
          HasDouble("time",
                    (expected_time - base::TimeTicks()).InMicroseconds()),
          HasString("app_id", kAppId1), HasString("session_id", kSessionId1),
          HasString("sdk_version", kSdkVersion), HasInt("value", kValue))));
  metrics_helper_.RecordApplicationEventWithValue(kEvent, kValue);
}

TEST_F(CastMetricsHelperTest, LogTimeToFirstPaint) {
  metrics_helper_.DidStartLoad(kAppId1);
  metrics_helper_.DidCompleteLoad(kAppId1, kSessionId1);

  constexpr base::TimeDelta kTimeToFirstPaint = base::Seconds(5);
  task_environment_.FastForwardBy(kTimeToFirstPaint);

  EXPECT_CALL(metrics_sink_, OnTimeEvent(AllOf(HasSubstr(kAppId1),
                                               HasSubstr("TimeToFirstPaint")),
                                         kTimeToFirstPaint, _, _, _));
  metrics_helper_.LogTimeToFirstPaint();
}

TEST_F(CastMetricsHelperTest, LogTimeToFirstAudio) {
  metrics_helper_.DidStartLoad(kAppId1);
  metrics_helper_.DidCompleteLoad(kAppId1, kSessionId1);

  constexpr base::TimeDelta kTimeToFirstAudio = base::Seconds(5);
  task_environment_.FastForwardBy(kTimeToFirstAudio);

  EXPECT_CALL(metrics_sink_, OnTimeEvent(AllOf(HasSubstr(kAppId1),
                                               HasSubstr("TimeToFirstAudio")),
                                         kTimeToFirstAudio, _, _, _));
  metrics_helper_.LogTimeToFirstAudio();
}

TEST_F(CastMetricsHelperTest, MultipleApps) {
  metrics_helper_.DidStartLoad(kAppId1);
  task_environment_.FastForwardBy(kAppLoadTimeout);
  metrics_helper_.DidStartLoad(kAppId2);
  metrics_helper_.DidStartLoad(kAppId3);
  metrics_helper_.DidCompleteLoad(kAppId3, kSessionId3);
  // kAppId2 should become the current app.
  metrics_helper_.DidCompleteLoad(kAppId2, kSessionId2);
  // kAppId1 should not become the current app because it timed out.
  metrics_helper_.DidCompleteLoad(kAppId1, kSessionId1);

  EXPECT_CALL(metrics_sink_,
              OnAction(AllOf(HasString("app_id", kAppId2),
                             HasString("session_id", kSessionId2))));
  metrics_helper_.RecordApplicationEvent(kEvent);
}

}  // namespace metrics
}  // namespace chromecast
