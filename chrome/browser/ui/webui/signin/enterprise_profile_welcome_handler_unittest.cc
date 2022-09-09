// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_handler.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

class EnterpriseProfileWelcomeHandlerTestBase
    : public BrowserWithTestWindowTest {
 public:
  EnterpriseProfileWelcomeHandlerTestBase() = default;
  EnterpriseProfileWelcomeHandlerTestBase(
      const EnterpriseProfileWelcomeHandlerTestBase&) = delete;
  EnterpriseProfileWelcomeHandler& operator=(
      const EnterpriseProfileWelcomeHandlerTestBase&) = delete;
  ~EnterpriseProfileWelcomeHandlerTestBase() override = default;

  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    chrome::NewTab(browser());

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(
        browser()->tab_strip_model()->GetActiveWebContents());

    account_info_.email = user_manager::kStubUserEmail;
    account_info_.gaia = user_manager::kStubUserId;
    account_info_.account_id = CoreAccountId::FromGaiaId(account_info_.gaia);
  }

  void InitializeHandler(EnterpriseProfileWelcomeUI::ScreenType screen_type,
                         bool profile_creation_required_by_policy,
                         bool show_link_data_option,
                         signin::SigninChoiceCallback proceed_callback) {
    message_handler_.reset();

    message_handler_ = std::make_unique<EnterpriseProfileWelcomeHandler>(
        /*browser=*/nullptr, screen_type, profile_creation_required_by_policy,
        show_link_data_option, account_info_, absl::optional<SkColor>(),
        std::move(proceed_callback));
    message_handler_->set_web_ui_for_test(web_ui());
    message_handler_->RegisterMessages();
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
    message_handler_.reset();
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  EnterpriseProfileWelcomeHandler* handler() { return message_handler_.get(); }

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  AccountInfo account_info_;

  std::unique_ptr<EnterpriseProfileWelcomeHandler> message_handler_;
};

struct HandleProceedTestParam {
  bool profile_creation_required_by_policy = false;
  bool should_link_data = false;
  signin::SigninChoice expected_choice = signin::SIGNIN_CHOICE_CANCEL;
};
const HandleProceedTestParam kHandleProceedParams[] = {
    {false, false, signin::SIGNIN_CHOICE_NEW_PROFILE},
    {false, true, signin::SIGNIN_CHOICE_CONTINUE},
    {true, false, signin::SIGNIN_CHOICE_NEW_PROFILE},
    {true, true, signin::SIGNIN_CHOICE_CONTINUE},
};

class EnterpriseProfileWelcomeHandleProceedTest
    : public EnterpriseProfileWelcomeHandlerTestBase,
      public testing::WithParamInterface<HandleProceedTestParam> {};

// Tests how `HandleProceed` processes the arguments and the handler's state to
// notify the registered callback.
TEST_P(EnterpriseProfileWelcomeHandleProceedTest, HandleProceed) {
  base::MockCallback<signin::SigninChoiceCallback> mock_proceed_callback;
  InitializeHandler(
      EnterpriseProfileWelcomeUI::ScreenType::kEntepriseAccountSyncEnabled,
      GetParam().profile_creation_required_by_policy,
      /*show_link_data_option=*/true, mock_proceed_callback.Get());

  base::Value::List args;
  args.Append(GetParam().should_link_data);
  EXPECT_CALL(mock_proceed_callback, Run(GetParam().expected_choice));
  web_ui()->HandleReceivedMessage("proceed", args);
}

INSTANTIATE_TEST_SUITE_P(All,
                         EnterpriseProfileWelcomeHandleProceedTest,
                         testing::ValuesIn(kHandleProceedParams));
