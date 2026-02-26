// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/settings/settings_default_browser_handler.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/default_browser/test_support/fake_default_browser_setter.h"
#include "chrome/browser/default_browser/test_support/fake_shell_delegate.h"
#include "chrome/browser/global_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

namespace {

const char kRequestDefaultBrowserStateCallback[] =
    "requestDefaultBrowserStateCallback";
const char kRequestUserValueStringsFeatureStateCallback[] =
    "requestUserValueStringsFeatureStateCallback";

}  // namespace

namespace settings {

class TestingDefaultBrowserHandler : public DefaultBrowserHandler {
 public:
  TestingDefaultBrowserHandler() = default;
  TestingDefaultBrowserHandler& operator=(const TestingDefaultBrowserHandler&) =
      delete;
  TestingDefaultBrowserHandler(const TestingDefaultBrowserHandler&) = delete;

  void OnJavascriptAllowed() override {
    DefaultBrowserHandler::OnJavascriptAllowed();
    // Override the controller with a mock.
    default_browser_controller_ =
        std::make_unique<default_browser::DefaultBrowserController>(
            std::make_unique<default_browser::FakeDefaultBrowserSetter>(),
            default_browser::DefaultBrowserEntrypointType::kSettingsPage);
  }

 private:
  friend class DefaultBrowserHandlerTest;
};

class DefaultBrowserHandlerTest : public testing::Test {
 public:
  DefaultBrowserHandlerTest() = default;

  void SetUp() override {
    scoped_override_ =
        GlobalFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindLambdaForTesting([&](BrowserProcess& browser_process) {
              return std::make_unique<default_browser::DefaultBrowserManager>(
                  &browser_process,
                  std::make_unique<default_browser::FakeShellDelegate>(),
                  base::BindLambdaForTesting(
                      [&]() { return static_cast<Profile*>(profile_.get()); }));
            }));
    TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
        /*profile_manager=*/false);

    handler_ = std::make_unique<TestingDefaultBrowserHandler>();

    profile_ = std::make_unique<TestingProfile>();
    test_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_.get()));
    test_web_ui_ = std::make_unique<content::TestWebUI>();
    test_web_ui_->set_web_contents(test_web_contents_.get());
    handler_->set_web_ui(test_web_ui_.get());

    handler_->RegisterMessages();
    handler_->AllowJavascript();
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
    handler_.reset();
    test_web_ui_.reset();
    test_web_contents_.reset();
    profile_.reset();
  }

 protected:
  void CallRequestDefaultBrowserState() {
    base::ListValue args;
    args.Append(kRequestDefaultBrowserStateCallback);
    test_web_ui()->HandleReceivedMessage("requestDefaultBrowserState", args);

    WaitForSingleCallData();

    const content::TestWebUI::CallData& call_data = GetCallData();
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    ASSERT_TRUE(call_data.arg1()->is_string());
    EXPECT_EQ(kRequestDefaultBrowserStateCallback,
              call_data.arg1()->GetString());
    ASSERT_TRUE(call_data.arg2()->GetBool());
  }

  void CallRequestUserValueStringsFeatureState() {
    // Simulate the WebUI call to the handler.
    base::ListValue args;
    args.Append(kRequestUserValueStringsFeatureStateCallback);
    test_web_ui()->HandleReceivedMessage("requestUserValueStringsFeatureState",
                                         args);

    WaitForSingleCallData();

    // Verify the response sent back to the WebUI.
    const content::TestWebUI::CallData& call_data = GetCallData();
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    // Check the callback ID.
    ASSERT_TRUE(call_data.arg1()->is_string());
    EXPECT_EQ(kRequestUserValueStringsFeatureStateCallback,
              call_data.arg1()->GetString());
    // Check the success argument.
    ASSERT_TRUE(call_data.arg2()->is_bool());
    ASSERT_TRUE(call_data.arg2()->GetBool());
  }

  void CallSetDefaultBrowser() {
    base::ListValue empty_args;
    test_web_ui()->HandleReceivedMessage("setAsDefaultBrowser", empty_args);

    WaitForSingleCallData();

    VerifyBrowserDefaultStateChangedEventFired();
  }

  void VerifyBrowserDefaultStateChangedEventFired() {
    const content::TestWebUI::CallData& call_data =
        *test_web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    ASSERT_TRUE(call_data.arg1()->is_string());
    EXPECT_EQ("browser-default-state-changed", call_data.arg1()->GetString());
  }

  void WaitForSingleCallData() {
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return !test_web_ui()->call_data().empty(); }));
    ASSERT_EQ(test_web_ui()->call_data().size(), 1u);
  }

  const content::TestWebUI::CallData& GetCallData() {
    return *test_web_ui()->call_data().back();
  }

  void VerifyDefaultBrowserState(const base::Value* value,
                                 bool expected_is_default,
                                 bool expected_is_unknown_error,
                                 bool expected_is_disabled_by_policy) {
    ASSERT_TRUE(value->is_dict());
    const auto& dict = value->GetDict();

    const auto& is_default = dict.FindBool("isDefault");
    ASSERT_TRUE(is_default);
    EXPECT_EQ(*is_default, expected_is_default);

    const auto& can_be_default = dict.FindBool("canBeDefault");
    ASSERT_TRUE(can_be_default);
    // The value canBeDefault may change based on operation system, so don't
    // expect something specific.

    const auto& is_unknown_error = dict.FindBool("isUnknownError");
    ASSERT_TRUE(is_unknown_error);
    EXPECT_EQ(*is_unknown_error, expected_is_unknown_error);

    const auto& is_disabled_by_policy = dict.FindBool("isDisabledByPolicy");
    ASSERT_TRUE(is_disabled_by_policy);
    EXPECT_EQ(*is_disabled_by_policy, expected_is_disabled_by_policy);
  }

  content::TestWebUI* test_web_ui() { return test_web_ui_.get(); }

  TestingPrefServiceSimple* local_state() {
    return TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;

  std::unique_ptr<TestingDefaultBrowserHandler> handler_;

  ui::UserDataFactory::ScopedOverride scoped_override_;
};

