// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/event_history.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/strings/string_split.h"
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

class EventHistoryTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    log_path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("event.log"));
    InitHistoryLogging(log_path_, 1024);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath log_path_;
};

TEST_F(EventHistoryTest, ErrorToDict) {
  HistoryEventError error{.category = 1, .code = 2, .extracode1 = 3};
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

TEST_F(EventHistoryTest, Write) {
  InstallStartEvent()
      .SetEventId("test-event-id-1")
      .SetAppId("test-app-id-1")
      .AddError({.category = 1, .code = 2, .extracode1 = 3})
      .Write();

  InstallEndEvent().SetEventId("test-event-id-2").SetVersion("1.2.3.4").Write();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(log_path_, &contents));
  std::vector<std::string> lines = base::SplitString(
      contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  ASSERT_EQ(lines.size(), 2U);

  std::optional<base::Value> json1 =
      base::JSONReader::Read(lines[0], base::JSON_PARSE_RFC);
  ASSERT_TRUE(json1);
  ASSERT_TRUE(json1->is_dict());

  std::optional<base::Value> json2 =
      base::JSONReader::Read(lines[1], base::JSON_PARSE_RFC);
  ASSERT_TRUE(json2);
  ASSERT_TRUE(json2->is_dict());

  base::Value::List expected_errors =
      base::Value::List().Append(base::Value::Dict()
                                     .Set("category", 1)
                                     .Set("code", 2)
                                     .Set("extracode1", 3));
  EXPECT_THAT(json1->GetDict(),
              DictHasFields(ExpectedFields({
                  {"eventType", ValueIs("INSTALL")},
                  {"bound", ValueIs("START")},
                  {"eventId", ValueIs("test-event-id-1")},
                  {"appId", ValueIs("test-app-id-1")},
                  {"deviceUptime", IsPositiveTimeDeltaValue()},
                  {"pid", ValueIs(GetCurrentPid())},
                  {"processToken", ValueIs(GetProcessToken())},
                  {"errors", Eq(std::cref(expected_errors))},
              })));

  EXPECT_THAT(json2->GetDict(),
              DictHasFields(ExpectedFields({
                  {"eventType", ValueIs("INSTALL")},
                  {"bound", ValueIs("END")},
                  {"eventId", ValueIs("test-event-id-2")},
                  {"version", ValueIs("1.2.3.4")},
                  {"deviceUptime", IsPositiveTimeDeltaValue()},
                  {"pid", ValueIs(GetCurrentPid())},
                  {"processToken", ValueIs(GetProcessToken())},
              })));

  // Reset logging state.
  InitHistoryLogging(base::FilePath(), 0);
}

TEST_F(EventHistoryTest, Rotate) {
  // Write a bunch of events to force the log to rotate.
  for (int i = 0; i < 10; ++i) {
    InstallStartEvent()
        .SetEventId("test-event-id-1")
        .SetAppId("test-app-id-1")
        .Write();
  }
  std::optional<int64_t> log_size = base::GetFileSize(log_path_);
  EXPECT_GT(log_size, 0);

  // Re-init to trigger rotation.
  InitHistoryLogging(log_path_, 10);

  EXPECT_TRUE(base::PathExists(log_path_));
  EXPECT_EQ(base::GetFileSize(log_path_), 0);
  base::FilePath rotated_log_path =
      log_path_.AddExtension(FILE_PATH_LITERAL("old"));
  EXPECT_TRUE(base::PathExists(rotated_log_path));
  EXPECT_EQ(base::GetFileSize(rotated_log_path), log_size);
}

TEST_F(EventHistoryTest, InstallStartEventToDict) {
  std::optional<base::Value::Dict> event =
      InstallStartEvent()
          .SetEventId("test-event-id")
          .SetAppId("test-app-id")
          .AddError({.category = 1, .code = 2, .extracode1 = 3})
          .Build();

  base::Value::List expected_errors =
      base::Value::List().Append(base::Value::Dict()
                                     .Set("category", 1)
                                     .Set("code", 2)
                                     .Set("extracode1", 3));
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("INSTALL")},
                          {"bound", ValueIs("START")},
                          {"eventId", ValueIs("test-event-id")},
                          {"appId", ValueIs("test-app-id")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                          {"errors", Eq(std::cref(expected_errors))},
                      })));
}

TEST_F(EventHistoryTest, InstallEndEventToDict) {
  std::optional<base::Value::Dict> event = InstallEndEvent()
                                               .SetEventId("test-event-id")
                                               .SetVersion("1.2.3.4")
                                               .Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("INSTALL")},
                          {"bound", ValueIs("END")},
                          {"eventId", ValueIs("test-event-id")},
                          {"version", ValueIs("1.2.3.4")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, InstallEndEventNoVersionToDict) {
  std::optional<base::Value::Dict> event =
      InstallEndEvent().SetEventId("test-event-id").Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("INSTALL")},
                          {"bound", ValueIs("END")},
                          {"eventId", ValueIs("test-event-id")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, InstallStartEventBuilderReturnsNullptrOnMissingAppId) {
  EXPECT_EQ(InstallStartEvent().SetEventId("test-event-id").Build(),
            std::nullopt);
}

}  // namespace
}  // namespace updater
