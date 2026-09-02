// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_SYNTHESIS_RESPONSE_PARSER_H_
#define CHROME_SERVICES_READALOUD_SYNTHESIS_RESPONSE_PARSER_H_

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "media/base/decoder_buffer.h"
#include "mojo/public/cpp/base/big_buffer.h"

namespace readaloud {

struct ParsedSynthesisResult {
  bool success = false;
  scoped_refptr<media::DecoderBuffer> audio_buffer;
  std::vector<DecodedAudioSegment::WordTiming> timings;
};

struct TimingBounds {
  base::TimeDelta start_time;
  base::TimeDelta end_time;
};

// Pure utility function enforcing temporal monotonicity post-conditions:
// Invariant 1: start_time >= 0ms.
// Invariant 2: end_time >= start_time.
TimingBounds CalculateMonotonicTimingBounds(
    int64_t raw_start_ms,
    std::optional<int64_t> raw_next_start_ms = std::nullopt);

// Pure utility function enforcing UTF-16 code unit boundary safety:
// Invariant 3: 0 <= start_offset < end_offset <= chunk_text.size().
std::string ExtractUTF8WordText(std::u16string_view chunk_text,
                                int32_t start_offset,
                                int32_t end_offset);

// Main IPC parsing function delegating validation to utility functions.
ParsedSynthesisResult ParseAndValidateSynthesisResponse(
    mojo_base::BigBuffer response_bytes,
    std::u16string_view chunk_text = u"");

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_SYNTHESIS_RESPONSE_PARSER_H_
