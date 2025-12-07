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
#include "chrome/updater/constants.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
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

MATCHER(IsTimeValue, "") {
  return base::ValueToTime(&arg).has_value();
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

TEST_F(EventHistoryTest, InstallStartEventBuilderReturnsNulloptOnMissingAppId) {
  EXPECT_EQ(InstallStartEvent().SetEventId("test-event-id").Build(),
            std::nullopt);
}

TEST_F(EventHistoryTest, UninstallStartEventToDict) {
  std::optional<base::Value::Dict> event =
      UninstallStartEvent()
          .SetEventId("test-event-id")
          .SetAppId("test-app-id")
          .SetVersion("1.2.3.4")
          .SetReason(UninstallPingReason::kNoAppsRemain)
          .AddError({.category = 1, .code = 2, .extracode1 = 3})
          .Build();

  base::Value::List expected_errors =
      base::Value::List().Append(base::Value::Dict()
                                     .Set("category", 1)
                                     .Set("code", 2)
                                     .Set("extracode1", 3));
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("UNINSTALL")},
                          {"bound", ValueIs("START")},
                          {"eventId", ValueIs("test-event-id")},
                          {"appId", ValueIs("test-app-id")},
                          {"version", ValueIs("1.2.3.4")},
                          {"reason", ValueIs("NO_APPS_REMAIN")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                          {"errors", Eq(std::cref(expected_errors))},
                      })));
}

TEST_F(EventHistoryTest, UninstallEndEventToDict) {
  std::optional<base::Value::Dict> event =
      UninstallEndEvent().SetEventId("test-event-id").Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("UNINSTALL")},
                          {"bound", ValueIs("END")},
                          {"eventId", ValueIs("test-event-id")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest,
       UninstallStartEventBuilderReturnsNulloptOnMissingFields) {
  EXPECT_EQ(UninstallStartEvent().SetEventId("test-event-id").Build(),
            std::nullopt);
  EXPECT_EQ(UninstallStartEvent()
                .SetEventId("test-event-id")
                .SetAppId("test-app-id")
                .Build(),
            std::nullopt);
  EXPECT_EQ(UninstallStartEvent()
                .SetEventId("test-event-id")
                .SetAppId("test-app-id")
                .SetVersion("1.2.3.4")
                .Build(),
            std::nullopt);
}

