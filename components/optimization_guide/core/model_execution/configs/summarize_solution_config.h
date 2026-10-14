// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_CONFIGS_SUMMARIZE_SOLUTION_CONFIG_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_CONFIGS_SUMMARIZE_SOLUTION_CONFIG_H_

#include <stdint.h>

#include "components/optimization_guide/proto/manifest.pb.h"

namespace optimization_guide {

// Builds a SolutionConfig for the Summarize feature.
proto::SolutionConfig BuildSummarizeSolutionConfig();

// ML Kit Prompt API accepts inputs under 4000 tokens and allows
// maxOutputTokens up to 4096 according to
// https://developers.google.com/ml-kit/genai/prompt/android/get-started#supported-features.
// On Android, context and execution input are combined into one prompt, and
// these Chromium-side limits are not currently enforced by the AICore backend.
// The Summarizer API exposes this kAndroidMaxExecuteTokens as its
// input quota (context window) instead of using the hardcoded 9216-token
// fallback.
inline constexpr uint32_t kAndroidMaxExecuteTokens = 4000;
inline constexpr uint32_t kAndroidMaxOutputTokens = 4096;

// Builds the Summarize config with Android AICore runtime settings.
proto::SolutionConfig BuildAndroidSummarizeSolutionConfig();

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_CONFIGS_SUMMARIZE_SOLUTION_CONFIG_H_
