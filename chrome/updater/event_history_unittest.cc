// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/event_history.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;

TEST(EventHistoryTest, InstallStartEventMembers) {
  std::unique_ptr<InstallStartEvent> event =
      InstallStartEvent::Builder()
          .SetEventId("test-event-id")
          .SetAppId("test-app-id")
          .AddError({.category = 1, .code = 2, .extracode1 = 3})
          .Build();
  EXPECT_EQ(event->event_type(), "INSTALL");
  EXPECT_EQ(event->bound(), Event::Bound::kStart);
  EXPECT_EQ(event->event_id(), "test-event-id");
  EXPECT_EQ(event->app_id(), "test-app-id");
  ASSERT_THAT(event->errors(),
              ElementsAre(AllOf(Field(&Event::Error::category, Eq(1)),
                                Field(&Event::Error::code, Eq(2)),
                                Field(&Event::Error::extracode1, Eq(3)))));
}

TEST(EventHistoryTest, InstallEndEventMembers) {
  std::unique_ptr<InstallEndEvent> event = InstallEndEvent::Builder()
                                               .SetEventId("test-event-id")
                                               .SetVersion("1.2.3.4")
                                               .Build();
  EXPECT_EQ(event->event_type(), "INSTALL");
  EXPECT_EQ(event->bound(), Event::Bound::kEnd);
  EXPECT_EQ(event->event_id(), "test-event-id");
  EXPECT_EQ(event->version(), "1.2.3.4");
  EXPECT_THAT(event->errors(), IsEmpty());
}

TEST(EventHistoryTest, InstallStartEventBuilderReturnsNullptrOnMissingAppId) {
  EXPECT_EQ(InstallStartEvent::Builder().SetEventId("test-event-id").Build(),
            nullptr);
}

}  // namespace
}  // namespace updater
