// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/test_model_executor.h"

namespace optimization_guide {

void TestModelExecutor::SendForExecution(
    ExecutionCallback callback_on_complete,
    base::TimeTicks start_time,
    const std::vector<float>& args) {
  std::vector<float> results = std::vector<float>();
  for (auto& arg : args)
    results.push_back(arg);
  std::move(callback_on_complete).Run(std::move(results));
}

}  // namespace optimization_guide