TEST_F(EventHistoryTest, QualifyStartEventToDict) {
  std::optional<base::Value::Dict> event =
      QualifyStartEvent().SetEventId("test-event-id").Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("QUALIFY")},
                          {"bound", ValueIs("START")},
                          {"eventId", ValueIs("test-event-id")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, QualifyEndEventToDict) {
  std::optional<base::Value::Dict> event =
      QualifyEndEvent().SetEventId("test-event-id").SetQualified(true).Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("QUALIFY")},
                          {"bound", ValueIs("END")},
                          {"eventId", ValueIs("test-event-id")},
                          {"qualified", ValueIs(true)},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, ActivateStartEventToDict) {
  std::optional<base::Value::Dict> event =
      ActivateStartEvent().SetEventId("test-event-id").Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("ACTIVATE")},
                          {"bound", ValueIs("START")},
                          {"eventId", ValueIs("test-event-id")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, ActivateEndEventToDict) {
  std::optional<base::Value::Dict> event =
      ActivateEndEvent().SetEventId("test-event-id").SetActivated(true).Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("ACTIVATE")},
                          {"bound", ValueIs("END")},
                          {"eventId", ValueIs("test-event-id")},
                          {"activated", ValueIs(true)},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, PersistedDataEventToDict) {
  base::Time last_checked;
  ASSERT_TRUE(base::Time::FromString("2 Jan 2024 12:00:00 GMT", &last_checked));
  base::Time last_started;
  ASSERT_TRUE(base::Time::FromString("1 Jan 2024 12:00:00 GMT", &last_started));
  PersistedDataEvent::RegisteredApp app1;
  app1.app_id = "app1";
  app1.version = "1.0";
  app1.cohort = "cohort1";
  app1.brand_code = "brand1";
  PersistedDataEvent::RegisteredApp app2;
  app2.app_id = "app2";
  app2.version = "2.0";
  app2.brand_code = "brand2";
  std::optional<base::Value::Dict> event = PersistedDataEvent()
                                               .SetEventId("test-event-id")
                                               .SetEulaRequired(true)
                                               .SetLastChecked(last_checked)
                                               .SetLastStarted(last_started)
                                               .AddRegisteredApp(app1)
                                               .AddRegisteredApp(app2)
                                               .Build();

  base::Value::List expected_apps;
  expected_apps.Append(base::Value::Dict()
                           .Set("appId", "app1")
                           .Set("version", "1.0")
                           .Set("cohort", "cohort1")
                           .Set("brandCode", "brand1"));
  expected_apps.Append(base::Value::Dict()
                           .Set("appId", "app2")
                           .Set("version", "2.0")
                           .Set("brandCode", "brand2"));

  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("PERSISTED_DATA")},
                          {"bound", ValueIs("INSTANT")},
                          {"eventId", ValueIs("test-event-id")},
                          {"eulaRequired", ValueIs(true)},
                          {"lastChecked", IsTimeValue()},
                          {"lastStarted", IsTimeValue()},
                          {"registeredApps", Eq(std::cref(expected_apps))},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, PostRequestStartEventToDict) {
  std::optional<base::Value::Dict> event = PostRequestStartEvent()
                                               .SetEventId("test-event-id")
                                               .SetRequest("test-request")
                                               .Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("POST_REQUEST")},
                          {"bound", ValueIs("START")},
                          {"eventId", ValueIs("test-event-id")},
                          {"request", ValueIs("test-request")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, PostRequestEndEventToDict) {
  std::optional<base::Value::Dict> event = PostRequestEndEvent()
                                               .SetEventId("test-event-id")
                                               .SetResponse("test-response")
                                               .Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("POST_REQUEST")},
                          {"bound", ValueIs("END")},
                          {"eventId", ValueIs("test-event-id")},
                          {"response", ValueIs("test-response")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest,
       PostRequestStartEventBuilderReturnsNulloptOnMissingRequest) {
  EXPECT_EQ(PostRequestStartEvent().SetEventId("test-event-id").Build(),
            std::nullopt);
}

TEST_F(EventHistoryTest, LoadPolicyStartEventToDict) {
  std::optional<base::Value::Dict> event =
      LoadPolicyStartEvent().SetEventId("test-event-id").Build();
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("LOAD_POLICY")},
                          {"bound", ValueIs("START")},
                          {"eventId", ValueIs("test-event-id")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, LoadPolicyEndEventToDict) {
  const base::Value::Dict fake_policy_set =
      base::Value::Dict().Set("stub_policy_set", true);
  std::optional<base::Value::Dict> event = LoadPolicyEndEvent()
                                               .SetEventId("test-event-id")
                                               .SetPolicySet(fake_policy_set)
                                               .Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("LOAD_POLICY")},
                          {"bound", ValueIs("END")},
                          {"eventId", ValueIs("test-event-id")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                          {"policySet", Eq(std::cref(fake_policy_set))},
                      })));
}

TEST_F(EventHistoryTest,
       LoadPolicyEndEventBuilderReturnsNulloptOnMissingPolicySet) {
  EXPECT_EQ(LoadPolicyEndEvent().SetEventId("test-event-id").Build(),
            std::nullopt);
}

