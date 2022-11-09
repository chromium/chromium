// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_controller_metrics.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/fake_autocomplete_controller.h"
#include "components/omnibox/browser/fake_autocomplete_provider.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/fake_tab_matcher.h"
#include "components/omnibox/browser/tab_matcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/gurl.h"

using testing::ElementsAre;
using testing::ElementsAreArray;

class AutocompleteControllerMetricsTest : public testing::Test {
 public:
  AutocompleteControllerMetricsTest()
      : metrics_(controller_.metrics_),
        histogram_tester_(std::make_unique<base::HistogramTester>()) {}

  // By default, the controller wants async matches. `SetInputSync()` will
  // explicitly set this behavior.
  void SetInputSync(bool sync) {
    controller_.input_.set_omit_asynchronous_matches(sync);
  }

  // Mimics `AutocompleteController::Start()`'s behavior. `sync_results_only`
  // determines whether the controller should be done after the sync pass.
  // `sync_milliseconds` is how many seconds the sync pass should take.
  // `old_results` and `sync_results` are passed to
  // `AutocompleteControllerMetrics::OnUpdateResult()` to determine which
  // matches changed.
  void SimulateStart(
      bool sync_results_only,
      int sync_milliseconds,
      std::vector<AutocompleteResult::MatchDedupComparator> old_results,
      std::vector<AutocompleteResult::MatchDedupComparator> sync_results) {
    metrics_->OnStart();
    controller_.in_start_ = true;
    controller_.done_ = sync_results_only;
    task_environment_.FastForwardBy(base::Milliseconds(sync_milliseconds));
    metrics_->OnNotifyChanged(old_results, sync_results);
    controller_.in_start_ = false;
  }

  // Convenience function to be called before/after EXPECT'ing histograms to
  // avoid old logs converting new logs.
  void ResetHistogramTester() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  // Convenience function to call AutocompleteControllerMetrics::OnStop()` and
  // expects it to not log any suggestion finalization metrics. Should be used
  // to simulate the controller completing in which case metrics should have
  // already been logged earlier during `::OnUpdateResult()`. Should not be used
  // when the controller is interrupted in which case metrics are expected to be
  // logged. Does not check provider or cross stability metrics.
  void StopAndExpectNoSuggestionFinalizationMetrics() {
    ResetHistogramTester();
    metrics_->OnStop();
    ExpectNoSuggestionFinalizationMetrics();
  }

  // Convenience method to check the buckets of a single metric.
  void ExpectMetrics(const std::string metric_name,
                     std::vector<base::Bucket> expected_buckets) {
    const std::string prefix = "Omnibox.AsyncAutocompletionTime2.";
    EXPECT_THAT(
        histogram_tester_->GetAllSamples(prefix + metric_name),
        ElementsAreArray(expected_buckets.data(), expected_buckets.size()))
        << metric_name;
  }

  // Convenience method to check the buckets of 3 metrics:
  // - '...<metric>' will be expected to have `buckets`.
  // - '...<metric>.[Completed]' will be expected to either have
  //   `buckets` or be empty, depending on `completed`.
  void ExpectSlicedMetrics(const std::string& metric,
                           std::vector<base::Bucket> buckets,
                           bool completed) {
    const std::vector<base::Bucket> empty_buckets = {};
    ExpectMetrics(metric, buckets);
    ExpectMetrics(metric + ".Completed", completed ? buckets : empty_buckets);
  }

  // Convenience method to check all 9 suggestion finalization metrics are
  // empty. '...[Done|LastChange|LastDefaultChange][|.Completed]'
  void ExpectNoSuggestionFinalizationMetrics() {
    ExpectSlicedMetrics("Done", {}, false);
    ExpectSlicedMetrics("LastChange", {}, false);
    ExpectSlicedMetrics("LastDefaultChange", {}, false);
  }

  // Convince method to check all 9 suggestion finalization metrics have been
  // logged exactly once with the correct bucket. This should be used if
  // there's been a single controller request logged since the last
  // `ResetHistogramTester()`. If there have been none,
  // `ExpectNoSuggestionFinalizationMetrics()` should be used.
  void ExpectSingleCountSuggestionFinalizationMetrics(
      int done_bucket_min,
      int last_change_bucket_min,
      int last_default_change_bucket_min,
      bool completed) {
    ExpectSlicedMetrics("Done", {{done_bucket_min, 1}}, completed);
    ExpectSlicedMetrics("LastChange", {{last_change_bucket_min, 1}}, completed);
    ExpectSlicedMetrics("LastDefaultChange",
                        {{last_default_change_bucket_min, 1}}, completed);
  }

