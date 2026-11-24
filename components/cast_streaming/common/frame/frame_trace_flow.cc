// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/common/frame/frame_trace_flow.h"

namespace cast_streaming {

namespace {

// Bit layout of the flow ID:
//   [63:48] kTag, which keeps bit 62 clear. Open Screen always sets bit 62 in
//           its frame flow IDs (see cast/streaming/public/frame_trace_ids.h
//           in Open Screen), so the two kinds of ID never collide.
//   [47:44] The stream type.
//   [43:0]  The buffer timestamp in microseconds. This wraps after about 200
//           days of stream time, which is fine for linking trace events.
//
// The ID must be the same in the browser and renderer processes, so it must
// not use base::HashInts() or similar, which are seeded per process.
constexpr uint64_t kTag = 0x8C57;
constexpr int kTagShift = 48;
constexpr int kTypeShift = 44;
constexpr uint64_t kTypeMask = 0xF;
constexpr uint64_t kTimestampMask = (uint64_t{1} << kTypeShift) - 1;

}  // namespace

uint64_t GetRendererHandoffFlowId(media::DemuxerStream::Type type,
                                  base::TimeDelta timestamp) {
  return (kTag << kTagShift) |
         ((static_cast<uint64_t>(type) & kTypeMask) << kTypeShift) |
         (static_cast<uint64_t>(timestamp.InMicroseconds()) & kTimestampMask);
}

}  // namespace cast_streaming
