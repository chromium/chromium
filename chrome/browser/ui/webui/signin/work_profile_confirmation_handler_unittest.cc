// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/work_profile_confirmation_handler.h"

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/checked_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/work_profile_confirmation_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/dialog_test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"

const int kExpectedProfileImageSize = 128;

// The dialog needs to be initialized with a height but the actual value doesn't
// really matter in unit tests.
const double kDefaultDialogHeight = 350.0;

class TestingWorkProfileConfirmationHandler
    : public WorkProfileConfirmationHandler {
 public:
  TestingWorkProfileConfirmationHandler(
      Browser* browser,
      content::WebUI* web_ui,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback)
      : WorkProfileConfirmationHandler(browser->profile(),
                                       browser,
                                       std::move(callback)) {
    set_web_ui(web_ui);
  }

  using WorkProfileConfirmationHandler::HandleCancel;
  using WorkProfileConfirmationHandler::HandleConfirm;
  using WorkProfileConfirmationHandler::HandleInitializedWithSize;
  using WorkProfileConfirmationHandler::SetUserImageURL;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingWorkProfileConfirmationHandler);
};

// TODO (crbug/1170448): Re-enable all tests.
class WorkProfileConfirmationHandlerTest : public BrowserWithTestWindowTest {
 public:
  WorkProfileConfirmationHandlerTest() : web_ui_(new content::TestWebUI) {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    chrome::NewTab(browser());
    web_ui()->set_web_contents(
        browser()->tab_strip_model()->GetActiveWebContents());

    auto handler = std::make_unique<TestingWorkProfileConfirmationHandler>(
        browser(), web_ui(),
        base::BindOnce(&WorkProfileConfirmationHandlerTest::
                           OnWorkProfileConfirmationUIClosed,
                       base::Unretained(this)));
    handler_ = handler.get();

    work_profile_confirmation_ui_ =
        std::make_unique<WorkProfileConfirmationUI>(web_ui());
    web_ui()->AddMessageHandler(std::move(handler));

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    account_info_ =
        identity_test_env()->MakePrimaryAccountAvailable("foo@example.com");
  }

  void TearDown() override {
    work_profile_confirmation_ui_.reset();
    web_ui_.reset();
    identity_test_env_adaptor_.reset();
    BrowserWithTestWindowTest::TearDown();

    EXPECT_EQ(did_user_explicitly_interact_ ? 0 : 1,
              user_action_tester()->GetActionCount("Signin_Abort_Signin"));
  }

  TestingWorkProfileConfirmationHandler* handler() { return handler_; }

  content::TestWebUI* web_ui() { return web_ui_.get(); }

  base::UserActionTester* user_action_tester() { return &user_action_tester_; }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override {
    return std::make_unique<DialogTestBrowserWindow>();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories;
    IdentityTestEnvironmentProfileAdaptor::
        AppendIdentityTestEnvironmentFactories(&factories);
    return factories;
  }

  void OnWorkProfileConfirmationUIClosed(
      DiceTurnSyncOnHelper::SigninChoice result) {
    on_work_profile_confirmation_ui_closed_called_ = true;
    work_profile_confirmation_ui_closed_result_ = result;
  }

  void ExpectAccountImageChanged(
      const content::TestWebUI::CallData& call_data) {
    EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
    std::string event;
    ASSERT_TRUE(call_data.arg1()->GetAsString(&event));
    EXPECT_EQ("account-image-changed", event);

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    base::Optional<AccountInfo> primary_account =
        identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
            identity_manager->GetPrimaryAccountInfo());

    std::string original_picture_url =
        primary_account ? primary_account->picture_url : std::string();
    std::string expected_picture_url =
        original_picture_url.empty()
            ? profiles::GetPlaceholderAvatarIconUrl()
            : signin::GetAvatarImageURLWithOptions(GURL(original_picture_url),
                                                   kExpectedProfileImageSize,
                                                   false /* no_silhouette */)
                  .spec();
    std::string passed_picture_url;
    ASSERT_TRUE(call_data.arg2()->GetAsString(&passed_picture_url));
    EXPECT_EQ(expected_picture_url, passed_picture_url);
  }

 protected:
  bool did_user_explicitly_interact_ = false;
  bool on_work_profile_confirmation_ui_closed_called_ = false;
  DiceTurnSyncOnHelper::SigninChoice
      work_profile_confirmation_ui_closed_result_ =
          DiceTurnSyncOnHelper::SigninChoice::SIGNIN_CHOICE_CANCEL;
  // Holds information for the account currently logged in.
  AccountInfo account_info_;

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<WorkProfileConfirmationUI> work_profile_confirmation_ui_;
  CheckedPtr<TestingWorkProfileConfirmationHandler> handler_;  // Not owned.
  base::UserActionTester user_action_tester_;
  std::unordered_map<std::string, int> string_to_grd_id_map_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

  DISALLOW_COPY_AND_ASSIGN(WorkProfileConfirmationHandlerTest);
};

