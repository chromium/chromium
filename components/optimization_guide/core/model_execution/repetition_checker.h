// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REPETITION_CHECKER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REPETITION_CHECKER_H_

#include <string_view>

namespace optimization_guide {

// Returns true if `text` has a suffix which repeats `num_repeats` times with at
// least a length of `min_chars`.
bool HasRepeatingSuffix(int min_chars, int num_repeats, std::string_view text);

// As above, but get min_chars and num_repeats from Feature flags.
bool HasRepeatingSuffix(std::string_view text);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_REPETITION_CHECKER_H_
