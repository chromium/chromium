// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/configs/summarize_solution_config.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

TEST(SummarizeSolutionConfigTest, AndroidUsesAICoreSettings) {
  const auto android = BuildAndroidSummarizeSolutionConfig();
  const auto& feature = android.feature();

  EXPECT_EQ(feature.input_config().min_context_tokens(), 0u);
  EXPECT_EQ(feature.input_config().max_context_tokens(),
            kAndroidMaxExecuteTokens);
  EXPECT_EQ(feature.input_config().max_execute_tokens(),
            kAndroidMaxExecuteTokens);
  EXPECT_EQ(feature.output_config().max_output_tokens(),
            kAndroidMaxOutputTokens);
}

}  // namespace
}  // namespace optimization_guide