TEST_F(WorkProfileConfirmationHandlerTest,
       DISABLED_TestSetImageIfPrimaryAccountReady) {
  identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
      account_info_.account_id, account_info_.email, account_info_.gaia, "",
      "full_name", "given_name", "locale",
      "http://picture.example.com/picture.jpg");

  base::ListValue args;
  args.Set(0, std::make_unique<base::Value>(kDefaultDialogHeight));
  handler()->HandleInitializedWithSize(&args);

  ASSERT_EQ(1U, web_ui()->call_data().size());
  ExpectAccountImageChanged(*web_ui()->call_data()[0]);
}

TEST_F(WorkProfileConfirmationHandlerTest,
       DISABLED_TestSetImageIfPrimaryAccountReadyLater) {
  base::ListValue args;
  args.Set(0, std::make_unique<base::Value>(kDefaultDialogHeight));
  handler()->HandleInitializedWithSize(&args);

  ASSERT_EQ(0U, web_ui()->call_data().size());

  identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
      account_info_.account_id, account_info_.email, account_info_.gaia, "",
      "full_name", "given_name", "locale",
      "http://picture.example.com/picture.jpg");

  ASSERT_EQ(1U, web_ui()->call_data().size());
  ExpectAccountImageChanged(*web_ui()->call_data()[0]);
}

TEST_F(WorkProfileConfirmationHandlerTest,
       DISABLED_TestSetImageIgnoredIfSecondaryAccountUpdated) {
  base::ListValue args;
  args.Set(0, std::make_unique<base::Value>(kDefaultDialogHeight));
  handler()->HandleInitializedWithSize(&args);

  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bar@example.com");
  identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
      account_info.account_id, account_info.email, account_info.gaia, "",
      "bar_full_name", "bar_given_name", "bar_locale",
      "http://picture.example.com/bar_picture.jpg");

  // Updating the account info of a secondary account should not update the
  // image of the sync confirmation dialog.
  EXPECT_EQ(0U, web_ui()->call_data().size());

  identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
      account_info_.account_id, account_info_.email, account_info_.gaia, "",
      "full_name", "given_name", "locale",
      "http://picture.example.com/picture.jpg");

  // Updating the account info of the primary account should update the
  // image of the sync confirmation dialog.
  ASSERT_EQ(1U, web_ui()->call_data().size());
  ExpectAccountImageChanged(*web_ui()->call_data()[0]);
}

TEST_F(WorkProfileConfirmationHandlerTest, DISABLED_TestHandleCancel) {
  handler()->HandleCancel(nullptr);
  did_user_explicitly_interact_ = true;

  EXPECT_TRUE(on_work_profile_confirmation_ui_closed_called_);
  EXPECT_EQ(DiceTurnSyncOnHelper::SigninChoice::SIGNIN_CHOICE_CANCEL,
            work_profile_confirmation_ui_closed_result_);
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
                   "Signin_WorkProfilePrompt_Cancel"));
  EXPECT_EQ(
      0, user_action_tester()->GetActionCount("Signin_WorkProfilePrompt_Add"));
}

TEST_F(WorkProfileConfirmationHandlerTest, DISABLED_TestHandleConfirm) {
  // These are passed as parameters to HandleConfirm().
  base::ListValue args;

  handler()->HandleConfirm(&args);
  did_user_explicitly_interact_ = true;

  EXPECT_TRUE(on_work_profile_confirmation_ui_closed_called_);
  EXPECT_EQ(DiceTurnSyncOnHelper::SigninChoice::SIGNIN_CHOICE_NEW_PROFILE,
            work_profile_confirmation_ui_closed_result_);
  EXPECT_EQ(0, user_action_tester()->GetActionCount(
                   "Signin_WorkProfilePrompt_Cancel"));
  EXPECT_EQ(
      1, user_action_tester()->GetActionCount("Signin_WorkProfilePrompt_Add"));
}
