// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_BERT_MODEL_HANDLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_BERT_MODEL_HANDLER_H_

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/model_handler.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/category.h"

namespace optimization_guide {

// An implementation of a ModelHandler that executes BERT models.
//
// Note that sentencepiece tokenizers are not supported by Chromium's copy of
// the TFLite Support library.
class BertModelHandler
    : public ModelHandler<std::vector<tflite::task::core::Category>,
                          const std::string&> {
 public:
  BertModelHandler(
      OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      proto::OptimizationTarget optimization_target,
      const std::optional<proto::Any>& model_metadata);
  ~BertModelHandler() override;

  BertModelHandler(const BertModelHandler&) = delete;
  BertModelHandler& operator=(const BertModelHandler&) = delete;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_BERT_MODEL_HANDLER_H_
