// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_MODEL_HANDLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_MODEL_HANDLER_H_

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/test_model_executor.h"

namespace optimization_guide {

class TestModelHandler
    : public ModelHandler<std::vector<float>, const std::vector<float>&> {
 public:
  TestModelHandler(
      OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      std::unique_ptr<TestModelExecutor> executor =
          std::make_unique<TestModelExecutor>())
      : ModelHandler<std::vector<float>, const std::vector<float>&>(
            model_provider,
            background_task_runner,
            std::move(executor),
            /*model_inference_timeout=*/std::nullopt,
            proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
            /*model_metadata=*/std::nullopt) {}
  ~TestModelHandler() override = default;
  TestModelHandler(const TestModelHandler&) = delete;
  TestModelHandler& operator=(const TestModelHandler&) = delete;

  // There is a method on the base class that exposes the returned supported
  // features, if provided by the loaded model received from the server.
  // std::optional<T> ParsedSupportedFeaturesForLoadedModel();
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_MODEL_HANDLER_H_
