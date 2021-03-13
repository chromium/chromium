// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_service.h"

#include "base/metrics/histogram_macros_local.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/optimization_guide/content/browser/page_content_annotations_model_manager.h"
#endif

namespace optimization_guide {

PageContentAnnotationsService::PageContentAnnotationsService(
    OptimizationGuideDecider* optimization_guide_decider) {
  DCHECK(optimization_guide_decider);
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  model_manager_ = std::make_unique<PageContentAnnotationsModelManager>(
      optimization_guide_decider);
#endif
}

PageContentAnnotationsService::~PageContentAnnotationsService() = default;

void PageContentAnnotationsService::Annotate(const HistoryVisit& visit,
                                             const std::string& text) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  model_manager_->Annotate(
      text,
      base::BindOnce(&PageContentAnnotationsService::OnPageContentAnnotated,
                     weak_ptr_factory_.GetWeakPtr(), visit));
#endif
}

void PageContentAnnotationsService::OnPageContentAnnotated(
    const HistoryVisit& visit,
    const base::Optional<PageContentAnnotations>& page_content_annotations) {
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PageContentAnnotationsService.PageContentAnnotated",
      page_content_annotations.has_value());
  if (page_content_annotations) {
    // TODO(crbug/1177102): Send annotations to store in history service.
  }
}

base::Optional<int64_t>
PageContentAnnotationsService::GetPageTopicsModelVersion() const {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  return model_manager_->GetPageTopicsModelVersion();
#else
  return base::nullopt;
#endif
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