TEST_F(EventHistoryTest, UpdateStartEventToDict) {
  std::optional<base::Value::Dict> event =
      UpdateStartEvent()
          .SetEventId("test-event-id")
          .SetAppId("test-app-id")
          .SetPriority(UpdateService::Priority::kForeground)
          .Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("UPDATE")},
                          {"bound", ValueIs("START")},
                          {"eventId", ValueIs("test-event-id")},
                          {"appId", ValueIs("test-app-id")},
                          {"priority", ValueIs("FOREGROUND")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, UpdateStartEventNoOptionalFieldsToDict) {
  std::optional<base::Value::Dict> event =
      UpdateStartEvent().SetEventId("test-event-id").Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("UPDATE")},
                          {"bound", ValueIs("START")},
                          {"eventId", ValueIs("test-event-id")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, UpdateEndEventToDict) {
  std::optional<base::Value::Dict> event =
      UpdateEndEvent()
          .SetEventId("test-event-id")
          .SetOutcome(UpdateService::UpdateState::State::kUpdateError)
          .SetNextVersion("1.2.3.4")
          .Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("UPDATE")},
                          {"bound", ValueIs("END")},
                          {"eventId", ValueIs("test-event-id")},
                          {"outcome", ValueIs("UPDATE_ERROR")},
                          {"nextVersion", ValueIs("1.2.3.4")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, UpdateEndEventNoOptionalFieldsToDict) {
  std::optional<base::Value::Dict> event =
      UpdateEndEvent().SetEventId("test-event-id").Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("UPDATE")},
                          {"bound", ValueIs("END")},
                          {"eventId", ValueIs("test-event-id")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, UpdaterProcessStartEventToDict) {
  base::Time timestamp;
  ASSERT_TRUE(base::Time::FromString("1 Jan 2024 12:00:00 GMT", &timestamp));
  std::optional<base::Value::Dict> event =
      UpdaterProcessStartEvent()
          .SetEventId("test-event-id")
          .SetCommandLine("test-command-line")
          .SetTimestamp(timestamp)
          .SetUpdaterVersion("1.2.3.4")
          .SetScope(UpdaterScope::kSystem)
          .SetOsPlatform("test-platform")
          .SetOsVersion("10.0")
          .SetOsArchitecture("x86_64")
          .SetUpdaterArchitecture("x86")
          .SetParentPid(123)
          .Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("UPDATER_PROCESS")},
                          {"bound", ValueIs("START")},
                          {"eventId", ValueIs("test-event-id")},
                          {"commandLine", ValueIs("test-command-line")},
                          {"timestamp", IsTimeValue()},
                          {"updaterVersion", ValueIs("1.2.3.4")},
                          {"scope", ValueIs("SYSTEM")},
                          {"osPlatform", ValueIs("test-platform")},
                          {"osVersion", ValueIs("10.0")},
                          {"osArchitecture", ValueIs("x86_64")},
                          {"updaterArchitecture", ValueIs("x86")},
                          {"parentPid", ValueIs(123)},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, UpdaterProcessEndEventToDict) {
  std::optional<base::Value::Dict> event = UpdaterProcessEndEvent()
                                               .SetEventId("test-event-id")
                                               .SetExitCode(1)
                                               .Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("UPDATER_PROCESS")},
                          {"bound", ValueIs("END")},
                          {"eventId", ValueIs("test-event-id")},
                          {"exitCode", ValueIs(1)},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, AppCommandStartEventToDict) {
  std::optional<base::Value::Dict> event =
      AppCommandStartEvent()
          .SetEventId("test-event-id")
          .SetAppId("test-app-id")
          .SetCommandLine("test-command-line")
          .Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("APP_COMMAND")},
                          {"bound", ValueIs("START")},
                          {"eventId", ValueIs("test-event-id")},
                          {"appId", ValueIs("test-app-id")},
                          {"commandLine", ValueIs("test-command-line")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest, AppCommandEndEventToDict) {
  std::optional<base::Value::Dict> event = AppCommandEndEvent()
                                               .SetEventId("test-event-id")
                                               .SetExitCode(1)
                                               .SetOutput("test-output")
                                               .Build();
  ASSERT_TRUE(event);
  EXPECT_THAT(*event, DictHasFields(ExpectedFields({
                          {"eventType", ValueIs("APP_COMMAND")},
                          {"bound", ValueIs("END")},
                          {"eventId", ValueIs("test-event-id")},
                          {"exitCode", ValueIs(1)},
                          {"output", ValueIs("test-output")},
                          {"deviceUptime", IsPositiveTimeDeltaValue()},
                          {"pid", ValueIs(GetCurrentPid())},
                          {"processToken", ValueIs(GetProcessToken())},
                      })));
}

TEST_F(EventHistoryTest,
       AppCommandStartEventBuilderReturnsNulloptOnMissingAppId) {
  EXPECT_EQ(AppCommandStartEvent().SetEventId("test-event-id").Build(),
            std::nullopt);
}

}  // namespace
}  // namespace updater
