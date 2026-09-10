// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_WORD_TIMING_H_
#define CHROME_SERVICES_READALOUD_WORD_TIMING_H_

#include <cstdint>

#include "base/time/time.h"

namespace readaloud {

// Struct tracking timing and character offset metadata for a single word.
// Synchronizes visual text highlighting in the UI with spoken audio playback.
struct WordTiming {
  // Start time offset relative to the segment start.
  base::TimeDelta start_time;

  // End time offset relative to the segment start.
  base::TimeDelta end_time;

  // Absolute character offset in the document text where the word begins.
  uint32_t start_character_offset = 0;

  // Absolute character offset in the document text where the word ends
  // (exclusive).
  uint32_t end_character_offset = 0;

  friend bool operator==(const WordTiming&, const WordTiming&) = default;
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_WORD_TIMING_H_
