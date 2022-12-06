// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/values.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/mojom/event.mojom.h"
#include "components/metrics/structured/mojom/event_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AllOf;
using testing::ContainerEq;
using testing::Eq;
using testing::Property;

namespace metrics::structured {

namespace {
void ExpectEventsEqual(const Event& expected, const Event& actual) {
  EXPECT_THAT(expected,
              AllOf(Property(&Event::project_name, Eq(actual.project_name())),
                    Property(&Event::event_name, Eq(actual.event_name())),
                    Property(&Event::IsEventSequenceType,
                             Eq(actual.IsEventSequenceType()))));

  for (const auto& expected_pair : expected.metric_values()) {
    auto actual_pair = actual.metric_values().find(expected_pair.first);
    ASSERT_FALSE(actual_pair == actual.metric_values().end());
    EXPECT_EQ(expected_pair.second, actual_pair->second);
  }

  // Check uptimes only if event is part of a sequence.
  if (expected.IsEventSequenceType() && actual.IsEventSequenceType()) {
    EXPECT_EQ(expected.recorded_time_since_boot(),
              actual.recorded_time_since_boot());
  }
}

}  // namespace

TEST(EventStructTraitsTest, ValidEvent) {
  const std::string kProjectName = "project_name";
  const std::string kEventName = "event_name";

  Event test_event(kProjectName, kEventName);

  ASSERT_TRUE(test_event.AddMetric("hmac", Event::MetricType::kHmac,
                                   base::Value("1234")));
  ASSERT_TRUE(test_event.AddMetric("long", Event::MetricType::kLong,
                                   base::Value("123456789")));
  ASSERT_TRUE(
      test_event.AddMetric("int", Event::MetricType::kInt, base::Value(123)));
  ASSERT_TRUE(test_event.AddMetric("double", Event::MetricType::kDouble,
                                   base::Value(123.4)));
  ASSERT_TRUE(test_event.AddMetric("string", Event::MetricType::kRawString,
                                   base::Value("string")));
  ASSERT_TRUE(test_event.AddMetric("boolean", Event::MetricType::kBoolean,
                                   base::Value(false)));

  // Doesn't matter what the values of the string are.
  Event output("", "");
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::Event>(test_event, output));
  ExpectEventsEqual(test_event, output);
}

TEST(EventStructTraitsTest, EventWithUptime) {
  Event sequence_event("project_name", "event_name", true);
  sequence_event.SetRecordedTimeSinceBoot(base::Microseconds(500));

  ASSERT_TRUE(sequence_event.AddMetric("double", Event::MetricType::kDouble,
                                       base::Value(1.0)));

  // Doesn't matter what the values of the string are.
  Event test_output("", "");
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Event>(sequence_event,
                                                                test_output));
  ExpectEventsEqual(sequence_event, test_output);
}

}  // namespace metrics::structured
