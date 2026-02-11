// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/on_device_ai_settings_handler.h"

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using optimization_guide::model_execution::prefs::localstate::
    kOnDeviceAiUserSettingsEnabled;

class TestingOnDeviceAiSettingsHandler
    : public settings::OnDeviceAiSettingsHandler {
 public:
  using OnDeviceAiSettingsHandler::HandleGetOnDeviceAiEnabled;
  using OnDeviceAiSettingsHandler::HandleSetOnDeviceAiEnabled;
  using OnDeviceAiSettingsHandler::set_web_ui;
};
}  // namespace

namespace settings {

class OnDeviceAiSettingsHandlerTest : public testing::Test {
 public:
  OnDeviceAiSettingsHandlerTest() {
    handler_ = std::make_unique<TestingOnDeviceAiSettingsHandler>();
    handler_->set_web_ui(&test_web_ui_);
  }

  void SetUp() override {
    ASSERT_EQ(local_state(), g_browser_process->local_state());
    EXPECT_TRUE(test_web_ui()->call_data().empty());

    // Initialize the handler and allow Javascript.
    base::ListValue args;
    args.Append(1);  // Callback ID
    handler()->HandleGetOnDeviceAiEnabled(args);

    EXPECT_TRUE(handler()->IsJavascriptAllowed());
    EXPECT_EQ(1u, test_web_ui()->call_data().size());

    test_web_ui()->ClearTrackedCalls();
  }

  void TearDown() override { handler_.reset(); }

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }
  TestingOnDeviceAiSettingsHandler* handler() { return handler_.get(); }
  content::TestWebUI* test_web_ui() { return &test_web_ui_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestWebUI test_web_ui_;
  std::unique_ptr<TestingOnDeviceAiSettingsHandler> handler_;
};

TEST_F(OnDeviceAiSettingsHandlerTest, HandleSetOnDeviceAIEnabled) {
  // Ensure the pref is initially false for this test.
  local_state()->SetBoolean(kOnDeviceAiUserSettingsEnabled, false);

  base::ListValue args;
  args.Append(true);
  handler()->HandleSetOnDeviceAiEnabled(args);

  EXPECT_TRUE(local_state()->GetBoolean(kOnDeviceAiUserSettingsEnabled));
}

TEST_F(OnDeviceAiSettingsHandlerTest,
       HandleGetOnDeviceAiEnabled_PolicyDisabled) {
  using optimization_guide::model_execution::prefs::
      GenAILocalFoundationalModelEnterprisePolicySettings;
  local_state()->SetInteger(
      optimization_guide::model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(
          GenAILocalFoundationalModelEnterprisePolicySettings::kDisallowed));

  base::ListValue args;
  args.Append("callback_id");
  handler()->HandleGetOnDeviceAiEnabled(args);

  EXPECT_EQ(1u, test_web_ui()->call_data().size());
  const content::TestWebUI::CallData& call_data =
      *test_web_ui()->call_data()[0];
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("callback_id", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());  // success
  const base::DictValue& result = call_data.arg3()->GetDict();
  EXPECT_FALSE(result.FindBool("allowedByPolicy").value());
}

TEST_F(OnDeviceAiSettingsHandlerTest,
       PrefChangesNotifyPage_UserSettingEnabled) {
  // Toggle the pref.
  local_state()->SetBoolean(
      kOnDeviceAiUserSettingsEnabled,
      !local_state()->GetBoolean(kOnDeviceAiUserSettingsEnabled));
  EXPECT_EQ(1u, test_web_ui()->call_data().size());
  const content::TestWebUI::CallData& call_data =
      *test_web_ui()->call_data()[0];
  EXPECT_EQ("on-device-ai-enabled-changed", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetDict().FindBool("allowedByPolicy").value());

  test_web_ui()->ClearTrackedCalls();
  handler()->DisallowJavascript();

  // Toggle the pref again, while JavaScript is disabled.
  local_state()->SetBoolean(
      kOnDeviceAiUserSettingsEnabled,
      !local_state()->GetBoolean(kOnDeviceAiUserSettingsEnabled));
  EXPECT_TRUE(test_web_ui()->call_data().empty());
}

TEST_F(OnDeviceAiSettingsHandlerTest, PrefChangesNotifyPage_PolicyDisabled) {
  using optimization_guide::model_execution::prefs::
      GenAILocalFoundationalModelEnterprisePolicySettings;
  local_state()->SetInteger(
      optimization_guide::model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(
          GenAILocalFoundationalModelEnterprisePolicySettings::kDisallowed));

  // Toggle the pref.
  local_state()->SetBoolean(
      kOnDeviceAiUserSettingsEnabled,
      !local_state()->GetBoolean(kOnDeviceAiUserSettingsEnabled));
  EXPECT_EQ(1u, test_web_ui()->call_data().size());
  const content::TestWebUI::CallData& call_data =
      *test_web_ui()->call_data()[0];
  EXPECT_EQ("on-device-ai-enabled-changed", call_data.arg1()->GetString());
  EXPECT_FALSE(call_data.arg2()->GetDict().FindBool("allowedByPolicy").value());
}

}  // namespace settings
