// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_SYNTHESIS_RESPONSE_PARSER_H_
#define CHROME_SERVICES_READALOUD_SYNTHESIS_RESPONSE_PARSER_H_

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/services/readaloud/chunking/text_chunker.h"
#include "chrome/services/readaloud/word_timing.h"
#include "media/base/decoder_buffer.h"
#include "mojo/public/cpp/base/big_buffer.h"

namespace readaloud {

// Error categories when parsing speech synthesis response in the utility
// process. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
// LINT.IfChange(SynthesisParseError)
enum class SynthesisParseError {
  kEmptyBuffer = 0,
  kPayloadTooLarge = 1,
  kParseFailed = 2,
  kEmptyAudio = 3,
  kMaxValue = kEmptyAudio,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/readaloud/enums.xml:ReadAloudSynthesisParseError)

// Error categories when validating word timings from speech synthesis in the
// utility process. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
// LINT.IfChange(HighlightingTimingError)
enum class HighlightingTimingError {
  kCorrectionApplied = 0,
  kMissing = 1,
  kMaxValue = kMissing,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/readaloud/enums.xml:ReadAloudHighlightingTimingError)

struct ParsedSynthesisResult {
  bool success = false;
  scoped_refptr<media::DecoderBuffer> audio_buffer;
  std::vector<WordTiming> timings;
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

// Main IPC parsing function delegating validation to utility functions.
ParsedSynthesisResult ParseAndValidateSynthesisResponse(
    mojo_base::BigBuffer response_bytes,
    const TextChunk& chunk);

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_SYNTHESIS_RESPONSE_PARSER_H_
