// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_TEST_UTILS_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_TEST_UTILS_H_

#include <string>
#include <vector>

#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "url/gurl.h"

namespace optimization_guide {
class OptimizationMetadata;
}  // namespace optimization_guide

namespace multistep_filter {

ExtractTaskAttributesResponse CreateExtractTaskAttributesResponse(
    const std::string& task_type,
    const std::vector<std::pair<std::string, std::string>>& attributes);

GetSupportedTasksResponse CreateSupportedTasksResponse(
    const std::vector<std::string>& task_types);

GetTaskExecutionStrategiesResponse CreateTaskExecutionStrategiesResponse(
    const GURL& suggestion_url,
    const std::vector<std::pair<std::string, std::string>>& attributes);

optimization_guide::OptimizationMetadata CreateOptimizationMetadata(
    const optimization_guide::proto::Any& any_metadata);

optimization_guide::OptimizationGuideDecisionWithMetadata
CreateDecisionWithMetadata(
    const optimization_guide::OptimizationGuideDecision& decision,
    const optimization_guide::OptimizationMetadata& metadata);

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_TEST_UTILS_H_
