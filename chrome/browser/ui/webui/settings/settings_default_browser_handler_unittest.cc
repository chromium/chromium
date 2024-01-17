// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/settings/settings_default_browser_handler.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/run_until.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kRequestDefaultBrowserStateCallback[] =
    "requestDefaultBrowserStateCallback";

class FakeDefaultBrowserWorker
    : public shell_integration::DefaultBrowserWorker {
 public:
  explicit FakeDefaultBrowserWorker(
      shell_integration::DefaultWebClientState os_state)
      : os_state_(os_state) {}

 private:
  ~FakeDefaultBrowserWorker() override = default;

  shell_integration::DefaultWebClientState CheckIsDefaultImpl() override {
    return os_state_;
  }

  void SetAsDefaultImpl(base::OnceClosure on_finished_callback) override {
    os_state_ = shell_integration::IS_DEFAULT;
    std::move(on_finished_callback).Run();
  }

  shell_integration::DefaultWebClientState os_state_;
};

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
    // Override the worker with a mock.
    default_browser_worker_ = base::MakeRefCounted<FakeDefaultBrowserWorker>(
        shell_integration::NOT_DEFAULT);
  }

 private:
  friend class DefaultBrowserHandlerTest;
};

class DefaultBrowserHandlerTest : public testing::Test {
 public:
  DefaultBrowserHandlerTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
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
    handler_.reset();
    test_web_ui_.reset();
    test_web_contents_.reset();
    profile_.reset();
  }

 protected:
  void CallRequestDefaultBrowserState() {
    base::Value::List args;
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

  void CallSetDefaultBrowser() {
    base::Value::List empty_args;
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

  TestingPrefServiceSimple* local_state() { return testing_local_state_.Get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  ScopedTestingLocalState testing_local_state_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;

  std::unique_ptr<TestingDefaultBrowserHandler> handler_;
};

TEST_F(DefaultBrowserHandlerTest, RequestDefaultBrowserState) {
  CallRequestDefaultBrowserState();

  VerifyDefaultBrowserState(GetCallData().arg3(), /*expected_is_default=*/false,
                            /*expected_is_unknown_error=*/false,
                            /*expected_is_disabled_by_policy=*/false);
}

TEST_F(DefaultBrowserHandlerTest, SetDefaultBrowser) {
  CallSetDefaultBrowser();

  VerifyDefaultBrowserState(GetCallData().arg2(), /*expected_is_default=*/true,
                            /*expected_is_unknown_error=*/false,
                            /*expected_is_disabled_by_policy=*/false);
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
