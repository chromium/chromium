// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_CONFIGS_SUMMARIZE_SOLUTION_CONFIG_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_CONFIGS_SUMMARIZE_SOLUTION_CONFIG_H_

#include "components/optimization_guide/proto/manifest.pb.h"

namespace optimization_guide {

// Builds a SolutionConfig for the Summarize feature.
proto::SolutionConfig BuildSummarizeSolutionConfig();

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_CONFIGS_SUMMARIZE_SOLUTION_CONFIG_H_
