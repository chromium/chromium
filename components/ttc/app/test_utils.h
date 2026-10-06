// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TTC_APP_TEST_UTILS_H_
#define COMPONENTS_TTC_APP_TEST_UTILS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/ttc/app/public/tool_types.h"
#include "components/ttc/app/ttc_backend.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace ttc {

// A TtcBackend that records the connection state set by Connect()/Close() so
// that is_transport_connected() behaves like the real thing by default.
class MockTtcBackend : public TtcBackend {
 public:
  MockTtcBackend();
  ~MockTtcBackend() override;

  // Returns the observer passed to the last Connect() call.
  TtcBackend::Observer* observer() const { return observer_; }

  MOCK_METHOD(void, Connect, (TtcBackend::Observer*), (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(bool, is_transport_connected, (), (const, override));
  MOCK_METHOD(void, SendAudioChunk, (base::span<const int16_t>), (override));
  MOCK_METHOD(void, SendTextInput, (const std::string&), (override));
  MOCK_METHOD(void,
              SendContextUpdate,
              (const GURL&,
               const std::string&,
               const optimization_guide::proto::AnnotatedPageContent&),
              (override));
  MOCK_METHOD(void, ReportPlaybackStatus, (int64_t), (override));
  MOCK_METHOD(void,
              SendToolSetUpdate,
              (const std::vector<ToolDefinition>&),
              (override));

 private:
  bool is_transport_connected_ = false;
  raw_ptr<TtcBackend::Observer> observer_ = nullptr;
};

}  // namespace ttc

#endif  // COMPONENTS_TTC_APP_TEST_UTILS_H_
