// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/annotation_index_conversion_util.h"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/optimization_guide/proto/filter_execution_metadata.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "url/gurl.h"

namespace multistep_filter {

using ::optimization_guide::proto::FilterExecutionRequestContextMetadata;
using ::optimization_guide::proto::RequestContextMetadata;

GetSupportedTasksRequest ToGetSupportedTasksRequest(std::string_view domain) {
  GetSupportedTasksRequest request;
  request.set_domain(domain);
  return request;
}

std::vector<std::string> ToSupportedTasks(
    const GetSupportedTasksResponse& response) {
  std::vector<std::string> tasks;
  for (const SupportedTask& task : response.supported_tasks()) {
    tasks.push_back(task.task_type());
  }
  return tasks;
}

ExecutionCandidate ToExecutionCandidate(const FilterAnnotation& annotation) {
  ExecutionCandidate candidate;
  candidate.set_candidate_id(annotation.id.AsLowercaseString());
  candidate.set_task_type(annotation.task_type);

  for (const FilterAttribute& attr : annotation.attributes) {
    TaskAttribute* task_attr = candidate.add_task_attributes();
    task_attr->set_key(attr.key);
    task_attr->set_value(attr.value);
  }

  return candidate;
}

GetTaskExecutionStrategiesRequest ToGetTaskExecutionStrategiesRequest(
    const GURL& url,
    base::span<const FilterAnnotation> filter_annotations) {
  GetTaskExecutionStrategiesRequest request;
  request.set_current_url(url.spec());
  for (const FilterAnnotation& annotation : filter_annotations) {
    *request.add_candidates() = ToExecutionCandidate(annotation);
  }
  return request;
}

RequestContextMetadata ToRequestContextMetadata(
    base::span<const FilterAnnotation> filter_annotations) {
  RequestContextMetadata metadata;
  FilterExecutionRequestContextMetadata filter_execution_metadata;
  for (const FilterAnnotation& annotation : filter_annotations) {
    *filter_execution_metadata.add_execution_candidate() =
        ToExecutionCandidate(annotation);
  }
  *metadata.mutable_filter_execution_metadata() = filter_execution_metadata;
  return metadata;
}

std::vector<FilterSuggestionCandidate> ToFilterSuggestionCandidates(
    const GetTaskExecutionStrategiesResponse& response) {
  std::vector<FilterSuggestionCandidate> candidates;

  for (const TaskExecutionStrategy& strategy :
       response.execution_strategies()) {
    std::vector<FilterSuggestionCandidateAttribute> attributes;
    for (const AppliedFilterUIString& filter : strategy.applied_filters()) {
      attributes.emplace_back(filter.key(), base::UTF8ToUTF16(filter.label()));
    }

    GURL navigation_url;
    if (strategy.has_execution() && strategy.execution().has_url_navigation()) {
      navigation_url =
          GURL(strategy.execution().url_navigation().navigation_url());
    }

    if (!navigation_url.is_valid()) {
      continue;
    }

    base::Uuid annotation_id =
        base::Uuid::ParseLowercase(strategy.candidate_id());
    if (!annotation_id.is_valid()) {
      continue;
    }

    std::u16string short_text;
    std::u16string detailed_text;
    if (strategy.has_suggestion_message()) {
      short_text =
          base::UTF8ToUTF16(strategy.suggestion_message().short_text());
      detailed_text =
          base::UTF8ToUTF16(strategy.suggestion_message().detailed_text());
    }

    candidates.emplace_back(std::move(annotation_id), std::move(navigation_url),
                            std::move(attributes), std::move(short_text),
                            std::move(detailed_text));
  }

  return candidates;
}

ExtractTaskAttributesRequest ToExtractTaskAttributesRequest(const GURL& url) {
  ExtractTaskAttributesRequest request;
  request.mutable_source()->set_raw_url(url.spec());
  return request;
}

std::optional<FilterAnnotation> ToFilterAnnotation(
    const GURL& url,
    const ExtractTaskAttributesResponse& response) {
  const std::string host(url.host());
  if (host.empty() || response.task_type().empty() ||
      response.task_attributes().empty()) {
    return std::nullopt;
  }

  std::vector<FilterAttribute> attributes;
  for (const TaskAttribute& attr : response.task_attributes()) {
    attributes.emplace_back(attr.key(), attr.value());
  }

  return FilterAnnotation(base::Uuid::GenerateRandomV4(), response.task_type(),
                          host, base::Time::Now(), std::move(attributes));
}

}  // namespace multistep_filter
