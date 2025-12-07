// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/repetition_checker.h"

#include "base/containers/adapters.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

namespace optimization_guide {

bool HasRepeatingSuffix(int min_chars, int num_repeats, std::string_view text) {
  if (num_repeats == 0) {
    return false;
  }

  size_t cur_size = min_chars;
  while (text.size() >= cur_size * num_repeats) {
    size_t index = text.size() - cur_size;
    bool is_repeating = true;
    // Check back in the string for consecutive matches of cur_size length.
    for (int i = 1; i < num_repeats; i++) {
      if (text.substr(index, cur_size) !=
          text.substr(index - cur_size * i, cur_size)) {
        is_repeating = false;
        break;
      }
    }
    if (is_repeating) {
      return true;
    }
    cur_size++;
  }
  return false;
}

bool HasRepeatingSuffix(std::string_view text) {
  int num_repeats = features::GetOnDeviceModelNumRepeats();
  if (num_repeats <= 1) {
    return false;
  }
  return HasRepeatingSuffix(features::GetOnDeviceModelMinRepeatChars(),
                            num_repeats, text);
}

size_t GetNumTrailingNewlines(std::string_view text) {
  size_t num_trailing_newlines = 0;
  for (const char it : base::Reversed(text)) {
    if (it == '\n') {
      num_trailing_newlines++;
    } else {
      break;
    }
  }
  return num_trailing_newlines;
}

bool CheckTextContainsNonNewline(std::string_view text) {
  return text.find_first_not_of('\n') != std::string::npos;
}

NewlineBuffer::Chunk NewlineBuffer::Append(std::string_view text) {
  NewlineBuffer::Chunk released_chunk{};

  if (CheckTextContainsNonNewline(text)) {
    // If we hit a non-newline character, release the newline buffer
    released_chunk.text = std::string(num_newlines_, '\n');
    released_chunk.num_tokens = num_tokens_;

    num_newlines_ = 0;
    num_tokens_ = 0;
  } else {
    num_tokens_++;
  }

  const auto num_trailing_newlines = GetNumTrailingNewlines(text);
  const auto num_non_newline_chars = text.size() - num_trailing_newlines;
  num_newlines_ += num_trailing_newlines;

  released_chunk.text += text.substr(0, num_non_newline_chars);
  released_chunk.num_tokens += (num_non_newline_chars > 0);

  return released_chunk;
}

}  // namespace optimization_guide
