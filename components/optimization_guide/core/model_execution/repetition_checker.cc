// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/repetition_checker.h"

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

}  // namespace optimization_guide
