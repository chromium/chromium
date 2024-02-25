// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/bert_model_handler.h"

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/bert_model_executor.h"

namespace optimization_guide {

BertModelHandler::BertModelHandler(
    OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    proto::OptimizationTarget optimization_target,
    const std::optional<proto::Any>& model_metadata)
    : ModelHandler<std::vector<tflite::task::core::Category>,
                   const std::string&>(
          model_provider,
          background_task_runner,
          std::make_unique<BertModelExecutor>(optimization_target),
          /*model_inference_timeout=*/std::nullopt,
          optimization_target,
          model_metadata) {}

BertModelHandler::~BertModelHandler() = default;

}  // namespace optimization_guide
