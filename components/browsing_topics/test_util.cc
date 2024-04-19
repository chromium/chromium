// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/test_util.h"

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "components/history/core/browser/history_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace browsing_topics {

std::vector<ApiResultUkmMetrics> ReadApiResultUkmMetrics(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  using Event = ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult2;

  std::vector<ApiResultUkmMetrics> result;

  auto entries = ukm_recorder.GetEntriesByName(Event::kEntryName);

  for (const ukm::mojom::UkmEntry* entry : entries) {
    std::vector<CandidateTopic> topics;

    const int64_t* topic0_metric =
        ukm_recorder.GetEntryMetric(entry, Event::kCandidateTopic0Name);
    const int64_t* topic0_is_true_topic_metric = ukm_recorder.GetEntryMetric(
        entry, Event::kCandidateTopic0IsTrueTopTopicName);
    const int64_t* topic0_should_be_filtered_metric =
        ukm_recorder.GetEntryMetric(
            entry, Event::kCandidateTopic0ShouldBeFilteredName);
    const int64_t* topic0_taxonomy_version_metric = ukm_recorder.GetEntryMetric(
        entry, Event::kCandidateTopic0TaxonomyVersionName);
    const int64_t* topic0_model_version_metric = ukm_recorder.GetEntryMetric(
        entry, Event::kCandidateTopic0ModelVersionName);

    if (topic0_metric) {
      topics.emplace_back(CandidateTopic::Create(
          Topic(*topic0_metric), *topic0_is_true_topic_metric,
          *topic0_should_be_filtered_metric, /*config_version=*/0,
          *topic0_taxonomy_version_metric, *topic0_model_version_metric));

      DCHECK(topic0_is_true_topic_metric);
      DCHECK(topic0_should_be_filtered_metric);
      DCHECK(topic0_taxonomy_version_metric);
      DCHECK(topic0_model_version_metric);
    } else {
      topics.emplace_back(CandidateTopic::CreateInvalid());

      DCHECK(!topic0_is_true_topic_metric);
      DCHECK(!topic0_should_be_filtered_metric);
      DCHECK(!topic0_taxonomy_version_metric);
      DCHECK(!topic0_model_version_metric);
    }

    const int64_t* topic1_metric =
        ukm_recorder.GetEntryMetric(entry, Event::kCandidateTopic1Name);
    const int64_t* topic1_is_true_topic_metric = ukm_recorder.GetEntryMetric(
        entry, Event::kCandidateTopic1IsTrueTopTopicName);
    const int64_t* topic1_should_be_filtered_metric =
        ukm_recorder.GetEntryMetric(
            entry, Event::kCandidateTopic1ShouldBeFilteredName);
    const int64_t* topic1_taxonomy_version_metric = ukm_recorder.GetEntryMetric(
        entry, Event::kCandidateTopic1TaxonomyVersionName);
    const int64_t* topic1_model_version_metric = ukm_recorder.GetEntryMetric(
        entry, Event::kCandidateTopic1ModelVersionName);

    if (topic1_metric) {
      topics.emplace_back(CandidateTopic::Create(
          Topic(*topic1_metric), *topic1_is_true_topic_metric,
          *topic1_should_be_filtered_metric, /*config_version=*/0,
          *topic1_taxonomy_version_metric, *topic1_model_version_metric));

      DCHECK(topic1_is_true_topic_metric);
      DCHECK(topic1_should_be_filtered_metric);
      DCHECK(topic1_taxonomy_version_metric);
      DCHECK(topic1_model_version_metric);
    } else {
      topics.emplace_back(CandidateTopic::CreateInvalid());

      DCHECK(!topic1_is_true_topic_metric);
      DCHECK(!topic1_should_be_filtered_metric);
      DCHECK(!topic1_taxonomy_version_metric);
      DCHECK(!topic1_model_version_metric);
    }

    const int64_t* topic2_metric =
        ukm_recorder.GetEntryMetric(entry, Event::kCandidateTopic2Name);
    const int64_t* topic2_is_true_topic_metric = ukm_recorder.GetEntryMetric(
        entry, Event::kCandidateTopic2IsTrueTopTopicName);
    const int64_t* topic2_should_be_filtered_metric =
        ukm_recorder.GetEntryMetric(
            entry, Event::kCandidateTopic2ShouldBeFilteredName);
    const int64_t* topic2_taxonomy_version_metric = ukm_recorder.GetEntryMetric(
        entry, Event::kCandidateTopic2TaxonomyVersionName);
    const int64_t* topic2_model_version_metric = ukm_recorder.GetEntryMetric(
        entry, Event::kCandidateTopic2ModelVersionName);

    if (topic2_metric) {
      topics.emplace_back(CandidateTopic::Create(
          Topic(*topic2_metric), *topic2_is_true_topic_metric,
          *topic2_should_be_filtered_metric, /*config_version=*/0,
          *topic2_taxonomy_version_metric, *topic2_model_version_metric));

      DCHECK(topic2_is_true_topic_metric);
      DCHECK(topic2_should_be_filtered_metric);
      DCHECK(topic2_taxonomy_version_metric);
      DCHECK(topic2_model_version_metric);
    } else {
      topics.emplace_back(CandidateTopic::CreateInvalid());

      DCHECK(!topic2_is_true_topic_metric);
      DCHECK(!topic2_should_be_filtered_metric);
      DCHECK(!topic2_taxonomy_version_metric);
      DCHECK(!topic2_model_version_metric);
    }

    DCHECK_EQ(topics.size(), 3u);

    std::optional<ApiAccessResult> failure_reason;

    const int64_t* failure_reason_metric =
        ukm_recorder.GetEntryMetric(entry, Event::kFailureReasonName);

    if (failure_reason_metric) {
      failure_reason = static_cast<ApiAccessResult>(*failure_reason_metric);
    }

    result.emplace_back(std::move(failure_reason), std::move(topics[0]),
                        std::move(topics[1]), std::move(topics[2]));
  }

  return result;
}

