// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REPETITION_CHECKER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REPETITION_CHECKER_H_

#include <string>
#include <string_view>

namespace optimization_guide {

// These params configure the repetition checker. See HasRepeatingSuffix()
// below for explanation. A value of 2 for num repeats and 16 for min repeat
// chars would mean we will halt a response once it repeats at least 16 chars 2
// times at the end of the response.
inline constexpr int GetOnDeviceModelNumRepeats() {
  return 2;
}
inline constexpr int GetOnDeviceModelMinRepeatChars() {
  return 16;
}

// Whether the response should be retracted if repeats are detected.
inline constexpr bool GetOnDeviceModelRetractRepeats() {
  return true;
}

// Returns true if `text` has a suffix which repeats `num_repeats` times with at
// least a length of `min_chars`.
bool HasRepeatingSuffix(int min_chars, int num_repeats, std::string_view text);

// As above, using GetOnDeviceModelMinRepeatChars() and
// GetOnDeviceModelNumRepeats().
bool HasRepeatingSuffix(std::string_view text);

// Get the number of trailing new lines at the end of text
size_t GetNumTrailingNewlines(std::string_view text);

// Check if the text contains any character that is not a newline.
bool CheckTextContainsNonNewline(std::string_view text);

// A buffer used to hold the trailing newlines from input chunk.
class NewlineBuffer {
 public:
  struct Chunk {
    std::string text;
    size_t num_tokens;
  };
  // First, if we encounter any non-newline character in chunk, prepare a chunk
  // with num_newlines_ newline characters, and add all contents in input chunk
  // that are not part of trailing newlines to it. Then all trailing newlines
  // are added to an internal buffer. Finally, the prepared chunk is returned.
  Chunk Append(std::string_view text);

 private:
  size_t num_newlines_ = 0;
  size_t num_tokens_ = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REPETITION_CHECKER_H_
