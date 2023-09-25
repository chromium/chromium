// Copyright 2022 The Chromium Authors
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
    const base::Value::Dict& state =
        pref_service_->GetDict(tracing::kBackgroundTracingSessionState);

    std::string json;
    EXPECT_TRUE(base::JSONWriter::Write(state, &json));
    return json;
  }

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BackgroundTracingStateManagerTest, InitializeEmptyPrefs) {
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  EXPECT_EQ(GetSessionStateJson(), R"({"state":0})");
}

TEST_F(BackgroundTracingStateManagerTest, InitializeInvalidState) {
  base::Value::Dict dict;
  dict.Set("state",
           static_cast<int>(tracing::BackgroundTracingState::LAST) + 1);
  pref_service_->Set(tracing::kBackgroundTracingSessionState,
                     base::Value(std::move(dict)));

  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  EXPECT_EQ(GetSessionStateJson(), R"({"state":0})");
}

TEST_F(BackgroundTracingStateManagerTest, InitializeNoScenario) {
  base::Value::Dict dict;
  dict.Set("state",
           static_cast<int>(tracing::BackgroundTracingState::NOT_ACTIVATED));
  base::Value::Dict scenario;
  scenario.Set("time", base::TimeToValue(base::Time::Now()));
  pref_service_->Set(tracing::kBackgroundTracingSessionState,
                     base::Value(std::move(dict)));

  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  EXPECT_EQ(GetSessionStateJson(), R"({"state":0})");
}

TEST_F(BackgroundTracingStateManagerTest, InitializeValidPrefs) {
  base::Value::Dict dict;
  dict.Set("state",
           static_cast<int>(tracing::BackgroundTracingState::NOT_ACTIVATED));
  base::Value::Dict scenario;
  scenario.Set("scenario", "TestScenario");
  scenario.Set("time", base::TimeToValue(base::Time::Now()));
  pref_service_->Set(tracing::kBackgroundTracingSessionState,
                     base::Value(std::move(dict)));

  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  EXPECT_TRUE(base::MatchPattern(GetSessionStateJson(), R"({"state":0})"))
      << "Actual: " << GetSessionStateJson();
  ;
}

TEST_F(BackgroundTracingStateManagerTest, SaveStateValidPrefs) {
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      tracing::BackgroundTracingState::NOT_ACTIVATED);
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);

  EXPECT_TRUE(base::MatchPattern(GetSessionStateJson(), R"({"state":0})"))
      << "Actual: " << GetSessionStateJson();
  EXPECT_FALSE(tracing::BackgroundTracingStateManager::GetInstance()
                   .DidLastSessionEndUnexpectedly());
}

TEST_F(BackgroundTracingStateManagerTest, SessionEndedUnexpectedly) {
  tracing::BackgroundTracingStateManager::GetInstance().SaveState(
      tracing::BackgroundTracingState::STARTED);
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  EXPECT_TRUE(tracing::BackgroundTracingStateManager::GetInstance()
                  .DidLastSessionEndUnexpectedly());
}

TEST_F(BackgroundTracingStateManagerTest, OnTracingStarted) {
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  tracing::BackgroundTracingStateManager::GetInstance().OnTracingStarted();
  EXPECT_TRUE(base::MatchPattern(GetSessionStateJson(), R"({"state":1})"))
      << "Actual: " << GetSessionStateJson();
}

TEST_F(BackgroundTracingStateManagerTest, OnTracingStopped) {
  tracing::BackgroundTracingStateManager::GetInstance().Initialize(nullptr);
  tracing::BackgroundTracingStateManager::GetInstance().OnTracingStopped();
  EXPECT_TRUE(base::MatchPattern(GetSessionStateJson(), R"({"state":3})"))
      << "Actual: " << GetSessionStateJson();
}

}  // namespace tracing
