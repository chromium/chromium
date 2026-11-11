// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_INTERNAL_TEST_SUPPORT_MOCK_STREAM_FRAMER_H_
#define COMPONENTS_STREAM_TRANSPORT_INTERNAL_TEST_SUPPORT_MOCK_STREAM_FRAMER_H_

#include <string_view>

#include "components/stream_transport/internal/stream_framer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace stream_transport {

class MockStreamFramer : public StreamFramer {
 public:
  MockStreamFramer();
  ~MockStreamFramer() override;

  MOCK_METHOD(FeedResult, Feed, (std::string_view), (override));
};

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_INTERNAL_TEST_SUPPORT_MOCK_STREAM_FRAMER_H_
