// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_TIMING_TRACKER_H_
#define CHROMECAST_COMMON_TIMING_TRACKER_H_

#include <string_view>

#include "base/time/time.h"

// Convenience wrapper around creating a TimingTracker instance.
// Typically this is created at the start of a function to measure
// the duration the function takes to complete (simple profile).
// If a span is needed over callbacks/async work, prefer directly managing
// the lifetime of the object instead (e.g. allocate the object & release
// when the task completes).
#define CHROMECAST_TIMING_TRACKER            \
  chromecast::TimingTracker timing_tracker_( \
      chromecast::TimingTracker::GetFilename(__FILE__), __func__)

namespace chromecast {

// Provides a simple time span profiler/tracker.
// When constructed, an initial timestamp will be taken.
// When destructed, a final timestamp will be taken.
// On destruction, a log will be written out in the format:
// |file| |measured_tag| start us: [timestamp] end us: [timestamp] dt us:
// [duration]
class TimingTracker {
 public:
  TimingTracker(std::string_view file, std::string_view measured_tag);
  ~TimingTracker();

  // __FILE__ may contain the path to the file, instead of just the
  // associated filename, this function will strip the filepath at compile time
  // so we have shorter more readable logs.
  static constexpr std::string_view GetFilename(const char* path) {
    if (!path) {
      return {};
    }
    std::string_view path_piece(path);
    auto last_slash_pos = path_piece.find_last_of('/');
    if (last_slash_pos != std::string_view::npos) {
      return path_piece.substr(last_slash_pos + 1);
    }
    return path_piece;
  }

 private:
  std::string_view file_;
  std::string_view measured_tag_;
  base::Time start_;
};

}  // namespace chromecast

#endif  // CHROMECAST_COMMON_TIMING_TRACKER_H_
