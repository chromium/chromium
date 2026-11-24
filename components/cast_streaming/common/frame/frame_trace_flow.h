// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_COMMON_FRAME_FRAME_TRACE_FLOW_H_
#define COMPONENTS_CAST_STREAMING_COMMON_FRAME_FRAME_TRACE_FLOW_H_

#include <cstdint>

#include "base/time/time.h"
#include "media/base/demuxer_stream.h"

namespace cast_streaming {

// Returns the ID of the trace flow that links the browser process event for a
// frame sent to the renderer with the renderer process event for the same
// frame. The renderer does not know the Open Screen FrameId of a buffer, so
// the ID is made from what both processes know: the stream type and the
// buffer timestamp sent over Mojo.
uint64_t GetRendererHandoffFlowId(media::DemuxerStream::Type type,
                                  base::TimeDelta timestamp);

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_COMMON_FRAME_FRAME_TRACE_FLOW_H_
