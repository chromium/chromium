// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_insights/populations/typed_population.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/private_insights/events/contextual_cue_log_event.pb.h"
#include "components/private_insights/populations/pending_events_batch.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/federated_compute/src/fcp/client/example_query_result.pb.h"

namespace private_insights {

namespace {

BASE_FEATURE(kTestFeature, base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kTestPopulationName{
    &kTestFeature, "population_name", "test_population"};
const base::FeatureParam<int> kTestMaxEvents{&kTestFeature, "max_events", 10};

constexpr char kHistogramName[] = "TestEvents";
constexpr char kOnShutdownCountHistogram[] =
    "PrivateMetrics.PrivateInsights.TestEvents.Logging.OnShutdownCount";
constexpr char kQueuedCountHistogram[] =
    "PrivateMetrics.PrivateInsights.TestEvents.Logging.QueuedCount";
constexpr char kRemovedCountHistogram[] =
    "PrivateMetrics.PrivateInsights.TestEvents.Logging.RemovedCount";
constexpr char kRequeuedCountHistogram[] =
    "PrivateMetrics.PrivateInsights.TestEvents.Requeuing.RequeuedCount";
constexpr char kDroppedCountHistogram[] =
    "PrivateMetrics.PrivateInsights.TestEvents.Requeuing.DroppedCount";

using TestPopulation = TypedPopulation<events::ContextualCueLogEvent>;

events::ContextualCueLogEvent MakeEvent(const std::string& cue_id) {
  events::ContextualCueLogEvent event;
  event.set_cue_id(cue_id);
  return event;
}

std::vector<std::string> CueIds(const TestPopulation& population) {
  std::vector<std::string> ids;
  for (const auto& entry : population.queue_for_testing().entries()) {
    ids.push_back(entry.event.cue_id());
  }
  return ids;
}

class TypedPopulationTest : public testing::Test {
 protected:
  std::unique_ptr<TestPopulation> MakePopulation() {
    return std::make_unique<TestPopulation>(kHistogramName, kTestPopulationName,
                                            kTestMaxEvents);
  }

  // May be called more than once; the population re-reads the param.
  void SetMaxEvents(int max_events) {
    feature_list_.Reset();
    feature_list_.InitAndEnableFeatureWithParameters(
        kTestFeature, {{"max_events", base::NumberToString(max_events)}});
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

TEST_F(TypedPopulationTest, ConfigurationComesFromParams) {
  auto population = MakePopulation();
  EXPECT_EQ(population->population_name(), "test_population");
  EXPECT_EQ(population->pending_event_count(), 0u);
  EXPECT_TRUE(population->queue_for_testing().empty());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kTestFeature, {{"population_name", "overridden_population"}});
  EXPECT_EQ(population->population_name(), "overridden_population");
}

TEST_F(TypedPopulationTest, HistogramsUseSharedPrefixAndName) {
  auto population = MakePopulation();
  population->LogEvent(MakeEvent("cue_1"));
  histogram_tester_.ExpectUniqueSample(kQueuedCountHistogram, 1, 1);
}

TEST_F(TypedPopulationTest, LogEventQueuesUpToMaxEvents) {
  SetMaxEvents(2);
  auto population = MakePopulation();
  population->LogEvent(MakeEvent("cue_1"));
  population->LogEvent(MakeEvent("cue_2"));
  population->LogEvent(MakeEvent("cue_3"));

  EXPECT_EQ(population->pending_event_count(), 2u);
  EXPECT_EQ(CueIds(*population), (std::vector<std::string>{"cue_2", "cue_3"}));
  histogram_tester_.ExpectUniqueSample(kRemovedCountHistogram, 1, 1);
}

TEST_F(TypedPopulationTest, TakePendingEventsDetachesAndSerializes) {
  auto population = MakePopulation();
  population->LogEvent(MakeEvent("cue_1"));
  population->LogEvent(MakeEvent("cue_2"));

  std::unique_ptr<PendingEventsBatch> batch = population->TakePendingEvents();
  ASSERT_TRUE(batch);
  EXPECT_EQ(batch->size(), 2u);
  EXPECT_EQ(population->pending_event_count(), 0u);

  fcp::client::ExampleQueryResult query_result;
  batch->SerializeToQueryResult(&query_result);
  EXPECT_EQ(query_result.result_source(),
            fcp::client::ExampleQueryResult::PRIVATE_LOGGER);
  EXPECT_EQ(query_result.stats().output_rows_count(), 2);
  const auto& vectors = query_result.vector_data().vectors();
  ASSERT_EQ(vectors.count("entry"), 1u);
  ASSERT_EQ(vectors.count("confidential_compute_event_time"), 1u);
  EXPECT_EQ(vectors.at("entry").bytes_values().value_size(), 2);
  EXPECT_EQ(vectors.at("confidential_compute_event_time")
                .string_values()
                .value_size(),
            2);
}

TEST_F(TypedPopulationTest, TakePendingEventsOnEmptyPopulationIsEmptyBatch) {
  auto population = MakePopulation();
  std::unique_ptr<PendingEventsBatch> batch = population->TakePendingEvents();
  ASSERT_TRUE(batch);
  EXPECT_EQ(batch->size(), 0u);
}

TEST_F(TypedPopulationTest, RequeuePrependsPreservingOrder) {
  auto population = MakePopulation();
  population->LogEvent(MakeEvent("cue_1"));
  population->LogEvent(MakeEvent("cue_2"));
  std::unique_ptr<PendingEventsBatch> batch = population->TakePendingEvents();

  // New events arrive while the batch is in flight.
  population->LogEvent(MakeEvent("cue_3"));

  batch->Requeue();

  EXPECT_EQ(CueIds(*population),
            (std::vector<std::string>{"cue_1", "cue_2", "cue_3"}));
  histogram_tester_.ExpectUniqueSample(kRequeuedCountHistogram, 2, 1);
  histogram_tester_.ExpectUniqueSample(kDroppedCountHistogram, 0, 1);
}

TEST_F(TypedPopulationTest, RequeueUsesCapInEffectWhenBatchWasTaken) {
  SetMaxEvents(3);
  auto population = MakePopulation();
  population->LogEvent(MakeEvent("cue_1"));
  population->LogEvent(MakeEvent("cue_2"));
  std::unique_ptr<PendingEventsBatch> batch = population->TakePendingEvents();

  // The cap shrinks (a Finch change) while the upload is in flight, and a
  // new event arrives.
  SetMaxEvents(1);
  population->LogEvent(MakeEvent("cue_3"));

  batch->Requeue();

  // The requeue applies the cap snapshotted when the batch was taken (3): two
  // slots are free next to "cue_3", so both in-flight events fit. Reading the
  // current cap (1) instead would have dropped both.
  EXPECT_EQ(CueIds(*population),
            (std::vector<std::string>{"cue_1", "cue_2", "cue_3"}));
  histogram_tester_.ExpectUniqueSample(kRequeuedCountHistogram, 2, 1);
  histogram_tester_.ExpectUniqueSample(kDroppedCountHistogram, 0, 1);
}

TEST_F(TypedPopulationTest, RequeueDropsOldestWhenExceedingCap) {
  SetMaxEvents(2);
  auto population = MakePopulation();
  population->LogEvent(MakeEvent("cue_1"));
  population->LogEvent(MakeEvent("cue_2"));
  std::unique_ptr<PendingEventsBatch> batch = population->TakePendingEvents();
  population->LogEvent(MakeEvent("cue_3"));

  batch->Requeue();

  // One slot free: the newest in-flight event ("cue_2") takes it.
  EXPECT_EQ(CueIds(*population), (std::vector<std::string>{"cue_2", "cue_3"}));
  histogram_tester_.ExpectUniqueSample(kRequeuedCountHistogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(kDroppedCountHistogram, 1, 1);
}

TEST_F(TypedPopulationTest, RequeueAfterDestructionIsNoOp) {
  auto population = MakePopulation();
  population->LogEvent(MakeEvent("cue_1"));
  std::unique_ptr<PendingEventsBatch> batch = population->TakePendingEvents();

  // The upload outlives the population (e.g. profile destroyed mid-upload).
  population.reset();
  batch->Requeue();

  histogram_tester_.ExpectTotalCount(kRequeuedCountHistogram, 0);
  histogram_tester_.ExpectTotalCount(kDroppedCountHistogram, 0);
}

TEST_F(TypedPopulationTest, OnShutdownRecordsPendingCount) {
  auto population = MakePopulation();
  population->LogEvent(MakeEvent("cue_1"));
  population->LogEvent(MakeEvent("cue_2"));

  population->OnShutdown();

  histogram_tester_.ExpectUniqueSample(kOnShutdownCountHistogram, 2, 1);
}

}  // namespace

}  // namespace private_insights
