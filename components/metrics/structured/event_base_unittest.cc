// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/event_base.h"

#include <cstdio>
#include <string>

#include "base/values.h"
#include "components/metrics/structured/enums.h"
#include "components/metrics/structured/event.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace structured {

namespace {

using testing::AllOf;
using testing::Eq;
using testing::Property;
using testing::UnorderedElementsAre;

// These project, event, and metric names are used for testing. This test metric
// is defined in //tools/metrics/structured/structured.xml. The constants below
// are from the project defined in structured.xml.
//
// - project: TestProjectOne
//   - event: TestEventOne
//     - metric: TestMetricOne
//     - metric: TestMetricTwo

constexpr uint64_t kProjectOneHash = UINT64_C(16881314472396226433);
constexpr char kProjectOneName[] = "TestProjectOne";

constexpr uint64_t kEventOneHash = UINT64_C(13593049295042080097);
constexpr char kEventOneName[] = "TestEventOne";

constexpr uint64_t kMetricOneHash = UINT64_C(637929385654885975);
constexpr char kMetricOneName[] = "TestMetricOne";

constexpr uint64_t kMetricTwoHash = UINT64_C(14083999144141567134);
constexpr char kMetricTwoName[] = "TestMetricTwo";

constexpr IdScope kProjectOneIdScope = IdScope::kPerProfile;
constexpr IdType kProjectOneIdType = IdType::kProjectId;
constexpr EventType kProjectOneEventType =
    EventType::StructuredEventProto_EventType_REGULAR;

}  // namespace

TEST(EventBaseTest, FromEventConvertsValidEventToEventBase) {
  const std::string kHmacValue = "hmac-value";
  const std::string kLongValue = "12345";  // No long in base::Value.

  Event test_event(kProjectOneName, kEventOneName);
  test_event.AddMetric(kMetricOneName, Event::MetricType::kHmac,
                       base::Value(kHmacValue));
  test_event.AddMetric(kMetricTwoName, Event::MetricType::kLong,
                       base::Value(kLongValue));

  EventBase::Metric expected_metric1(kMetricOneHash,
                                     EventBase::MetricType::kHmac);
  expected_metric1.hmac_value = kHmacValue;
  EventBase::Metric expected_metric2(kMetricTwoHash,
                                     EventBase::MetricType::kInt);
  expected_metric2.int_value = 12345;  // kLongValue.

  auto event_base = EventBase::FromEvent(test_event);
  ASSERT_TRUE(event_base.has_value());

  EXPECT_THAT(
      event_base.value(),
      AllOf(
          Property(&EventBase::project_name_hash, Eq(kProjectOneHash)),
          Property(&EventBase::name_hash, Eq(kEventOneHash)),
          Property(&EventBase::id_type, Eq(kProjectOneIdType)),
          Property(&EventBase::id_scope, Eq(kProjectOneIdScope)),
          Property(&EventBase::event_type, Eq(kProjectOneEventType)),
          Property(&EventBase::metrics,
                   UnorderedElementsAre(expected_metric1, expected_metric2))));
}

TEST(EventBaseTest, FromEventWithInvalidMetricNameIsEmpty) {
  const std::string kHmacValue = "hmac-value";

  Event test_event(kProjectOneName, kEventOneName);
  test_event.AddMetric("fake-metric", Event::MetricType::kHmac,
                       base::Value(kHmacValue));

  auto event_base = EventBase::FromEvent(test_event);
  ASSERT_FALSE(event_base.has_value());
}

TEST(EventBaseTest, FromEventWithInvalidEventNameIsEmpty) {
  const std::string kHmacValue = "hmac-value";

  Event test_event(kProjectOneName, "fake-event-name");
  test_event.AddMetric(kMetricOneName, Event::MetricType::kHmac,
                       base::Value(kHmacValue));

  auto event_base = EventBase::FromEvent(test_event);
  ASSERT_FALSE(event_base.has_value());
}

TEST(EventBaseTest, FromEventWithInvalidMetricTypeIsEmpty) {
  const double kDoubleValue = 123.45;

  Event test_event(kProjectOneName, kEventOneName);
  // Metric should be of type kHmac.
  test_event.AddMetric(kMetricOneName, Event::MetricType::kDouble,
                       base::Value(kDoubleValue));

  auto event_base = EventBase::FromEvent(test_event);
  ASSERT_FALSE(event_base.has_value());
}

}  // namespace structured
}  // namespace metrics
