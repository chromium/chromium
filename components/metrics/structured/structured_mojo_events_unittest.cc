// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_mojo_events.h"

#include "components/metrics/structured/event.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/structured_metrics_validator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace structured {

// This test ensures that the generated code compiles and is correct.
//
// TODO(crbug.com/1249222): Because the feature is still WIP, the tryjobs may
// omit during linking as there are no dependencies. This should be moved to be
// implicitly tested in recording delegate unittests. Remove this once feature
// is fully launched.
TEST(StructuredMojoEventsTest, CreatesMojoEventFromXmlCorrectly) {
  events::v2::test_project_one::TestEventOne event1;
  event1.SetTestMetricOne("hash").SetTestMetricTwo(1);

  events::v2::test_project_two::TestEventThree event2;
  event2.SetTestMetricFour("hash");

  // The generated classes should yield valid Events to be recorded.
  ASSERT_TRUE(EventBase::FromEvent(event1).has_value());
  ASSERT_TRUE(EventBase::FromEvent(event2).has_value());
}

}  // namespace structured
}  // namespace metrics
