// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CONVERSION_UTIL_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CONVERSION_UTIL_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"

class GURL;

namespace multistep_filter {

class ExtractTaskAttributesRequest;
class ExtractTaskAttributesResponse;
class ExecutionCandidate;
class GetSupportedTasksRequest;
class GetSupportedTasksResponse;
class GetTaskExecutionStrategiesRequest;
class GetTaskExecutionStrategiesResponse;

struct FilterSuggestionCandidate;
struct FilterAnnotation;

// Converts a domain string into a `GetSupportedTasksRequest` proto.
GetSupportedTasksRequest ToGetSupportedTasksRequest(std::string_view domain);

// Converts a `GetSupportedTasksResponse` proto into a list of supported tasks.
std::vector<std::string> ToSupportedTasks(
    const GetSupportedTasksResponse& response);

// Converts a `FilterAnnotation` into an `ExecutionCandidate` proto.
ExecutionCandidate ToExecutionCandidate(const FilterAnnotation& annotation);

// Converts a URL and a list of `FilterAnnotation`s into a
// `GetTaskExecutionStrategiesRequest` proto.
GetTaskExecutionStrategiesRequest ToGetTaskExecutionStrategiesRequest(
    const GURL& url,
    base::span<const FilterAnnotation> filter_annotations);

// Converts a `GetTaskExecutionStrategiesResponse` proto into a list of
// `FilterSuggestionCandidate` data models.
std::vector<FilterSuggestionCandidate> ToFilterSuggestionCandidates(
    const GetTaskExecutionStrategiesResponse& response);

// Converts a URL into an `ExtractTaskAttributesRequest` proto.
ExtractTaskAttributesRequest ToExtractTaskAttributesRequest(const GURL& url);

// Converts an `ExtractTaskAttributesResponse` proto into a `FilterAnnotation`
// data model.
std::optional<FilterAnnotation> ToFilterAnnotation(
    const ExtractTaskAttributesResponse& response);

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CONVERSION_UTIL_H_
