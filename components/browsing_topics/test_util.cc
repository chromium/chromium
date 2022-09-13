// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/test_util.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/history/core/browser/history_service.h"

namespace browsing_topics {

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
    optimization_guide::PageContentAnnotationsService* annotations_service,
    const base::circular_deque<EpochTopics>& epochs,
    CalculateCompletedCallback callback,
    base::queue<uint64_t> rand_uint64_queue)
    : BrowsingTopicsCalculator(privacy_sandbox_settings,
                               history_service,
                               site_data_manager,
                               annotations_service,
                               epochs,
                               std::move(callback)),
      rand_uint64_queue_(std::move(rand_uint64_queue)) {}

TesterBrowsingTopicsCalculator::TesterBrowsingTopicsCalculator(
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    history::HistoryService* history_service,
    content::BrowsingTopicsSiteDataManager* site_data_manager,
    optimization_guide::PageContentAnnotationsService* annotations_service,
    CalculateCompletedCallback callback,
    EpochTopics mock_result,
    base::TimeDelta mock_result_delay)
    : BrowsingTopicsCalculator(privacy_sandbox_settings,
                               history_service,
                               site_data_manager,
                               annotations_service,
                               base::circular_deque<EpochTopics>(),
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
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
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

}  // namespace browsing_topics
