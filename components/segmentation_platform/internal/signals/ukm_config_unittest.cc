// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/ukm_config.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

using ::testing::IsEmpty;

UkmMetricHash TestMetric(uint64_t val) {
  return UkmMetricHash::FromUnsafeValue(val);
}

}  // namespace

TEST(UkmConfigTest, AddEvents) {
  auto generator = UkmEventHash::Generator();
  const UkmEventHash kEvent1 = generator.GenerateNextId();
  const UkmEventHash kEvent2 = generator.GenerateNextId();
  const UkmEventHash kEvent3 = generator.GenerateNextId();

  UkmConfig config;
  EXPECT_EQ(config.metrics_for_event_for_testing(),
            UkmConfig::EventsToMetricsMap());
  EXPECT_EQ(config.GetObservedMetrics(kEvent1), nullptr);
  EXPECT_EQ(config.GetRawObservedEvents(), base::flat_set<uint64_t>());

  config.AddEvent(kEvent1, {TestMetric(100), TestMetric(101), TestMetric(102)});
  EXPECT_EQ(
      config.metrics_for_event_for_testing(),
      UkmConfig::EventsToMetricsMap(
          {{kEvent1, {TestMetric(100), TestMetric(101), TestMetric(102)}}}));

  EXPECT_EQ(*config.GetObservedMetrics(kEvent1),
            base::flat_set<UkmMetricHash>(
                {TestMetric(100), TestMetric(101), TestMetric(102)}));
  EXPECT_EQ(config.GetObservedMetrics(kEvent3), nullptr);
  EXPECT_EQ(config.GetRawObservedEvents(),
            base::flat_set<uint64_t>({kEvent1.GetUnsafeValue()}));

  config.AddEvent(kEvent2, {TestMetric(100)});
  EXPECT_EQ(config.metrics_for_event_for_testing(),
            UkmConfig::EventsToMetricsMap(
                {{kEvent1, {TestMetric(100), TestMetric(101), TestMetric(102)}},
                 {kEvent2, {TestMetric(100)}}}));

  config.AddEvent(kEvent2, {TestMetric(100), TestMetric(102), TestMetric(103)});
  EXPECT_EQ(*config.GetObservedMetrics(kEvent1),
            base::flat_set<UkmMetricHash>(
                {TestMetric(100), TestMetric(101), TestMetric(102)}));
  EXPECT_EQ(*config.GetObservedMetrics(kEvent2),
            base::flat_set<UkmMetricHash>(
                {TestMetric(100), TestMetric(102), TestMetric(103)}));
  EXPECT_EQ(config.GetRawObservedEvents(),
            base::flat_set<uint64_t>(
                {kEvent1.GetUnsafeValue(), kEvent2.GetUnsafeValue()}));
}

TEST(UkmConfigTest, MergeConfig) {
  auto generator = UkmEventHash::Generator();
  const UkmEventHash kEvent1 = generator.GenerateNextId();
  const UkmEventHash kEvent2 = generator.GenerateNextId();
  const UkmEventHash kEvent3 = generator.GenerateNextId();

  UkmConfig config1;
  config1.AddEvent(kEvent1,
                   {TestMetric(100), TestMetric(101), TestMetric(102)});
  config1.AddEvent(kEvent2, {TestMetric(100)});
  EXPECT_EQ(config1.metrics_for_event_for_testing(),
            UkmConfig::EventsToMetricsMap(
                {{kEvent1, {TestMetric(100), TestMetric(101), TestMetric(102)}},
                 {kEvent2, {TestMetric(100)}}}));

  UkmConfig config2;
  config2.AddEvent(kEvent1, {TestMetric(102)});
  config2.AddEvent(kEvent2, {TestMetric(100), TestMetric(102)});
  config2.AddEvent(kEvent3, {TestMetric(120), TestMetric(121)});
  EXPECT_EQ(config2.metrics_for_event_for_testing(),
            UkmConfig::EventsToMetricsMap(
                {{kEvent1, {TestMetric(102)}},
                 {kEvent2, {TestMetric(100), TestMetric(102)}},
                 {kEvent3, {TestMetric(120), TestMetric(121)}}}));

  EXPECT_EQ(config1.Merge(config2), UkmConfig::NEW_EVENT_ADDED);
  EXPECT_EQ(config1.metrics_for_event_for_testing(),
            UkmConfig::EventsToMetricsMap(
                {{kEvent1, {TestMetric(100), TestMetric(101), TestMetric(102)}},
                 {kEvent2, {TestMetric(100), TestMetric(102)}},
                 {kEvent3, {TestMetric(120), TestMetric(121)}}}));

  UkmConfig config3;
  config2.AddEvent(kEvent1, {TestMetric(150), TestMetric(151)});
  EXPECT_EQ(config1.Merge(config3), UkmConfig::NO_NEW_EVENT);
}

}  // namespace segmentation_platform
