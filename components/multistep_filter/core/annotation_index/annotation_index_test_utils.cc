// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/annotation_index_test_utils.h"

#include <string>
#include <vector>

#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "url/gurl.h"

namespace multistep_filter {

ExtractTaskAttributesResponse CreateExtractTaskAttributesResponse(
    const std::string& domain,
    const std::string& task_type,
    const std::vector<std::pair<std::string, std::string>>& attributes) {
  ExtractTaskAttributesResponse response;
  response.set_domain(domain);
  response.set_task_type(task_type);
  for (const auto& attribute : attributes) {
    auto* attr = response.add_task_attributes();
    attr->set_key(attribute.first);
    attr->set_value(attribute.second);
  }
  return response;
}

GetSupportedTasksResponse CreateSupportedTasksResponse(
    const std::vector<std::string>& task_types) {
  GetSupportedTasksResponse response;
  for (const auto& task_type : task_types) {
    auto* task = response.add_supported_tasks();
    task->set_task_type(task_type);
  }
  return response;
}

GetTaskExecutionStrategiesResponse CreateTaskExecutionStrategiesResponse(
    const GURL& suggestion_url,
    const std::vector<std::pair<std::string, std::string>>& attributes) {
  GetTaskExecutionStrategiesResponse response;
  auto* strategy = response.add_execution_strategies();
  for (const auto& attribute : attributes) {
    auto* ui_filter = strategy->add_applied_filters();
    ui_filter->set_key(attribute.first);
    ui_filter->set_label(attribute.second);
  }

  auto* url_nav = new UrlNavigation();
  url_nav->set_navigation_url(suggestion_url.spec());

  auto* execution = new TaskExecution();
  execution->set_allocated_url_navigation(url_nav);
  strategy->set_allocated_execution(execution);
  return response;
}

}  // namespace multistep_filter
