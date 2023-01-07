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

namespace metrics {
namespace structured {

namespace {
void ExpectEventsEqual(const Event& expected, const Event& actual) {
  EXPECT_THAT(expected,
              AllOf(Property(&Event::project_name, Eq(actual.project_name())),
                    Property(&Event::project_name, Eq(actual.project_name()))));

  for (const auto& expected_pair : expected.metric_values()) {
    auto actual_pair = actual.metric_values().find(expected_pair.first);
    ASSERT_FALSE(actual_pair == actual.metric_values().end());
    EXPECT_EQ(expected_pair.second, actual_pair->second);
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

}  // namespace structured
}  // namespace metrics
