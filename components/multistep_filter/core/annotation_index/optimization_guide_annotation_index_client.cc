// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/optimization_guide_annotation_index_client.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_conversion_util.h"
#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "url/gurl.h"

namespace multistep_filter {

// static
std::unique_ptr<OptimizationGuideAnnotationIndexClient>
OptimizationGuideAnnotationIndexClient::Create(
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    MultistepFilterLogRouter* log_router) {
  return std::make_unique<OptimizationGuideAnnotationIndexClient>(
      optimization_guide_decider, log_router);
}

OptimizationGuideAnnotationIndexClient::OptimizationGuideAnnotationIndexClient(
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    MultistepFilterLogRouter* log_router)
    : optimization_guide_decider_(optimization_guide_decider),
      log_router_(log_router) {
  RegisterOptimizationTypes();
}

OptimizationGuideAnnotationIndexClient::
    ~OptimizationGuideAnnotationIndexClient() = default;

void OptimizationGuideAnnotationIndexClient::GetFilterSuggestionCandidates(
    const GURL& url,
    base::span<const FilterAnnotation> filter_annotations,
    base::OnceCallback<
        void(std::optional<std::vector<FilterSuggestionCandidate>>)> callback,
    int64_t navigation_id) {
  // TODO(crbug.com/522751288): Implement this method.
  std::move(callback).Run(std::nullopt);
}

void OptimizationGuideAnnotationIndexClient::GetSupportedTasks(
    const GURL& url,
    base::OnceCallback<void(std::vector<std::string>)> callback,
    int64_t navigation_id) {
  // TODO(crbug.com/522749876): Implement this method.
  std::move(callback).Run(std::vector<std::string>());
}

void OptimizationGuideAnnotationIndexClient::ExtractFilterAnnotation(
    const GURL& url,
    base::OnceCallback<void(std::optional<FilterAnnotation>)> callback,
    int64_t navigation_id) {
  // TODO(crbug.com/522752340): Implement this method.
  std::move(callback).Run(std::nullopt);
}

void OptimizationGuideAnnotationIndexClient::RegisterOptimizationTypes() {
  // TODO(crbug.com/529713698): Register the optimization types.
}

}  // namespace multistep_filter
