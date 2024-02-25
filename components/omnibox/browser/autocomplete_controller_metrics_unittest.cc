// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_controller_metrics.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/fake_autocomplete_controller.h"
#include "components/omnibox/browser/fake_autocomplete_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::ElementsAreArray;

namespace {
// A fake provider that will:
//   a) Consume 1ms to run the sync pass.
//   b) Can be configured whether to be done or not after the sync pass.
class FakeAutocompleteProviderDelayed : public FakeAutocompleteProvider {
 public:
  FakeAutocompleteProviderDelayed(
      Type type,
      raw_ptr<base::test::SingleThreadTaskEnvironment> task_environment,
      bool done_after_sync_pass = false)
      : FakeAutocompleteProvider(type),
        task_environment_(task_environment),
        done_after_sync_pass_(done_after_sync_pass) {}

  void Start(const AutocompleteInput& input, bool minimal_changes) override {
    done_ = done_after_sync_pass_;
    task_environment_->FastForwardBy(base::Milliseconds(1));
    FakeAutocompleteProvider::Start(input, minimal_changes);
  }

  // Used to simulate the sync pass consuming 1ms.
  raw_ptr<base::test::SingleThreadTaskEnvironment> task_environment_ = nullptr;

  // Whether the provider should be done after the sync pass.
  bool done_after_sync_pass_;

 protected:
  ~FakeAutocompleteProviderDelayed() override = default;
};
}  // namespace

class AutocompleteControllerMetricsTest : public testing::Test {
 public:
  AutocompleteControllerMetricsTest()
      : controller_(&task_environment_),
        histogram_tester_(std::make_unique<base::HistogramTester>()) {
    controller_.providers_ = {
        base::MakeRefCounted<FakeAutocompleteProviderDelayed>(
            AutocompleteProvider::Type::TYPE_BOOKMARK, &task_environment_)};

    // Allow tests to simulate an initial update with no changes. Since the
    // 0-matches cases is special handled, tests can't simply do
    // `Simulate(true|false, {})` to simulate an initial update with no changes;
    // this allows tests to instead do `Simulate(true|false, {CreateMatch(0)})`.
    SimulateStart(true, {CreateMatch(0)});
    ResetHistogramTester();
  }

  AutocompleteMatch CreateMatch(int i) {
    const std::string name = base::NumberToString(i);
    AutocompleteMatch match{nullptr, 1000, false,
                            AutocompleteMatchType::HISTORY_URL};
    match.destination_url = GURL{"https://google.com/" + name};
    return match;
  }

  // By default, the controller wants async matches. `SetInputSync()` will
  // explicitly set this behavior.
  void SetInputSync(bool sync) {
    controller_.input_.set_omit_asynchronous_matches(sync);
  }

  // Mimics `AutocompleteController::Start()`'s behavior. `done` determines
  // whether the controller should be done after the sync pass. `old_result` and
  // `new_result` are passed to
  // `AutocompleteControllerMetrics::OnUpdateResult()` to determine which
  // matches changed.
  void SimulateStart(bool done, std::vector<AutocompleteMatch> matches) {
    controller_.GetFakeProvider().matches_ = matches;
    controller_.GetFakeProvider().done_ = true;
    controller_.GetFakeProvider<FakeAutocompleteProviderDelayed>()
        .done_after_sync_pass_ = done;
    controller_.Start(controller_.input_);
  }

