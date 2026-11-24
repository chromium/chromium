// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/common/frame/frame_trace_flow.h"

#include "base/time/time.h"
#include "media/base/demuxer_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_streaming {
namespace {

using Type = media::DemuxerStream::Type;

// The browser and renderer processes compute this ID separately, so it must
// not depend on anything that differs between processes. These values pin the
// layout, which catches per-process seeding such as base::HashInts().
TEST(FrameTraceFlowTest, IdIsStableAcrossProcesses) {
  EXPECT_EQ(GetRendererHandoffFlowId(Type::AUDIO, base::Microseconds(0)),
            0x8C57'1000'0000'0000u);
  EXPECT_EQ(GetRendererHandoffFlowId(Type::VIDEO, base::Microseconds(33333)),
            0x8C57'2000'0000'8235u);
}

TEST(FrameTraceFlowTest, IdDependsOnTypeAndTimestamp) {
  const base::TimeDelta timestamp = base::Milliseconds(40);
  EXPECT_NE(GetRendererHandoffFlowId(Type::AUDIO, timestamp),
            GetRendererHandoffFlowId(Type::VIDEO, timestamp));
  EXPECT_NE(GetRendererHandoffFlowId(Type::VIDEO, timestamp),
            GetRendererHandoffFlowId(Type::VIDEO, base::Milliseconds(80)));
}

// Open Screen's frame flow IDs always set bit 62. These IDs keep it clear so
// that the two never collide.
TEST(FrameTraceFlowTest, IdNeverCollidesWithOpenScreenFlowIds) {
  constexpr uint64_t kOpenScreenMarkerBit = uint64_t{1} << 62;
  EXPECT_EQ(GetRendererHandoffFlowId(Type::VIDEO, base::Hours(1000)) &
                kOpenScreenMarkerBit,
            0u);
}

}  // namespace
}  // namespace cast_streaming
