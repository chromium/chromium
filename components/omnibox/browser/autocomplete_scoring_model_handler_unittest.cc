// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_scoring_model_handler.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_scoring_model_executor.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/autocomplete_scoring_model_metadata.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_scoring_signals.pb.h"

using ScoringSignals = ::metrics::OmniboxScoringSignals;
using ::optimization_guide::proto::AutocompleteScoringModelMetadata;
using ::optimization_guide::proto::ScoringSignalSpec;
using ::optimization_guide::proto::ScoringSignalTransformation;
using ::optimization_guide::proto::ScoringSignalType;

namespace {

ScoringSignalSpec CreateScoringSignalSpec(
    ScoringSignalType type,
    std::optional<ScoringSignalTransformation> transformation = std::nullopt,
    std::optional<float> min_val = std::nullopt,
    std::optional<float> max_val = std::nullopt,
    std::optional<float> missing_val = std::nullopt) {
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
      std::optional<base::TimeDelta>,
      optimization_guide::proto::OptimizationTarget,
      scoped_refptr<base::SequencedTaskRunner>,
      scoped_refptr<base::SequencedTaskRunner>) override {}

  void UpdateModelFile(base::optional_ref<const base::FilePath>) override {}

  void UnloadModel() override {}

  // These interfere with the test code which is injecting its own model.
  void SetShouldUnloadModelOnComplete(bool should_auto_unload) override {}
  void SetShouldPreloadModel(bool should_preload_model) override {}
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
        /*model_metadata=*/std::nullopt);

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    // A model of `add` operator.
    model_file_path_ = source_root_dir.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("omnibox")
                           .AppendASCII("adder.tflite");
  }

  void TearDown() override {
    model_handler_.reset();
    model_provider_.reset();
    RunUntilIdle();
  }

  void PushModelFileToModelExecutor(
      std::optional<optimization_guide::proto::AutocompleteScoringModelMetadata>
          metadata) {
    std::optional<optimization_guide::proto::Any> any;

    // Craft a correct Any proto in the case we passed in metadata.
    if (metadata) {
      std::string serialized_metadata;
      metadata->SerializeToString(&serialized_metadata);
      optimization_guide::proto::Any any_proto;
      any = std::make_optional(any_proto);
      any->set_value(serialized_metadata);
      any->set_type_url(
          "type.googleapis.com/"
          "optimization_guide.protos.AutocompleteScoringModelMetadata");
    }

    auto model_metadata = optimization_guide::TestModelInfoBuilder()
                              .SetModelMetadata(any)
                              .SetModelFilePath(model_file_path_)
                              .SetVersion(123)
                              .Build();
    model_handler_->OnModelUpdated(
        optimization_guide::proto::OPTIMIZATION_TARGET_OMNIBOX_URL_SCORING,
        *model_metadata);
    task_environment_.RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
  std::unique_ptr<AutocompleteScoringModelHandler> model_handler_;
  base::FilePath model_file_path_;
};

