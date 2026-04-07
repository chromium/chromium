// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_TEST_UTILS_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_TEST_UTILS_H_

#include <string>
#include <vector>

#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "url/gurl.h"

namespace multistep_filter {

ExtractTaskAttributesResponse CreateExtractTaskAttributesResponse(
    const std::string& domain,
    const std::string& task_type,
    const std::vector<std::pair<std::string, std::string>>& attributes);

GetSupportedTasksResponse CreateSupportedTasksResponse(
    const std::vector<std::string>& task_types);

GetTaskExecutionStrategiesResponse CreateTaskExecutionStrategiesResponse(
    const GURL& suggestion_url,
    const std::vector<std::pair<std::string, std::string>>& attributes);

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_TEST_UTILS_H_