  // Convenience method to check the 3 metrics for a specific provider are
  // empty. '...Provider.<Provider>[|.Completed]'
  void ExpectNoProviderMetrics(const std::string& provider) {
    ExpectSlicedMetrics("Provider." + provider, {}, false);
  }

  // Convenience method to check the 3 metrics for a specific provider have
  // been logged exactly once with the correct bucket. This should be used if
  // there's been a single log for the specified provider since the last
  // `ResetHistogramTester()`. If there have been none,
  // `ExpectNoProviderMetrics()` should be used.
  void ExpectProviderMetrics(const std::string& provider,
                             int bucket_min,
                             bool completed) {
    ExpectSlicedMetrics("Provider." + provider, {{bucket_min, 1}}, completed);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  // Used to control time passed between calls. Many metrics tested are timing
  // metrics.
  FakeAutocompleteController controller_;
  // A convenience reference to `controller_.metrics_`.
  const raw_ref<AutocompleteControllerMetrics> metrics_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(AutocompleteControllerMetricsTest, SuggestionFinalization_SyncInput) {
  // Sync inputs should not log metrics.
  SetInputSync(true);
  SimulateStart(true, 1, {}, {{}});
  metrics_->OnStop();
  ExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_OnlySyncUpdate) {
  // Sync updates should log metrics.
  SimulateStart(true, 1, {}, {{}});
  ExpectSingleCountSuggestionFinalizationMetrics(1, 1, 1, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_OnlySyncUpdateWithNoChanges) {
  // Sync updates without changes should log metrics.
  SimulateStart(true, 1, {{}}, {{}});
  ExpectSingleCountSuggestionFinalizationMetrics(1, 0, 0, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_SyncAnd3AsyncUpdate) {
  // This is the typical flow: 1 sync update followed by multiple async updates.
  SimulateStart(false, 1, {}, {{}});
  // 1st async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({}, {{}});
  // 2nd async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({}, {{}});
  // Last async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  controller_.done_ = true;
  metrics_->OnNotifyChanged({}, {{}});
  ExpectSingleCountSuggestionFinalizationMetrics(4, 4, 4, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_SyncAnd3AsyncUpdateWithNoChanges) {
  // 1 sync and 3 async updates, none of the 4 has a change.
  SimulateStart(false, 1, {{}}, {{}});
  // 1st async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({{}}, {{}});
  // 2nd async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({{}}, {{}});
  // Last async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  controller_.done_ = true;
  metrics_->OnNotifyChanged({{}}, {{}});
  ExpectSingleCountSuggestionFinalizationMetrics(4, 0, 0, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(
    AutocompleteControllerMetricsTest,
    SuggestionFinalization_UnchangedSyncAnd2UnchangedAnd1ChangedAsyncUpdates) {
  // 1 sync and 3 async updates, only the last of the 4 has a change.
  SimulateStart(false, 1, {{}}, {{}});
  // 1st async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({{}}, {{}});
  // 2nd async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({{}}, {{}});
  // Last async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  controller_.done_ = true;
  metrics_->OnNotifyChanged({}, {{}});
  ExpectSingleCountSuggestionFinalizationMetrics(4, 4, 4, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(
    AutocompleteControllerMetricsTest,
    SuggestionFinalization_UnchangedSyncAnd1ChangedAnd2UnchangedAsyncUpdates) {
  // 1 sync and 3 async updates, only the 2nd of the 4 has a change.
  SimulateStart(false, 1, {{}}, {{}});
  // 1st async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({}, {{}});
  // 2nd async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({{}}, {{}});
  // Last async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  controller_.done_ = true;
  metrics_->OnNotifyChanged({{}}, {{}});
  ExpectSingleCountSuggestionFinalizationMetrics(4, 2, 2, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_ChangedSyncAnd3UnchangedAsyncUpdates) {
  // 1 sync and 3 async updates, only the 1st of the 4 has a change.
  SimulateStart(false, 1, {}, {{}});
  // 1st async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({{}}, {{}});
  // 2nd async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({{}}, {{}});
  // Last async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  controller_.done_ = true;
  metrics_->OnNotifyChanged({{}}, {{}});
  ExpectSingleCountSuggestionFinalizationMetrics(4, 1, 1, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_StopTimerReached) {
  // Simulates the case where the async updates take longer than the 1.5s stop
  // timer. It's not possible for the sync update to take longer, as the stop
  // timer only starts after the sync update. The logged times should however
  // measure starting from before the sync update.
  SimulateStart(false, 1, {}, {{}});
  // 1st async update.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({{}}, {{}});
  // Stop timer.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnStop();
  ExpectSingleCountSuggestionFinalizationMetrics(3, 1, 1, false);
  controller_.done_ = true;
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest, SuggestionFinalization_Interrupted) {
  // Start 1st input.
  SimulateStart(false, 1, {}, {{}});
  // 1 async update for 1st input.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({}, {{}});
  ExpectNoSuggestionFinalizationMetrics();
  // Interrupted by 2nd input. The log should include the time until
  // interruption.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  SimulateStart(false, 1, {}, {{}});
  ExpectSingleCountSuggestionFinalizationMetrics(3, 2, 2, false);
  // Interrupted by 3rd input.
  ResetHistogramTester();
  task_environment_.FastForwardBy(base::Milliseconds(1));
  SimulateStart(false, 1, {}, {{}});
  ExpectSingleCountSuggestionFinalizationMetrics(2, 1, 1, false);
  // 1 async update for 3rd input. Controller completes with the 2nd update.
  ResetHistogramTester();
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({}, {{}});
  ExpectNoSuggestionFinalizationMetrics();
  // 2nd and last async update for 3rd input.
  controller_.done_ = true;
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({}, {{}});
  ExpectSingleCountSuggestionFinalizationMetrics(3, 3, 3, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_DefaultUnchanged) {
  // Sync update.
  SimulateStart(false, 1, {}, {{}});
  // 1st async update with non-default match changed.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({{}}, {{}, {}});
  // 2nd async update with no matches changed.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  controller_.done_ = true;
  metrics_->OnNotifyChanged({{}}, {{}});
  ExpectSingleCountSuggestionFinalizationMetrics(3, 2, 1, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest, Provider_SyncAndAsyncCompletion) {
  scoped_refptr<FakeAutocompleteProvider> async_provider_done_sync =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_BOOKMARK);
  scoped_refptr<FakeAutocompleteProvider> async_provider_done_not_last =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_KEYWORD);
  scoped_refptr<FakeAutocompleteProvider> async_provider_done_last =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_BUILTIN);

  // Sync update with `async_provider_done_sync` completing.
  metrics_->OnStart();
  controller_.in_start_ = true;
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnProviderUpdate(*async_provider_done_sync);
  ExpectProviderMetrics(async_provider_done_sync->GetName(), 1, true);
  controller_.done_ = false;
  metrics_->OnNotifyChanged({{}}, {{}});
  controller_.in_start_ = false;
  ExpectNoSuggestionFinalizationMetrics();
  ResetHistogramTester();

  // 1st async update with `async_provider_done_not_last` completing.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnProviderUpdate(*async_provider_done_not_last);
  ExpectProviderMetrics(async_provider_done_not_last->GetName(), 2, true);
  metrics_->OnNotifyChanged({{}}, {{}});
  ExpectNoSuggestionFinalizationMetrics();
  ResetHistogramTester();

  // Last async update with `async_provider_done_last` completing.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnProviderUpdate(*async_provider_done_last);
  controller_.done_ = true;
  ExpectProviderMetrics(async_provider_done_last->GetName(), 3, true);
  metrics_->OnNotifyChanged({{}}, {{}});
  ExpectSingleCountSuggestionFinalizationMetrics(3, 0, 0, true);

  StopAndExpectNoSuggestionFinalizationMetrics();
  ExpectNoProviderMetrics(async_provider_done_sync->GetName());
  ExpectNoProviderMetrics(async_provider_done_not_last->GetName());
  ExpectNoProviderMetrics(async_provider_done_last->GetName());
}

TEST_F(AutocompleteControllerMetricsTest,
       Provider_1ProviderWithMultipleUpdates) {
  scoped_refptr<FakeAutocompleteProvider> provider =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_BOOKMARK);

  // Sync update without completion.
  metrics_->OnStart();
  controller_.in_start_ = true;
  provider->done_ = false;
  metrics_->OnProviderUpdate(*provider);
  controller_.done_ = false;
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnNotifyChanged({{}}, {{}});
  controller_.in_start_ = false;

  // 1st async update without completion.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  metrics_->OnProviderUpdate(*provider);
  metrics_->OnNotifyChanged({{}}, {{}});

  // Last async update with completion.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  provider->done_ = true;
  controller_.done_ = true;
  metrics_->OnProviderUpdate(*provider);
  metrics_->OnNotifyChanged({{}}, {{}});

  ExpectProviderMetrics(provider->GetName(), 3, true);
  ExpectSingleCountSuggestionFinalizationMetrics(3, 0, 0, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
  ExpectNoProviderMetrics(provider->GetName());
}

TEST_F(AutocompleteControllerMetricsTest, Provider_Interrupted) {
  scoped_refptr<FakeAutocompleteProvider> provider_started =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_BOOKMARK);
  scoped_refptr<FakeAutocompleteProvider> provider_not_started =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_SEARCH);

  controller_.providers_.push_back(provider_started);
  controller_.providers_.push_back(provider_not_started);

  // Sync update.
  SimulateStart(false, 1, {{}}, {{}});
  // Simulate stopping while `provider_started` is still ongoing.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  provider_started->done_ = false;
  metrics_->OnStop();
  provider_started->done_ = true;
  controller_.done_ = true;

  ExpectProviderMetrics(provider_started->GetName(), 2, false);
  ExpectNoProviderMetrics(provider_not_started->GetName());
  ExpectSingleCountSuggestionFinalizationMetrics(2, 0, 0, false);
}

TEST_F(AutocompleteControllerMetricsTest, MatchStability) {
  auto create_result = [&](std::vector<int> ids) {
    std::vector<AutocompleteResult::MatchDedupComparator> result;
    base::ranges::transform(ids, std::back_inserter(result), [](int id) {
      return AutocompleteResult::MatchDedupComparator{
          "http://" + base::NumberToString(id), false};
    });
    return result;
  };

  const auto first_result = create_result({0, 1, 2, 3, 4});
  // Same as `first_result`, but with these changes:
  //  - Last two matches removed.
  //  - Default match updated to a new URL.
  //  - Third match updated to a new URL.
  const auto second_result = create_result({10, 1, 11});
  // Same as `second_result`, but with these changes:
  //  - 2 new matches appended to the bottom.
  const auto third_result = create_result({10, 1, 11, 10, 2});

  // Verify logging to the Async* histograms.
  controller_.in_start_ = false;
  metrics_->OnNotifyChanged(first_result, second_result);
  // Expect the default match, third match, and last two matches to be logged
  // as changed, and nothing else.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex.Async"),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(2, 1),
                                   base::Bucket(3, 1), base::Bucket(4, 1)));
  // Expect that we log that at least one of the matches has changed.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeInAnyPosition.Async"),
              testing::ElementsAre(base::Bucket(1, 1)));
  // Expect that we don't log async updates to the sync histograms.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex.CrossInput"),
              testing::ElementsAre());
  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "Omnibox.MatchStability2.MatchChangeInAnyPosition.CrossInput"),
      testing::ElementsAre());
  // Verify the unsliced histograms.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex"),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(2, 1),
                                   base::Bucket(3, 1), base::Bucket(4, 1)));
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeInAnyPosition"),
              testing::ElementsAre(base::Bucket(1, 1)));
  ResetHistogramTester();

  // Verify logging to the CrossInput* histograms.
  controller_.in_start_ = true;
  metrics_->OnNotifyChanged(first_result, second_result);
  // Expect the default match, third match, and last two matches to be logged
  // as changed, and nothing else.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex.CrossInput"),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(2, 1),
                                   base::Bucket(3, 1), base::Bucket(4, 1)));
  // Expect that we log that at least one of the matches has changed.
  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "Omnibox.MatchStability2.MatchChangeInAnyPosition.CrossInput"),
      testing::ElementsAre(base::Bucket(1, 1)));
  // Expect that we don't log sync updates to the async histograms.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex.Async"),
              testing::ElementsAre());
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeInAnyPosition.Async"),
              testing::ElementsAre());
  // Verify the unsliced histograms.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex"),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(2, 1),
                                   base::Bucket(3, 1), base::Bucket(4, 1)));
  // Expect that we log that at least one of the matches has changed.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeInAnyPosition"),
              testing::ElementsAre(base::Bucket(1, 1)));
  ResetHistogramTester();

  // Verify no logging when appending matches.
  controller_.in_start_ = false;
  metrics_->OnNotifyChanged(second_result, third_result);
  controller_.in_start_ = true;
  metrics_->OnNotifyChanged(second_result, third_result);
  // Expect no changes logged; expect 1 false logged to
  // *MatchChangedInAnyPosition.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex.Async"),
              testing::ElementsAre());
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeInAnyPosition.Async"),
              testing::ElementsAre(base::Bucket(0, 1)));
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex.CrossInput"),
              testing::ElementsAre());
  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "Omnibox.MatchStability2.MatchChangeInAnyPosition.CrossInput"),
      testing::ElementsAre(base::Bucket(0, 1)));
  // Verify the unsliced histograms.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex"),
              testing::ElementsAre());
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeInAnyPosition"),
              testing::ElementsAre(base::Bucket(0, 2)));
  ResetHistogramTester();
}
