// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_service.h"

#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace optimization_guide {

PageContentAnnotationsService::PageContentAnnotationsService(
    OptimizationGuideDecider* optimization_guide_decider) {
  DCHECK(optimization_guide_decider);
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  page_topics_model_executor_ = std::make_unique<BertModelExecutor>(
      optimization_guide_decider, proto::OPTIMIZATION_TARGET_PAGE_TOPICS,
      /*model_metadata=*/base::nullopt,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
#endif
}

PageContentAnnotationsService::~PageContentAnnotationsService() = default;

void PageContentAnnotationsService::Annotate(const HistoryVisit& visit,
                                             const base::string16& text) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  page_topics_model_executor_->ExecuteModelWithInput(
      base::BindOnce(
          &PageContentAnnotationsService::OnPageTopicsModelExecutionCompleted,
          weak_ptr_factory_.GetWeakPtr(), visit),
      base::UTF16ToUTF8(text));
#endif
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void PageContentAnnotationsService::OnPageTopicsModelExecutionCompleted(
    const HistoryVisit& visit,
    const base::Optional<std::vector<tflite::task::core::Category>>& output) {
  // TODO(crbug/1177102): If success, then populate to history service.
}
#endif

base::Optional<int64_t>
PageContentAnnotationsService::GetPageTopicsModelVersion() const {
  // TODO(crbug/1177102): Extract this from |page_topics_model_executor| if
  /// building with tflite lib.
  return base::nullopt;
}

// static
HistoryVisit PageContentAnnotationsService::CreateHistoryVisitFromWebContents(
    content::WebContents* web_contents) {
  HistoryVisit visit = {
      web_contents->GetController().GetLastCommittedEntry()->GetUniqueID(),
      web_contents->GetLastCommittedURL()};
  return visit;
}

}  // namespace optimization_guide