TEST_F(AutocompleteScoringModelHandlerTest,
       ExtractInputFromScoringSignalsTest) {
  // Metadata with scoring signal specifications.
  AutocompleteScoringModelMetadata model_metadata;
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::SCORING_SIGNAL_TYPE_LENGTH_OF_URL);
  model_metadata.mutable_scoring_signal_specs(0)->set_norm_upper_boundary(50);
  // Signal with log2 transformation.
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::
          SCORING_SIGNAL_TYPE_ELAPSED_TIME_LAST_VISIT_SECS,
      /*transformation=*/optimization_guide::proto::
          SCORING_SIGNAL_TRANSFORMATION_LOG_2);
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::
          SCORING_SIGNAL_TYPE_ELAPSED_TIME_LAST_VISIT_DAYS);
  // Invalid signal.
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::
          SCORING_SIGNAL_TYPE_ELAPSED_TIME_LAST_SHORTCUT_VISIT_SEC,
      /*transformation=*/std::nullopt,
      /*min_val=*/0, /*max_val=*/std::nullopt, /*missing_val=*/-2);
  // Clamped by upper boundary.
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::SCORING_SIGNAL_TYPE_TYPED_COUNT);
  model_metadata.mutable_scoring_signal_specs(4)->set_norm_upper_boundary(100);
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::
          SCORING_SIGNAL_TYPE_MATCHES_TITLE_OR_HOST_OR_SHORTCUT_TEXT);
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::
          SCORING_SIGNAL_TYPE_NUM_INPUT_TERMS_MATCHED_BY_BOOKMARK_TITLE);
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::SCORING_SIGNAL_TYPE_SITE_ENGAGEMENT);
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::SCORING_SIGNAL_TYPE_SEARCH_SUGGEST_RELEVANCE);
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::SCORING_SIGNAL_TYPE_IS_SEARCH_SUGGEST_ENTITY);
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::SCORING_SIGNAL_TYPE_IS_VERBATIM);
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::SCORING_SIGNAL_TYPE_IS_NAVSUGGEST);
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::SCORING_SIGNAL_TYPE_IS_SEARCH_SUGGEST_TAIL);
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::SCORING_SIGNAL_TYPE_IS_ANSWER_SUGGEST);
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::SCORING_SIGNAL_TYPE_IS_CALCULATOR_SUGGEST);

  // Scoring signals.
  ScoringSignals scoring_signals;
  scoring_signals.set_length_of_url(10);
  scoring_signals.set_elapsed_time_last_visit_secs(32767);
  scoring_signals.set_elapsed_time_last_shortcut_visit_sec(-200);
  scoring_signals.set_typed_count(150);
  scoring_signals.set_num_input_terms_matched_by_bookmark_title(10);
  scoring_signals.set_site_engagement(50);
  scoring_signals.set_search_suggest_relevance(1234);
  scoring_signals.set_is_search_suggest_entity(true);
  scoring_signals.set_is_verbatim(false);
  scoring_signals.set_is_navsuggest(false);
  scoring_signals.set_is_search_suggest_tail(false);
  scoring_signals.set_is_answer_suggest(false);
  scoring_signals.set_is_calculator_suggest(false);

  auto input_signals = model_handler_->ExtractInputFromScoringSignals(
      scoring_signals, model_metadata);
  ASSERT_EQ(input_signals.size(), 15u);
  EXPECT_THAT(input_signals[0], 0.2);  // Normalized signal.
  EXPECT_THAT(input_signals[1], 15);
  EXPECT_NEAR(input_signals[2], 0.3792, 0.0001);
  EXPECT_THAT(input_signals[3], -2);
  EXPECT_NEAR(input_signals[4], 1.0f, 0.0001);  // Clamped and normalized.

  // `matches_title_or_host_or_shortcut_text` is derived from host or title
  // match length and shortcut visit count. Expect it to be false until those
  // values are set.
  EXPECT_THAT(input_signals[5], 0);

  scoring_signals.set_total_host_match_length(20);
  scoring_signals.set_total_title_match_length(0);
  scoring_signals.set_shortcut_visit_count(1);
  input_signals = model_handler_->ExtractInputFromScoringSignals(
      scoring_signals, model_metadata);
  EXPECT_THAT(input_signals[5], 1);

  EXPECT_THAT(input_signals[6], 10);
  EXPECT_THAT(input_signals[7], 50);
  EXPECT_THAT(input_signals[8], 1234);
  EXPECT_THAT(input_signals[9], 1);
  EXPECT_THAT(input_signals[10], 0);
  EXPECT_THAT(input_signals[11], 0);
  EXPECT_THAT(input_signals[12], 0);
  EXPECT_THAT(input_signals[13], 0);
  EXPECT_THAT(input_signals[14], 0);
}

TEST_F(AutocompleteScoringModelHandlerTest, GetBatchModelInputTest) {
  AutocompleteScoringModelMetadata model_metadata;
  *model_metadata.add_scoring_signal_specs() = CreateScoringSignalSpec(
      optimization_guide::proto::SCORING_SIGNAL_TYPE_LENGTH_OF_URL);
  PushModelFileToModelExecutor(model_metadata);

  std::vector<const ScoringSignals*> scoring_signals_vec;
  // Scoring signals.
  ScoringSignals scoring_signals_1, scoring_signals_2;
  scoring_signals_1.set_length_of_url(10);
  scoring_signals_vec.push_back(&scoring_signals_1);
  scoring_signals_2.set_length_of_url(12);
  scoring_signals_vec.push_back(&scoring_signals_2);
  const std::optional<std::vector<std::vector<float>>> batch_model_input =
      model_handler_->GetBatchModelInput(scoring_signals_vec);
  ASSERT_TRUE(batch_model_input);
  ASSERT_EQ(batch_model_input->size(), 2u);
  EXPECT_THAT(batch_model_input->at(0), testing::UnorderedElementsAre(10));
  EXPECT_THAT(batch_model_input->at(1), testing::UnorderedElementsAre(12));
}