  // Mimics `AutocompleteController::NotifyChanged()`'s behavior. `done`
  // determines weather the controller should be done after this async update.
  // `old_result` and `new_result` are passed to
  // `AutocompleteControllerMetrics::OnUpdateResult()` to determine which
  // matches changed.
  void SimulateAsyncUpdate(bool done, std::vector<AutocompleteMatch> matches) {
    controller_.GetFakeProvider().matches_ = matches;
    controller_.GetFakeProvider().done_ = done;
    task_environment_.FastForwardBy(base::Milliseconds(1));
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider());
  }

  // Simulates `AutocompleteController::Stop()`.
  void SimulateOnStop() {
    task_environment_.FastForwardBy(base::Milliseconds(1));
    controller_.Stop(false);
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
    controller_.Stop(false, false);
    ExpectNoSuggestionFinalizationMetrics();
  }

  // Convenience method to check the buckets of a single metric.
  void ExpectMetrics(const std::string metric_name,
                     std::optional<int> expected_time_ms) {
    SCOPED_TRACE(metric_name);
    const std::string full_name =
        "Omnibox.AsyncAutocompletionTime2." + metric_name;
    histogram_tester_->ExpectTotalCount(full_name,
                                        expected_time_ms.has_value());
    if (expected_time_ms) {
      // Total sum is exact, whereas buckets aren't. E.g. we can't verify
      // whether a metric recorded 300 or 301 if the containing bucket is [290,
      // 310].
      EXPECT_EQ(histogram_tester_->GetTotalSum(full_name), *expected_time_ms);
    }
  }

  // Convenience method to check the buckets of 3 metrics:
  // - '...<metric>' will be expected to have `buckets`.
  // - '...<metric>.[Completed]' will be expected to either have
  //   `buckets` or be empty, depending on `completed`.
  void ExpectSlicedMetrics(const std::string& metric,
                           std::optional<int> expected_time_ms,
                           bool completed) {
    ExpectMetrics(metric, expected_time_ms);
    ExpectMetrics(metric + ".Completed",
                  completed ? expected_time_ms : std::nullopt);
    ExpectMetrics(metric + ".Interrupted",
                  !completed ? expected_time_ms : std::nullopt);
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
    ExpectSlicedMetrics("Done", done_bucket_min, completed);
    ExpectSlicedMetrics("LastChange", last_change_bucket_min, completed);
    ExpectSlicedMetrics("LastDefaultChange", last_default_change_bucket_min,
                        completed);
    ResetHistogramTester();
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
    ExpectSlicedMetrics("Provider." + provider, bucket_min, completed);
  }

  // Used to control time passed between calls. Many metrics tested are timing
  // metrics.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeAutocompleteController controller_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(AutocompleteControllerMetricsTest, SuggestionFinalization_SyncInput) {
  // Sync inputs should not log metrics.
  SetInputSync(true);
  SimulateStart(true, {CreateMatch(1)});
  SimulateOnStop();
  ExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_OnlySyncUpdate) {
  // Sync updates should log metrics.
  SimulateStart(true, {CreateMatch(1)});
  ExpectSingleCountSuggestionFinalizationMetrics(1, 1, 1, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_OnlySyncUpdateWithNoChanges) {
  // Sync updates without changes should log metrics.
  SimulateStart(true, {CreateMatch(0)});
  ExpectSingleCountSuggestionFinalizationMetrics(1, 0, 0, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_SyncAnd3AsyncUpdate) {
  // This is the typical flow: 1 sync update followed by multiple async updates.
  SimulateStart(false, {CreateMatch(1)});
  // 1st async update.
  SimulateAsyncUpdate(false, {CreateMatch(2)});
  // 2nd async update.
  SimulateAsyncUpdate(false, {CreateMatch(3)});
  // Last async update.
  SimulateAsyncUpdate(true, {CreateMatch(4)});
  ExpectSingleCountSuggestionFinalizationMetrics(4, 4, 4, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_SyncAnd3AsyncUpdateWithNoChanges) {
  // 1 sync and 3 async updates, none of the 4 has a change.
  SimulateStart(false, {CreateMatch(0)});
  // 1st async update.
  SimulateAsyncUpdate(false, {CreateMatch(0)});
  // 2nd async update.
  SimulateAsyncUpdate(false, {CreateMatch(0)});
  // Last async update.
  SimulateAsyncUpdate(true, {CreateMatch(0)});
  ExpectSingleCountSuggestionFinalizationMetrics(4, 0, 0, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(
    AutocompleteControllerMetricsTest,
    SuggestionFinalization_UnchangedSyncAnd2UnchangedAnd1ChangedAsyncUpdates) {
  // 1 sync and 3 async updates, only the last of the 4 has a change.
  SimulateStart(false, {CreateMatch(0)});
  // 1st async update.
  SimulateAsyncUpdate(false, {CreateMatch(0)});
  // 2nd async update.
  SimulateAsyncUpdate(false, {CreateMatch(0)});
  // Last async update.
  SimulateAsyncUpdate(true, {CreateMatch(1)});
  ExpectSingleCountSuggestionFinalizationMetrics(4, 4, 4, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(
    AutocompleteControllerMetricsTest,
    SuggestionFinalization_UnchangedSyncAnd1ChangedAnd2UnchangedAsyncUpdates) {
  // 1 sync and 3 async updates, only the 2nd of the 4 has a change. Because of
  // debouncing, the change doesn't apply until the 4th update.
  SimulateStart(false, {CreateMatch(0)});
  // 1st async update.
  SimulateAsyncUpdate(false, {CreateMatch(1)});
  // 2nd async update.
  SimulateAsyncUpdate(false, {CreateMatch(1)});
  // Last async update.
  SimulateAsyncUpdate(true, {CreateMatch(1)});
  ExpectSingleCountSuggestionFinalizationMetrics(4, 4, 4, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(
    AutocompleteControllerMetricsTest,
    SuggestionFinalization_UnchangedSyncAnd1ChangedAnd2UnchangedAsyncUpdates_ChangeAppliedBeforeDone) {
  // 1 sync and 3 async updates, only the 2nd of the 4 has a change. The
  // debounced change applies before the last update.
  SimulateStart(false, {CreateMatch(0)});
  // 1st async update.
  SimulateAsyncUpdate(false, {CreateMatch(1)});
  task_environment_.FastForwardBy(base::Milliseconds(250));
  // 2nd async update.
  SimulateAsyncUpdate(false, {CreateMatch(1)});
  // Last async update.
  SimulateAsyncUpdate(true, {CreateMatch(1)});
  ExpectSingleCountSuggestionFinalizationMetrics(254, 202, 202, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_ChangedSyncAnd3UnchangedAsyncUpdates) {
  // 1 sync and 3 async updates, only the 1st of the 4 has a change.
  SimulateStart(false, {CreateMatch(1)});
  // 1st async update.
  SimulateAsyncUpdate(false, {CreateMatch(1)});
  // 2nd async update.
  SimulateAsyncUpdate(false, {CreateMatch(1)});
  // Last async update.
  SimulateAsyncUpdate(true, {CreateMatch(1)});
  ExpectSingleCountSuggestionFinalizationMetrics(4, 1, 1, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_StopTimerReached) {
  // Simulates the case where the async updates take longer than the 1.5s stop
  // timer. It's not possible for the sync update to take longer, as the stop
  // timer only starts after the sync update. The logged times should however
  // measure starting from before the sync update.
  SimulateStart(false, {CreateMatch(1)});
  // 1st async update.
  SimulateAsyncUpdate(false, {CreateMatch(1)});
  // Stop timer.
  SimulateOnStop();
  ExpectSingleCountSuggestionFinalizationMetrics(3, 1, 1, false);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest, SuggestionFinalization_Interrupted) {
  // Start 1st input.
  SimulateStart(false, {CreateMatch(1)});
  // 1 async update for 1st input.
  SimulateAsyncUpdate(false, {CreateMatch(2)});
  // Wait for the debouncer.
  task_environment_.FastForwardBy(base::Milliseconds(250));
  ExpectNoSuggestionFinalizationMetrics();
  // Interrupted by 2nd input. The log should include the time until
  // interruption.
  SimulateStart(false, {CreateMatch(3)});
  ExpectSingleCountSuggestionFinalizationMetrics(252, 202, 202, false);
  // Interrupted by 3rd input.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  SimulateStart(false, {CreateMatch(4)});
  ExpectSingleCountSuggestionFinalizationMetrics(2, 1, 1, false);
  // 1st async update for 3rd input. Controller completes with the 2nd async
  // update.
  SimulateAsyncUpdate(false, {CreateMatch(5)});
  ExpectNoSuggestionFinalizationMetrics();
  // 2nd and last async update for 3rd input.
  SimulateAsyncUpdate(true, {CreateMatch(6)});
  ExpectSingleCountSuggestionFinalizationMetrics(3, 3, 3, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest, SuggestionFinalization_ExpireTimer) {
  SimulateStart(true, {CreateMatch(0), CreateMatch(1)});
  ResetHistogramTester();
  // A sync update without matches. The transferred match should remain.
  SimulateStart(false, {CreateMatch(2)});
  // Wait for the expire timer.
  task_environment_.FastForwardBy(base::Milliseconds(1000));
  // Last update with no matches.
  ExpectNoSuggestionFinalizationMetrics();
  SimulateAsyncUpdate(true, {CreateMatch(2)});
  // 500 expire timer + 200 debounce timer + 1 sync pass delay = 701.
  ExpectSingleCountSuggestionFinalizationMetrics(1002, 701, 1, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_MatchDeletion) {
  SimulateStart(true, {CreateMatch(0), CreateMatch(1)});
  ResetHistogramTester();

  auto match = CreateMatch(0);
  match.provider = &controller_.GetFakeProvider();
  match.deletable = true;

  // Match deletion with changes. Since autocompletion is complete, finalization
  // metrics aren't logged. Stability metrics are logged, but that's just
  // arbitrary for consistency with historic behavior / code simplicity, and we
  // could stop logging them for deletions if we chose.
  controller_.DeleteMatch(match);
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeInAnyPosition"),
              testing::ElementsAre(base::Bucket(1, 1)));
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest,
       SuggestionFinalization_DefaultUnchanged) {
  // Sync update with a default match change.
  SimulateStart(false, {CreateMatch(1)});
  // 1st async update with non-default match changed.
  SimulateAsyncUpdate(false, {CreateMatch(1), CreateMatch(2)});
  // Wait for the debounce timer.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  // 2nd async update with no matches changed.
  SimulateAsyncUpdate(true, {CreateMatch(1), CreateMatch(2)});
  ExpectSingleCountSuggestionFinalizationMetrics(203, 202, 1, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
}

TEST_F(AutocompleteControllerMetricsTest, Provider_SyncAndAsyncCompletion) {
  controller_.providers_ = {
      base::MakeRefCounted<FakeAutocompleteProviderDelayed>(
          AutocompleteProvider::Type::TYPE_BOOKMARK, &task_environment_, true),
      base::MakeRefCounted<FakeAutocompleteProviderDelayed>(
          AutocompleteProvider::Type::TYPE_KEYWORD, &task_environment_),
      base::MakeRefCounted<FakeAutocompleteProviderDelayed>(
          AutocompleteProvider::Type::TYPE_BUILTIN, &task_environment_),
  };
  {
    SCOPED_TRACE("Sync update with 1st provider completing.");
    controller_.GetFakeProvider(0).matches_ = {CreateMatch(0)};
    controller_.Start(controller_.input_);
    ExpectNoProviderMetrics(controller_.GetFakeProvider(0).GetName());
    ExpectNoProviderMetrics(controller_.GetFakeProvider(1).GetName());
    ExpectNoProviderMetrics(controller_.GetFakeProvider(2).GetName());
    ExpectNoSuggestionFinalizationMetrics();
    ResetHistogramTester();
  }
  {
    SCOPED_TRACE("1st async update with 2nd provider completing.");
    task_environment_.FastForwardBy(base::Milliseconds(1));
    controller_.GetFakeProvider(1).done_ = true;
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider(1));
    ExpectNoProviderMetrics(controller_.GetFakeProvider(0).GetName());
    ExpectProviderMetrics(controller_.GetFakeProvider(1).GetName(), 4, true);
    ExpectNoProviderMetrics(controller_.GetFakeProvider(2).GetName());
    ExpectNoSuggestionFinalizationMetrics();
    ResetHistogramTester();
  }
  {
    SCOPED_TRACE("Last async update with 3rd provider completing.");
    task_environment_.FastForwardBy(base::Milliseconds(1));
    controller_.GetFakeProvider(2).done_ = true;
    controller_.OnProviderUpdate(true, &controller_.GetFakeProvider(2));
    ExpectNoProviderMetrics(controller_.GetFakeProvider(0).GetName());
    ExpectNoProviderMetrics(controller_.GetFakeProvider(1).GetName());
    ExpectProviderMetrics(controller_.GetFakeProvider(2).GetName(), 5, true);
    ExpectSingleCountSuggestionFinalizationMetrics(5, 0, 0, true);
    ResetHistogramTester();
  }
  {
    SCOPED_TRACE("Stop.");
    StopAndExpectNoSuggestionFinalizationMetrics();
    ExpectNoProviderMetrics(controller_.GetFakeProvider(0).GetName());
    ExpectNoProviderMetrics(controller_.GetFakeProvider(1).GetName());
    ExpectNoProviderMetrics(controller_.GetFakeProvider(2).GetName());
    ResetHistogramTester();
  }
}

TEST_F(AutocompleteControllerMetricsTest,
       Provider_1ProviderWithMultipleUpdates) {
  // Sync update without completion.
  controller_.GetFakeProvider().done_ = true;
  SimulateStart(false, {CreateMatch(0)});

  // 1st async update without completion.
  SimulateAsyncUpdate(false, {CreateMatch(0)});

  // Last async update with completion.
  SimulateAsyncUpdate(true, {CreateMatch(0)});
  ExpectProviderMetrics(controller_.GetFakeProvider().GetName(), 3, true);
  ExpectSingleCountSuggestionFinalizationMetrics(3, 0, 0, true);
  StopAndExpectNoSuggestionFinalizationMetrics();
  ExpectNoProviderMetrics(controller_.GetFakeProvider().GetName());
}

TEST_F(AutocompleteControllerMetricsTest, Provider_Interrupted) {
  controller_.providers_ = {
      base::MakeRefCounted<FakeAutocompleteProviderDelayed>(
          AutocompleteProvider::Type::TYPE_BOOKMARK, &task_environment_),
      base::MakeRefCounted<FakeAutocompleteProviderDelayed>(
          AutocompleteProvider::Type::TYPE_KEYWORD, &task_environment_),
  };

  // Sync update. 1st provider completes; 2nd provider does not.
  controller_.GetFakeProvider(0).done_ = true;
  controller_.GetFakeProvider(1).done_ = true;
  controller_.GetFakeProvider<FakeAutocompleteProviderDelayed>(0)
      .done_after_sync_pass_ = true;
  controller_.GetFakeProvider<FakeAutocompleteProviderDelayed>(1)
      .done_after_sync_pass_ = false;
  controller_.Start(controller_.input_);

  // 2nd provider is interrupted by stop.
  SimulateOnStop();

  ExpectNoProviderMetrics(controller_.GetFakeProvider(0).GetName());
  ExpectProviderMetrics(controller_.GetFakeProvider(1).GetName(), 3, false);
  ExpectSingleCountSuggestionFinalizationMetrics(3, 0, 0, false);
}

TEST_F(AutocompleteControllerMetricsTest, MatchStability) {
  auto create_result = [&](std::vector<int> ids) {
    std::vector<AutocompleteMatch> matches;
    base::ranges::transform(ids, std::back_inserter(matches), [&](int id) {
      auto match = CreateMatch(id);
      match.relevance -= id;
      return match;
    });
    return matches;
  };

  const auto first_result = create_result({10, 11, 12, 13, 14});
  // Same as `first_result`, but with these changes:
  //  - Last two matches removed.
  //  - Default match updated to a new URL.
  //  - Third match updated to a new URL.
  const auto second_result = create_result({0, 11, 20});
  // Same as `second_result`, but with these changes:
  //  - 2 new matches appended to the bottom.
  const auto third_result = create_result({0, 11, 20, 21, 22});

  // Verify logging to the CrossInput* histograms.
  SimulateStart(true, first_result);
  ResetHistogramTester();
  SimulateStart(true, second_result);
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
  // Expect that we don't log sync updates to the Async histograms.
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
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeInAnyPosition"),
              testing::ElementsAre(base::Bucket(1, 1)));
  ResetHistogramTester();

  // Verify logging to the Async* histograms.
  SimulateStart(false, first_result);
  ResetHistogramTester();
  SimulateAsyncUpdate(true, second_result);
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
  // Expect that we don't log sync updates to the CrossInput histograms.
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
  // Expect that we log that at least one of the matches has changed.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeInAnyPosition"),
              testing::ElementsAre(base::Bucket(1, 1)));
  ResetHistogramTester();

  // Verify no logging when appending matches for sync updates.
  SimulateStart(true, second_result);
  ResetHistogramTester();
  SimulateStart(true, third_result);
  // Expect no changes logged; expect 1 false logged to each
  // MatchChangeInAnyPosition.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex.CrossInput"),
              testing::ElementsAre());
  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "Omnibox.MatchStability2.MatchChangeInAnyPosition.CrossInput"),
      testing::ElementsAre(base::Bucket(0, 1)));
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex.Async"),
              testing::ElementsAre());
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeInAnyPosition.Async"),
              testing::ElementsAre());
  // Verify the unsliced histograms.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex"),
              testing::ElementsAre());
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeInAnyPosition"),
              testing::ElementsAre(base::Bucket(0, 1)));
  ResetHistogramTester();

  // Verify no logging when appending matches for async updates.
  SimulateStart(false, second_result);
  ResetHistogramTester();
  SimulateAsyncUpdate(true, third_result);
  // Expect no changes logged; expect 1 false logged to each
  // MatchChangeInAnyPosition.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex.CrossInput"),
              testing::ElementsAre());
  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "Omnibox.MatchStability2.MatchChangeInAnyPosition.CrossInput"),
      testing::ElementsAre());
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex.Async"),
              testing::ElementsAre());
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeInAnyPosition.Async"),
              testing::ElementsAre(base::Bucket(0, 1)));
  // Verify the unsliced histograms.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeIndex"),
              testing::ElementsAre());
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Omnibox.MatchStability2.MatchChangeInAnyPosition"),
              testing::ElementsAre(base::Bucket(0, 1)));
  ResetHistogramTester();
}
