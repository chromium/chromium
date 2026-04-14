// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/annotation_index_conversion_util.h"

#include <vector>

#include "base/time/time.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {
namespace {

constexpr char kTestCandidateId[] = "12345678-1234-4678-a234-567812345678";

TEST(AnnotationIndexConversionUtilTest, ToGetSupportedTasksRequest) {
  GetSupportedTasksRequest request = ToGetSupportedTasksRequest("example.com");

  EXPECT_EQ(request.domain(), "example.com");
}

TEST(AnnotationIndexConversionUtilTest, ToSupportedTasks) {
  GetSupportedTasksResponse response;
  response.add_supported_tasks()->set_task_type("TASK1");
  response.add_supported_tasks()->set_task_type("TASK2");

  std::vector<std::string> tasks = ToSupportedTasks(response);

  ASSERT_EQ(tasks.size(), 2u);
  EXPECT_EQ(tasks[0], "TASK1");
  EXPECT_EQ(tasks[1], "TASK2");
}

TEST(AnnotationIndexConversionUtilTest, ToExecutionCandidate) {
  FilterAnnotation annotation(base::Uuid::ParseLowercase(kTestCandidateId),
                              "SEARCH_FLIGHTS", "travel.com", base::Time::Now(),
                              {FilterAttribute("PRICE_MIN", "100"),
                               FilterAttribute("PRICE_MAX", "500")});

  ExecutionCandidate candidate = ToExecutionCandidate(annotation);

  EXPECT_EQ(candidate.candidate_id(), kTestCandidateId);
  EXPECT_EQ(candidate.task_type(), "SEARCH_FLIGHTS");
  ASSERT_EQ(candidate.task_attributes_size(), 2);
  EXPECT_EQ(candidate.task_attributes(0).key(), "PRICE_MIN");
  EXPECT_EQ(candidate.task_attributes(0).value(), "100");
  EXPECT_EQ(candidate.task_attributes(1).key(), "PRICE_MAX");
  EXPECT_EQ(candidate.task_attributes(1).value(), "500");
}

TEST(AnnotationIndexConversionUtilTest, ToGetTaskExecutionStrategiesRequest) {
  FilterAnnotation annotation1(
      base::Uuid::ParseLowercase("11111111-1111-1111-1111-111111111111"),
      "TASK1", "example.com", base::Time::Now(),
      {FilterAttribute("KEY1", "VAL1")});
  FilterAnnotation annotation2(
      base::Uuid::ParseLowercase("22222222-2222-2222-2222-222222222222"),
      "TASK2", "example.com", base::Time::Now(),
      {FilterAttribute("KEY2", "VAL2")});
  std::vector<FilterAnnotation> annotations = {annotation1, annotation2};

  GetTaskExecutionStrategiesRequest request =
      ToGetTaskExecutionStrategiesRequest(GURL("https://example.com/test"),
                                          annotations);

  EXPECT_EQ(request.current_url(), "https://example.com/test");
  ASSERT_EQ(request.candidates_size(), 2);
  EXPECT_EQ(request.candidates(0).candidate_id(),
            "11111111-1111-1111-1111-111111111111");
  EXPECT_EQ(request.candidates(0).task_type(), "TASK1");
  EXPECT_EQ(request.candidates(1).candidate_id(),
            "22222222-2222-2222-2222-222222222222");
  EXPECT_EQ(request.candidates(1).task_type(), "TASK2");
}

TEST(AnnotationIndexConversionUtilTest, ToFilterSuggestionCandidates) {
  GetTaskExecutionStrategiesResponse response;

  TaskExecutionStrategy* strategy = response.add_execution_strategies();
  strategy->set_candidate_id(kTestCandidateId);

  AppliedFilterUIString* filter1 = strategy->add_applied_filters();
  filter1->set_key("PRICE_MIN");
  filter1->set_label("Min Price");

  AppliedFilterUIString* filter2 = strategy->add_applied_filters();
  filter2->set_key("PRICE_MAX");
  filter2->set_label("Max Price");

  strategy->mutable_execution()->mutable_url_navigation()->set_navigation_url(
      "https://travel.com/flights?min=100&max=500");

  TaskExecutionStrategy* invalid_strategy = response.add_execution_strategies();
  invalid_strategy->set_candidate_id("invalid-candidate");
  invalid_strategy->mutable_execution()
      ->mutable_url_navigation()
      ->set_navigation_url("invalid-url");

  std::vector<FilterSuggestionCandidate> candidates =
      ToFilterSuggestionCandidates(response);

  ASSERT_EQ(candidates.size(), 1u);
  const FilterSuggestionCandidate& suggestion = candidates[0];

  EXPECT_EQ(suggestion.filter_annotation_id.AsLowercaseString(),
            kTestCandidateId);
  EXPECT_EQ(suggestion.navigation_url.spec(),
            "https://travel.com/flights?min=100&max=500");
  ASSERT_EQ(suggestion.attributes.size(), 2u);
  EXPECT_EQ(suggestion.attributes[0].key, "PRICE_MIN");
  EXPECT_EQ(suggestion.attributes[0].label, u"Min Price");
  EXPECT_EQ(suggestion.attributes[1].key, "PRICE_MAX");
  EXPECT_EQ(suggestion.attributes[1].label, u"Max Price");
}

TEST(AnnotationIndexConversionUtilTest, ToExtractTaskAttributesRequest) {
  ExtractTaskAttributesRequest request =
      ToExtractTaskAttributesRequest(GURL("https://example.com/path?q=1"));

  EXPECT_EQ(request.source().raw_url(), "https://example.com/path?q=1");
}

TEST(AnnotationIndexConversionUtilTest, ToFilterAnnotation) {
  ExtractTaskAttributesResponse response;
  response.set_domain("example.com");
  response.set_task_type("SEARCH_FLIGHTS");

  TaskAttribute* attr1 = response.add_task_attributes();
  attr1->set_key("PRICE_MIN");
  attr1->set_value("100");

  TaskAttribute* attr2 = response.add_task_attributes();
  attr2->set_key("PRICE_MAX");
  attr2->set_value("500");

  std::optional<FilterAnnotation> annotation = ToFilterAnnotation(response);

  ASSERT_TRUE(annotation.has_value());
  EXPECT_TRUE(annotation->id.is_valid());
  EXPECT_EQ(annotation->task_type, "SEARCH_FLIGHTS");
  EXPECT_EQ(annotation->source_domain, "example.com");
  ASSERT_EQ(annotation->attributes.size(), 2u);
  EXPECT_EQ(annotation->attributes[0].key, "PRICE_MIN");
  EXPECT_EQ(annotation->attributes[0].value, "100");
  EXPECT_EQ(annotation->attributes[1].key, "PRICE_MAX");
  EXPECT_EQ(annotation->attributes[1].value, "500");
}

TEST(AnnotationIndexConversionUtilTest, ToFilterAnnotation_EmptyResponse) {
  ExtractTaskAttributesResponse response;
  std::optional<FilterAnnotation> annotation = ToFilterAnnotation(response);
  EXPECT_FALSE(annotation.has_value());
}

}  // namespace
}  // namespace multistep_filter