TEST_F(DefaultBrowserHandlerTest, RequestDefaultBrowserState) {
  CallRequestDefaultBrowserState();

  VerifyDefaultBrowserState(GetCallData().arg3(), /*expected_is_default=*/false,
                            /*expected_is_unknown_error=*/false,
                            /*expected_is_disabled_by_policy=*/false);
}
TEST_F(DefaultBrowserHandlerTest,
       RequestUserValueStringsFeatureState_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUserValueDefaultBrowserStrings);

  CallRequestUserValueStringsFeatureState();

  // Check the returned feature state.
  ASSERT_TRUE(GetCallData().arg3()->is_bool());
  ASSERT_TRUE(GetCallData().arg3()->GetBool());
}

TEST_F(DefaultBrowserHandlerTest,
       RequestUserValueStringsFeatureState_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kUserValueDefaultBrowserStrings);

  CallRequestUserValueStringsFeatureState();

  // Check the returned feature state.
  ASSERT_TRUE(GetCallData().arg3()->is_bool());
  ASSERT_FALSE(GetCallData().arg3()->GetBool());
}

TEST_F(DefaultBrowserHandlerTest, SetDefaultBrowser) {
  base::HistogramTester histogram_tester;
  CallSetDefaultBrowser();

  VerifyDefaultBrowserState(GetCallData().arg2(), /*expected_is_default=*/true,
                            /*expected_is_unknown_error=*/false,
                            /*expected_is_disabled_by_policy=*/false);

  histogram_tester.ExpectUniqueSample(
      "DefaultBrowser.SettingsPage.ShellIntegration.Interaction",
      default_browser::DefaultBrowserInteractionType::kAccepted, 1);
}

// Check that changing the default browser policy triggers an event.
TEST_F(DefaultBrowserHandlerTest, DefaultBrowserPolicyChange) {
  local_state()->SetManagedPref(prefs::kDefaultBrowserSettingEnabled,
                                std::make_unique<base::Value>(false));

  WaitForSingleCallData();

  VerifyBrowserDefaultStateChangedEventFired();

  VerifyDefaultBrowserState(GetCallData().arg2(), /*expected_is_default=*/false,
                            /*expected_is_unknown_error=*/false,
                            /*expected_is_disabled_by_policy=*/true);
}

}  // namespace settings
