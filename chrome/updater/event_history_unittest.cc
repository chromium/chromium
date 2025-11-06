// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/event_history.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Property;

MATCHER_P(ValueIs, expected_value, "") {
  return arg == base::Value(expected_value);
}

using ExpectedFields =
    std::vector<std::pair<std::string, testing::Matcher<const base::Value&>>>;

MATCHER_P(DictHasFields, expected_fields, "") {
  const base::Value::Dict& dict = arg;
  if (dict.size() != expected_fields.size()) {
    *result_listener << "has " << dict.size() << " fields, but "
                     << expected_fields.size() << " were expected.";
    return false;
  }
  for (const auto& [key, value_matcher] : expected_fields) {
    const base::Value* value = dict.Find(key);
    if (!value) {
      *result_listener << "is missing field '" << key << "'";
      return false;
    }
    if (!testing::ExplainMatchResult(value_matcher, *value, result_listener)) {
      *result_listener << " for field '" << key << "'";
      return false;
    }
  }
  return true;
}

MATCHER(IsPositiveTimeDeltaValue, "") {
  const std::optional<base::TimeDelta> td = base::ValueToTimeDelta(&arg);
  return td.has_value() && td->is_positive();
}

TEST(EventHistoryTest, ErrorToDict) {
  Event::Error error{.category = 1, .code = 2, .extracode1 = 3};
  EXPECT_THAT(error.ToDict(), DictHasFields(ExpectedFields({
                                  {"category", ValueIs(1)},
                                  {"code", ValueIs(2)},
                                  {"extracode1", ValueIs(3)},
                              })));
}

// Retrieves the id of the current process as an integer. This is particularly
// useful for the `ValueIs` matcher on Windows as the constructor call for
// base::Value with a DWORD is ambiguous.
int GetCurrentPid() {
  return base::Process::Current().Pid();
}

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

TEST(EventHistoryTest, InstallStartEventToDict) {
  std::unique_ptr<InstallStartEvent> event =
      InstallStartEvent::Builder()
          .SetEventId("test-event-id")
          .SetAppId("test-app-id")
          .AddError({.category = 1, .code = 2, .extracode1 = 3})
          .Build();

  base::Value::List expected_errors =
      base::Value::List().Append(base::Value::Dict()
                                     .Set("category", 1)
                                     .Set("code", 2)
                                     .Set("extracode1", 3));
  EXPECT_THAT(event->ToDict(),
              DictHasFields(ExpectedFields({
                  {"eventType", ValueIs("INSTALL")},
                  {"bound", ValueIs("START")},
                  {"eventId", ValueIs("test-event-id")},
                  {"appId", ValueIs("test-app-id")},
                  {"deviceUptime", IsPositiveTimeDeltaValue()},
                  {"pid", ValueIs(GetCurrentPid())},
                  {"processToken", ValueIs(event->process_token())},
                  {"errors", Eq(std::cref(expected_errors))},
              })));
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

TEST(EventHistoryTest, InstallEndEventToDict) {
  std::unique_ptr<InstallEndEvent> event = InstallEndEvent::Builder()
                                               .SetEventId("test-event-id")
                                               .SetVersion("1.2.3.4")
                                               .Build();
  EXPECT_THAT(event->ToDict(),
              DictHasFields(ExpectedFields({
                  {"eventType", ValueIs("INSTALL")},
                  {"bound", ValueIs("END")},
                  {"eventId", ValueIs("test-event-id")},
                  {"version", ValueIs("1.2.3.4")},
                  {"deviceUptime", IsPositiveTimeDeltaValue()},
                  {"pid", ValueIs(GetCurrentPid())},
                  {"processToken", ValueIs(event->process_token())},
              })));
}

TEST(EventHistoryTest, InstallEndEventNoVersionToDict) {
  std::unique_ptr<InstallEndEvent> event =
      InstallEndEvent::Builder().SetEventId("test-event-id").Build();
  EXPECT_THAT(event->ToDict(),
              DictHasFields(ExpectedFields({
                  {"eventType", ValueIs("INSTALL")},
                  {"bound", ValueIs("END")},
                  {"eventId", ValueIs("test-event-id")},
                  {"deviceUptime", IsPositiveTimeDeltaValue()},
                  {"pid", ValueIs(GetCurrentPid())},
                  {"processToken", ValueIs(event->process_token())},
              })));
}

TEST(EventHistoryTest, InstallStartEventBuilderReturnsNullptrOnMissingAppId) {
  EXPECT_EQ(InstallStartEvent::Builder().SetEventId("test-event-id").Build(),
            nullptr);
}

}  // namespace
}  // namespace updater
