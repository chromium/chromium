// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_EDU_CLASSIFIER_MODEL_HANDLER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_EDU_CLASSIFIER_MODEL_HANDLER_H_

#include <optional>

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/inference/model_handler.h"
#include "components/page_content_annotations/core/edu_classifier_model_executor.h"

namespace page_content_annotations {

// Implements optimization_guide::ModelHandler for the edu classifier model.
class EduClassifierModelHandler : public optimization_guide::ModelHandler<
                                      EduClassifierModelExecutor::ModelOutput,
                                      EduClassifierModelExecutor::ModelInput> {
 public:
  EduClassifierModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner);
  ~EduClassifierModelHandler() override;

  // Disallow copy/assign.
  EduClassifierModelHandler(const EduClassifierModelHandler&) = delete;
  EduClassifierModelHandler& operator=(const EduClassifierModelHandler&) =
      delete;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_EDU_CLASSIFIER_MODEL_HANDLER_H_
