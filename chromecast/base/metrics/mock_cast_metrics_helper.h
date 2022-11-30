// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_METRICS_MOCK_CAST_METRICS_HELPER_H_
#define CHROMECAST_BASE_METRICS_MOCK_CAST_METRICS_HELPER_H_

#include <string>

#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace metrics {

class MockCastMetricsHelper : public CastMetricsHelper {
 public:
  MockCastMetricsHelper();

  MockCastMetricsHelper(const MockCastMetricsHelper&) = delete;
  MockCastMetricsHelper& operator=(const MockCastMetricsHelper&) = delete;

  ~MockCastMetricsHelper() override;

  MOCK_METHOD2(UpdateCurrentAppInfo,
               void(const std::string& app_id, const std::string& session_id));
  MOCK_METHOD1(UpdateSDKInfo, void(const std::string& sdk_version));
  MOCK_METHOD0(LogMediaPlay, void());
  MOCK_METHOD0(LogMediaPause, void());
  MOCK_METHOD1(RecordSimpleAction, void(const std::string& action));
  MOCK_METHOD2(RecordEventWithValue,
               void(const std::string& action, int value));
  MOCK_METHOD1(RecordApplicationEvent, void(const std::string& event));
  MOCK_METHOD2(RecordApplicationEventWithValue,
               void(const std::string& event, int value));
  MOCK_METHOD0(LogTimeToFirstPaint, void());
  MOCK_METHOD0(LogTimeToFirstAudio, void());
  MOCK_METHOD2(LogTimeToBufferAv,
               void(BufferingType buffering_type, base::TimeDelta time));
  MOCK_CONST_METHOD2(GetMetricsNameWithAppName,
                     std::string(const std::string& prefix,
                                 const std::string& suffix));
  MOCK_METHOD1(SetMetricsSink, void(MetricsSink* delegate));
  MOCK_METHOD1(SetRecordActionCallback, void(RecordActionCallback callback));
};

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_BASE_METRICS_MOCK_CAST_METRICS_HELPER_H_
