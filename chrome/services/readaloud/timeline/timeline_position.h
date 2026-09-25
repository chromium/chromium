// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_TIMELINE_TIMELINE_POSITION_H_
#define CHROME_SERVICES_READALOUD_TIMELINE_TIMELINE_POSITION_H_

#include <cstddef>
#include <cstdint>

#include "base/time/time.h"
#include "chrome/services/readaloud/chunking/text_chunker.h"

namespace readaloud {

// Represents a 0-based sentence chunk index and character offset bounds within
// that chunk.
struct IndexedCharRange {
  uint32_t index = 0;
  uint32_t start_char_offset = 0;
  uint32_t end_char_offset = 0;

  friend bool operator==(const IndexedCharRange&, const IndexedCharRange&) = default;
};

// Represents document-wide cumulative character offset bounds.
struct GlobalCharRange {
  size_t start_offset = 0;
  size_t end_offset = 0;

  friend bool operator==(const GlobalCharRange&, const GlobalCharRange&) = default;
};

// Represents 1.0x normalized media time bounds.
struct TimeRange {
  base::TimeDelta start_time;
  base::TimeDelta end_time;
  base::TimeDelta duration() const { return end_time - start_time; }

  friend bool operator==(const TimeRange&, const TimeRange&) = default;
};

// Captures a complete coordinate mapping across sentence chunk, document
// character offset, and normalized 1.0x media time domains.
struct TimelinePosition {
  TimelinePosition() = default;
  TimelinePosition(uint32_t chunk_index,
                   const TextChunk& text_chunk,
                   uint32_t char_offset_in_chunk,
                   base::TimeDelta start_time,
                   base::TimeDelta end_time)
      : chunk{.index = chunk_index,
              .start_char_offset = char_offset_in_chunk,
              .end_char_offset =
                  static_cast<uint32_t>(text_chunk.text.size())},
        global_char{.start_offset = text_chunk.start_code_unit_offset +
                                    char_offset_in_chunk,
                    .end_offset = text_chunk.start_code_unit_offset +
                                  text_chunk.text.size()},
        time{.start_time = start_time, .end_time = end_time} {}

  IndexedCharRange chunk;
  GlobalCharRange global_char;
  TimeRange time;

  friend bool operator==(const TimelinePosition&, const TimelinePosition&) = default;
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_TIMELINE_TIMELINE_POSITION_H_
