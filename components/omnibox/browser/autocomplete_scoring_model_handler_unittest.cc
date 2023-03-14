// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_scoring_model_handler.h"

#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_scoring_model_executor.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/autocomplete_scoring_model_metadata.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

using ScoringSignals = ::metrics::OmniboxEventProto::Suggestion::ScoringSignals;
using ::optimization_guide::proto::AutocompleteScoringModelMetadata;
using ::optimization_guide::proto::ScoringSignalSpec;
using ::optimization_guide::proto::ScoringSignalTransformation;
using ::optimization_guide::proto::ScoringSignalType;

namespace {

ScoringSignalSpec CreateScoringSignalSpec(
    ScoringSignalType type,
    absl::optional<ScoringSignalTransformation> transformation = absl::nullopt,
    absl::optional<float> min_val = absl::nullopt,
    absl::optional<float> max_val = absl::nullopt,
    absl::optional<float> missing_val = absl::nullopt) {
  ScoringSignalSpec spec;
  spec.set_type(type);
  if (transformation) {
    spec.set_transformation(*transformation);
  }
  if (min_val) {
    spec.set_min_value(*min_val);
  }
  if (max_val) {
    spec.set_max_value(*max_val);
  }
  if (missing_val) {
    spec.set_missing_value(*missing_val);
  }
  return spec;
}

}  // namespace

class TestAutocompleteScoringModelExecutor
    : public AutocompleteScoringModelExecutor {
 public:
  TestAutocompleteScoringModelExecutor() = default;
  ~TestAutocompleteScoringModelExecutor() override = default;

  void InitializeAndMoveToExecutionThread(
      absl::optional<base::TimeDelta>,
      optimization_guide::proto::OptimizationTarget,
      scoped_refptr<base::SequencedTaskRunner>,
      scoped_refptr<base::SequencedTaskRunner>) override {}

  void UpdateModelFile(const base::FilePath&) override {}

  void UnloadModel() override {}

  void SetShouldUnloadModelOnComplete(bool should_auto_unload) override {}
};

class AutocompleteScoringModelHandlerTest : public testing::Test {
 public:
  AutocompleteScoringModelHandlerTest() = default;
  ~AutocompleteScoringModelHandlerTest() override = default;

  void SetUp() override {
    model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    model_handler_ = std::make_unique<AutocompleteScoringModelHandler>(
        model_provider_.get(), task_environment_.GetMainThreadTaskRunner(),
        std::make_unique<TestAutocompleteScoringModelExecutor>(),
        /*optimization_target=*/
        optimization_guide::proto::OPTIMIZATION_TARGET_OMNIBOX_URL_SCORING,
        /*model_metadata=*/absl::nullopt);
  }

  void TearDown() override {
    model_handler_.reset();
    model_provider_.reset();
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
  std::unique_ptr<AutocompleteScoringModelHandler> model_handler_;
};

TEST_F(AutocompleteScoringModelHandlerTest,
       ExtractInputFromScoringSignalsTest) {
  // Metadata with three scoring signal specifications.
  AutocompleteScoringModelMetadata model_metadata;
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::SCORING_SIGNAL_TYPE_LENGTH_OF_URL);
  // Signal with log2 transformation.
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::
          SCORING_SIGNAL_TYPE_ELAPSED_TIME_LAST_VISIT_SECS,
      /*transformation=*/optimization_guide::proto::
          SCORING_SIGNAL_TRANSFORMATION_LOG_2);
  // Invalid signal.
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::
          SCORING_SIGNAL_TYPE_ELAPSED_TIME_LAST_SHORTCUT_VISIT_SEC,
      /*transformation=*/absl::nullopt,
      /*min_val=*/0, /*max_val=*/absl::nullopt, /*missing_val=*/-2);

  // Scoring signals.
  ScoringSignals scoring_signals;
  scoring_signals.set_length_of_url(10);
  scoring_signals.set_elapsed_time_last_visit_secs(511);
  scoring_signals.set_elapsed_time_last_shortcut_visit_sec(-200);

  const auto input_signals = model_handler_->ExtractInputFromScoringSignals(
      scoring_signals, model_metadata);
  EXPECT_THAT(input_signals, testing::UnorderedElementsAre(10, 9, -2));
}