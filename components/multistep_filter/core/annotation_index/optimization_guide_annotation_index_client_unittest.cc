// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/optimization_guide_annotation_index_client.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

constexpr int64_t kTestNavigationId = 12345;
constexpr char kTestUrl[] = "https://example.com/test";

class OptimizationGuideAnnotationIndexClientTest : public testing::Test {
 public:
  OptimizationGuideAnnotationIndexClientTest() {
    client_ = std::make_unique<OptimizationGuideAnnotationIndexClient>(
        /*optimization_guide_decider=*/nullptr, /*log_router=*/nullptr);
  }
  ~OptimizationGuideAnnotationIndexClientTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<OptimizationGuideAnnotationIndexClient> client_;
};

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetFilterSuggestionCandidatesTriggersCallback) {
  base::test::TestFuture<std::optional<std::vector<FilterSuggestionCandidate>>>
      future;
  std::vector<FilterAnnotation> annotations;

  client_->GetFilterSuggestionCandidates(
      GURL(kTestUrl), annotations, future.GetCallback(), kTestNavigationId);

  EXPECT_EQ(future.Take(), std::nullopt);
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       GetSupportedTasksTriggersCallback) {
  base::test::TestFuture<std::vector<std::string>> future;

  client_->GetSupportedTasks(GURL(kTestUrl), future.GetCallback(),
                             kTestNavigationId);

  EXPECT_TRUE(future.Take().empty());
}

TEST_F(OptimizationGuideAnnotationIndexClientTest,
       ExtractFilterAnnotationTriggersCallback) {
  base::test::TestFuture<std::optional<FilterAnnotation>> future;

  client_->ExtractFilterAnnotation(GURL(kTestUrl), future.GetCallback(),
                                   kTestNavigationId);

  EXPECT_EQ(future.Take(), std::nullopt);
}

}  // namespace

}  // namespace multistep_filter
