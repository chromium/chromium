// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/synthesis_response_parser.h"

#include <algorithm>
#include <utility>

#include "base/containers/span.h"
#include "base/logging.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "components/optimization_guide/proto/features/read_aloud_synthesize.pb.h"

namespace readaloud {

TimingBounds CalculateMonotonicTimingBounds(
    int64_t raw_start_ms,
    std::optional<int64_t> raw_next_start_ms) {
  // Post-Condition Invariant 1: start_time >= 0ms.
  const int64_t start_ms = std::max<int64_t>(0, raw_start_ms);
  const base::TimeDelta start_time = base::Milliseconds(start_ms);

  // Post-Condition Invariant 2: end_time >= start_time + 1ms (non-zero duration).
  int64_t end_ms = start_ms + 100;  // Default 100ms duration fallback
  if (raw_next_start_ms.has_value()) {
    end_ms = std::max<int64_t>(start_ms + 1, raw_next_start_ms.value());
  }
  const base::TimeDelta end_time = base::Milliseconds(end_ms);

  return {start_time, end_time};
}

ParsedSynthesisResult ParseAndValidateSynthesisResponse(
    mojo_base::BigBuffer response_bytes,
    const TextChunk& chunk) {
  ParsedSynthesisResult result;

  if (response_bytes.size() == 0 ||
      response_bytes.size() > kMaxMojoPayloadSizeBytes) {
    return result;
  }

  optimization_guide::proto::ReadAloudSynthesizeResponse response;
  if (!response.ParseFromArray(response_bytes.data(), response_bytes.size())) {
    DLOG(WARNING) << "Failed to deserialize ReadAloudSynthesizeResponse proto";
    return result;
  }

  if (response.audio_bytes().empty()) {
    DLOG(WARNING) << "ReadAloudSynthesizeResponse contains empty audio_bytes";
    return result;
  }

  result.audio_buffer = media::DecoderBuffer::CopyFrom(
      base::as_byte_span(response.audio_bytes()));

  result.timings.reserve(response.timings_size());
  for (int i = 0; i < response.timings_size(); ++i) {
    const optimization_guide::proto::WordTiming& timing = response.timings(i);
    WordTiming t;

    std::optional<int64_t> next_start_ms;
    if (i + 1 < response.timings_size()) {
      next_start_ms = response.timings(i + 1).time_offset_ms();
    }

    TimingBounds bounds =
        CalculateMonotonicTimingBounds(timing.time_offset_ms(), next_start_ms);
    t.start_time = bounds.start_time;
    t.end_time = bounds.end_time;

    const int32_t max_offset = static_cast<int32_t>(chunk.text.size());
    const int32_t start = std::clamp(timing.start_offset(), 0, max_offset);
    const int32_t end = std::clamp(timing.end_offset(), start, max_offset);

    t.start_character_offset =
        static_cast<uint32_t>(chunk.start_code_unit_offset + start);
    t.end_character_offset =
        static_cast<uint32_t>(chunk.start_code_unit_offset + end);

    result.timings.push_back(std::move(t));
  }

  result.success = true;
  return result;
}

}  // namespace readaloud
