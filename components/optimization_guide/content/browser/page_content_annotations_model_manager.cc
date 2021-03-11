// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_model_manager.h"

#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"

namespace optimization_guide {

namespace {

const char kPageTopicsModelMetadataTypeUrl[] =
    "type.googleapis.com/"
    "google.internal.chrome.optimizationguide.v1.PageTopicsModelMetadata";

}  // namespace

PageContentAnnotationsModelManager::PageContentAnnotationsModelManager(
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider) {
  proto::Any model_metadata;
  model_metadata.set_type_url(kPageTopicsModelMetadataTypeUrl);
  proto::PageTopicsModelMetadata page_topics_model_metadata;
  page_topics_model_metadata.add_supported_output(
      proto::PAGE_TOPICS_SUPPORTED_OUTPUT_SENSITIVITY);
  page_topics_model_metadata.add_supported_output(
      proto::PAGE_TOPICS_SUPPORTED_OUTPUT_CATEGORIES);
  page_topics_model_metadata.SerializeToString(model_metadata.mutable_value());

  page_topics_model_executor_ = std::make_unique<BertModelExecutor>(
      optimization_guide_decider, proto::OPTIMIZATION_TARGET_PAGE_TOPICS,
      model_metadata,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
}

PageContentAnnotationsModelManager::~PageContentAnnotationsModelManager() =
    default;

void PageContentAnnotationsModelManager::Annotate(
    const std::string& text,
    PageContentAnnotatedCallback callback) {
  // TODO(crbug/1177102): Figure out if we want to enqueue it for later if model
  // isn't ready, but if we call this when the model isn't ready, it will just
  // return base::nullopt for now.
  page_topics_model_executor_->ExecuteModelWithInput(
      base::BindOnce(&PageContentAnnotationsModelManager::
                         OnPageTopicsModelExecutionCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      text);
}

void PageContentAnnotationsModelManager::OnPageTopicsModelExecutionCompleted(
    PageContentAnnotatedCallback callback,
    const base::Optional<std::vector<tflite::task::core::Category>>& output) {
  base::Optional<PageContentAnnotations> page_content_annotations;
  if (output) {
    // TODO(crbug/1177102): Postprocess output.
    page_content_annotations =
        PageContentAnnotations(/*categories=*/{}, /*sensitivity=*/0.0);
  }
  std::move(callback).Run(page_content_annotations);
}

base::Optional<int64_t>
PageContentAnnotationsModelManager::GetPageTopicsModelVersion() const {
  base::Optional<proto::PageTopicsModelMetadata> model_metadata =
      page_topics_model_executor_->ParsedSupportedFeaturesForLoadedModel<
          proto::PageTopicsModelMetadata>();
  if (model_metadata)
    return model_metadata->version();
  return base::nullopt;
}

}  // namespace optimization_guide
