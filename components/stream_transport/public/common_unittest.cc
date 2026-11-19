// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/public/common.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace stream_transport {
namespace {

TEST(StreamTransportCommonTest, StartClientSessionErrorFromHttpStatus) {
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(-1),
            StartClientSessionError::kTransientError);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(0),
            StartClientSessionError::kTransientError);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(100),
            StartClientSessionError::kPermanentError);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(302),
            StartClientSessionError::kPermanentError);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(304),
            StartClientSessionError::kPermanentError);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(400),
            StartClientSessionError::kPermanentError);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(401),
            StartClientSessionError::kPermanentError);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(403),
            StartClientSessionError::kPermanentError);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(404),
            StartClientSessionError::kPermanentError);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(409),
            StartClientSessionError::kSessionIdConflict);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(429),
            StartClientSessionError::kTransientError);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(500),
            StartClientSessionError::kTransientError);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(502),
            StartClientSessionError::kTransientError);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(503),
            StartClientSessionError::kTransientError);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(599),
            StartClientSessionError::kTransientError);
  EXPECT_EQ(StartClientSessionErrorFromHttpStatus(600),
            StartClientSessionError::kPermanentError);
}

}  // namespace
}  // namespace stream_transport