bool BrowsingTopicsEligibleForURLVisit(history::HistoryService* history_service,
                                       const GURL& url) {
  bool topics_eligible;

  history::QueryOptions options;
  options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;

  base::RunLoop run_loop;
  base::CancelableTaskTracker tracker;

  history_service->QueryHistory(
      std::u16string(), options,
      base::BindLambdaForTesting([&](history::QueryResults results) {
        size_t num_matches = 0;
        const size_t* match_index = results.MatchesForURL(url, &num_matches);

        DCHECK_EQ(1u, num_matches);

        topics_eligible =
            results[*match_index].content_annotations().annotation_flags &
            history::VisitContentAnnotationFlag::kBrowsingTopicsEligible;
        run_loop.Quit();
      }),
      &tracker);

  run_loop.Run();

  return topics_eligible;
}

TesterBrowsingTopicsCalculator::TesterBrowsingTopicsCalculator(
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    history::HistoryService* history_service,
    content::BrowsingTopicsSiteDataManager* site_data_manager,
    Annotator* annotator,
    int previous_timeout_count,
    base::Time session_start_time,
    const base::circular_deque<EpochTopics>& epochs,
    CalculateCompletedCallback callback,
    base::queue<uint64_t> rand_uint64_queue)
    : BrowsingTopicsCalculator(privacy_sandbox_settings,
                               history_service,
                               site_data_manager,
                               annotator,
                               epochs,
                               /*is_manually_triggered=*/false,
                               previous_timeout_count,
                               session_start_time,
                               std::move(callback)),
      rand_uint64_queue_(std::move(rand_uint64_queue)) {}

