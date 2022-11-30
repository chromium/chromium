// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_MOCK_LOG_SINK_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_MOCK_LOG_SINK_H_

#include "components/download/internal/background_service/log_sink.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace download {
namespace test {

class MockLogSink : public LogSink {
 public:
  MockLogSink();
  ~MockLogSink() override;

  // LogSink implementation.
  MOCK_METHOD0(OnServiceStatusChanged, void());
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_MOCK_LOG_SINK_H_
