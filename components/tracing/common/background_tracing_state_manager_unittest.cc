// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/background_tracing_state_manager.h"

#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/strings/pattern.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/tracing/common/pref_names.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

class BackgroundTracingStateManagerTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, false);
    pref_service_->SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
    tracing::RegisterPrefs(pref_service_->registry());
    tracing::BackgroundTracingStateManager::GetInstance()
        .SetPrefServiceForTesting(pref_service_.get());
  }

  void TearDown() override {
    tracing::BackgroundTracingStateManager::GetInstance().Reset();
  }

  std::string GetSessionStateJson() {
    const base::Value* state =
        pref_service_->GetDictionary(tracing::kBackgroundTracingSessionState);

    std::string json;
    EXPECT_TRUE(base::JSONWriter::Write(*state, &json));
    return json;
  }

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BackgroundTracingStateManagerTest, InitializeEmptyPrefs) {
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  EXPECT_EQ(GetSessionStateJson(), R"({"state":0,"upload_times":[]})");
}

TEST_F(BackgroundTracingStateManagerTest, InitializeInvalidState) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("state",
                 static_cast<int>(tracing::BackgroundTracingState::LAST) + 1);
  pref_service_->Set(tracing::kBackgroundTracingSessionState, std::move(dict));

  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  EXPECT_EQ(GetSessionStateJson(), R"({"state":0,"upload_times":[]})");
}

TEST_F(BackgroundTracingStateManagerTest, InitializeNoScenario) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("state", static_cast<int>(
                              tracing::BackgroundTracingState::NOT_ACTIVATED));
  base::Value upload_times(base::Value::Type::LIST);
  base::Value scenario(base::Value::Type::DICTIONARY);
  scenario.SetKey("time", base::TimeToValue(base::Time::Now()));
  upload_times.Append(std::move(scenario));
  dict.SetKey("upload_times", std::move(upload_times));
  pref_service_->Set(tracing::kBackgroundTracingSessionState, std::move(dict));

  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  EXPECT_EQ(GetSessionStateJson(), R"({"state":0,"upload_times":[]})");
}

TEST_F(BackgroundTracingStateManagerTest, InitializeValidPrefs) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("state", static_cast<int>(
                              tracing::BackgroundTracingState::NOT_ACTIVATED));
  base::Value upload_times(base::Value::Type::LIST);
  base::Value scenario(base::Value::Type::DICTIONARY);
  scenario.SetStringKey("scenario", "TestScenario");
  scenario.SetKey("time", base::TimeToValue(base::Time::Now()));
  upload_times.Append(std::move(scenario));
  dict.SetKey("upload_times", std::move(upload_times));
  pref_service_->Set(tracing::kBackgroundTracingSessionState, std::move(dict));

  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  EXPECT_TRUE(base::MatchPattern(
      GetSessionStateJson(),
      R"({"state":0,"upload_times":[{"scenario":"TestScenario","time":"*"}]})"))
      << "Actual: " << GetSessionStateJson();
  ;
}

TEST_F(BackgroundTracingStateManagerTest, SaveStateValidPrefs) {
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      {{"TestScenario", base::Time::Now()}},
      tracing::BackgroundTracingState::NOT_ACTIVATED);
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);

  EXPECT_TRUE(base::MatchPattern(
      GetSessionStateJson(),
      R"({"state":0,"upload_times":[{"scenario":"TestScenario","time":"*"}]})"))
      << "Actual: " << GetSessionStateJson();
  EXPECT_FALSE(tracing::BackgroundTracingStateManager::GetInstance()
                   .DidLastSessionEndUnexpectedly());
}

TEST_F(BackgroundTracingStateManagerTest, SessionEndedUnexpectedly) {
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      {}, tracing::BackgroundTracingState::STARTED);
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  EXPECT_TRUE(tracing::BackgroundTracingStateManager::GetInstance()
                  .DidLastSessionEndUnexpectedly());
}

TEST_F(BackgroundTracingStateManagerTest, NotUploadedRecently) {
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      {{"TestScenario", base::Time::Now() - base::Days(8)}},
      tracing::BackgroundTracingState::NOT_ACTIVATED);
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("scenario_name", "TestScenario");
  dict.SetStringKey("mode", "PREEMPTIVE_TRACING_MODE");
  dict.SetStringKey("custom_categories", "toplevel");
  base::Value rules_list(base::Value::Type::LIST);

  {
    base::Value rules_dict(base::Value::Type::DICTIONARY);
    rules_dict.SetStringKey("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.SetStringKey("trigger_name", "test");
    rules_list.Append(std::move(rules_dict));
  }

  dict.SetKey("configs", std::move(rules_list));
  std::unique_ptr<content::BackgroundTracingConfig> config(
      content::BackgroundTracingConfig::FromDict(std::move(dict)));

  EXPECT_FALSE(tracing::BackgroundTracingStateManager::GetInstance()
                   .DidRecentlyUploadForScenario(*config));
}

TEST_F(BackgroundTracingStateManagerTest, UploadedRecently) {
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      {{"TestScenario", base::Time::Now() - base::Days(1)}},
      tracing::BackgroundTracingState::NOT_ACTIVATED);
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("scenario_name", "TestScenario");
  dict.SetStringKey("mode", "PREEMPTIVE_TRACING_MODE");
  dict.SetStringKey("custom_categories", "toplevel");
  base::Value rules_list(base::Value::Type::LIST);

  {
    base::Value rules_dict(base::Value::Type::DICTIONARY);
    rules_dict.SetStringKey("rule", "MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED");
    rules_dict.SetStringKey("trigger_name", "test");
    rules_list.Append(std::move(rules_dict));
  }

  dict.SetKey("configs", std::move(rules_list));
  std::unique_ptr<content::BackgroundTracingConfig> config(
      content::BackgroundTracingConfig::FromDict(std::move(dict)));

  EXPECT_TRUE(tracing::BackgroundTracingStateManager::GetInstance()
                  .DidRecentlyUploadForScenario(*config));
}

TEST_F(BackgroundTracingStateManagerTest, NotifyTracingStarted) {
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  tracing::BackgroundTracingStateManager::GetInstance().NotifyTracingStarted();
  EXPECT_TRUE(base::MatchPattern(GetSessionStateJson(),
                                 R"({"state":1,"upload_times":[]})"))
      << "Actual: " << GetSessionStateJson();
}

TEST_F(BackgroundTracingStateManagerTest, NotifyFinalizationStarted) {
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  tracing::BackgroundTracingStateManager::GetInstance()
      .NotifyFinalizationStarted();
  EXPECT_TRUE(base::MatchPattern(GetSessionStateJson(),
                                 R"({"state":3,"upload_times":[]})"))
      << "Actual: " << GetSessionStateJson();
}

TEST_F(BackgroundTracingStateManagerTest, OnScenarioUploaded) {
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  tracing::BackgroundTracingStateManager::GetInstance()
      .NotifyFinalizationStarted();
  tracing::BackgroundTracingStateManager::GetInstance().OnScenarioUploaded(
      "TestScenario");
  EXPECT_TRUE(base::MatchPattern(
      GetSessionStateJson(),
      R"({"state":3,"upload_times":[{"scenario":"TestScenario","time":"*"}]})"))
      << "Actual: " << GetSessionStateJson();
}
}  // namespace tracing