TesterBrowsingTopicsCalculator::TesterBrowsingTopicsCalculator(
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    history::HistoryService* history_service,
    content::BrowsingTopicsSiteDataManager* site_data_manager,
    Annotator* annotator,
    int previous_timeout_count,
    base::Time session_start_time,
    CalculateCompletedCallback callback,
    EpochTopics mock_result,
    base::TimeDelta mock_result_delay)
    : BrowsingTopicsCalculator(privacy_sandbox_settings,
                               history_service,
                               site_data_manager,
                               annotator,
                               base::circular_deque<EpochTopics>(),
                               /*is_manually_triggered=*/false,
                               previous_timeout_count,
                               session_start_time,
                               base::DoNothing()),
      use_mock_result_(true),
      mock_result_(std::move(mock_result)),
      mock_result_delay_(mock_result_delay),
      finish_callback_(std::move(callback)) {}

TesterBrowsingTopicsCalculator::~TesterBrowsingTopicsCalculator() = default;

uint64_t TesterBrowsingTopicsCalculator::GenerateRandUint64() {
  DCHECK(!rand_uint64_queue_.empty());

  uint64_t next_rand_uint64 = rand_uint64_queue_.front();
  rand_uint64_queue_.pop();

  return next_rand_uint64;
}

void TesterBrowsingTopicsCalculator::CheckCanCalculate() {
  if (use_mock_result_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TesterBrowsingTopicsCalculator::MockDelayReached,
                       weak_ptr_factory_.GetWeakPtr()),
        mock_result_delay_);
    return;
  }

  BrowsingTopicsCalculator::CheckCanCalculate();
}

void TesterBrowsingTopicsCalculator::MockDelayReached() {
  DCHECK(use_mock_result_);

  std::move(finish_callback_).Run(std::move(mock_result_));
}

MockBrowsingTopicsService::MockBrowsingTopicsService() = default;
MockBrowsingTopicsService::~MockBrowsingTopicsService() = default;

TestAnnotator::TestAnnotator() = default;
TestAnnotator::~TestAnnotator() = default;

void TestAnnotator::UseAnnotations(
    const std::map<std::string, std::set<int32_t>>& annotations) {
  annotations_ = annotations;
}

void TestAnnotator::UseModelInfo(
    const std::optional<optimization_guide::ModelInfo>& model_info) {
  model_info_ = model_info;
}

void TestAnnotator::SetModelAvailable(bool model_available) {
  model_available_ = model_available;
  if (model_available_) {
    model_available_callbacks_.Notify();
  }
}

void TestAnnotator::BatchAnnotate(BatchAnnotationCallback callback,
                                  const std::vector<std::string>& inputs) {
  auto run_callback_after_delay = base::BindLambdaForTesting(
      [callback = std::move(callback),
       this](const std::vector<Annotation>& result) mutable {
        std::vector<Annotation> copied_result = result;

        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindLambdaForTesting(
                [callback = std::move(callback),
                 copied_result = std::move(copied_result)]() mutable {
                  std::move(callback).Run(copied_result);
                }),
            annotation_request_delay_);
      });

  std::vector<Annotation> annotations;
  annotations.reserve(inputs.size());
  for (const std::string& input : inputs) {
    Annotation annotation(input);
    auto iter = annotations_.find(input);
    if (iter != annotations_.end()) {
      annotation.topics =
          std::vector<int32_t>{iter->second.begin(), iter->second.end()};
    }
    annotations.push_back(annotation);
  }
  std::move(std::move(run_callback_after_delay)).Run(annotations);
}

void TestAnnotator::NotifyWhenModelAvailable(base::OnceClosure callback) {
  auto run_callback_after_delay = base::BindLambdaForTesting(
      [callback = std::move(callback), this]() mutable {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindLambdaForTesting(
                [callback = std::move(callback)]() mutable {
                  std::move(callback).Run();
                }),
            model_request_delay_);
      });

  if (!model_available_) {
    model_available_callbacks_.AddUnsafe(std::move(run_callback_after_delay));
    return;
  }
  std::move(run_callback_after_delay).Run();
}

std::optional<optimization_guide::ModelInfo>
TestAnnotator::GetBrowsingTopicsModelInfo() const {
  return model_info_;
}

}  // namespace browsing_topics
