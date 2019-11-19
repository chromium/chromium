// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_create_profile_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

// Gmock matchers and actions.
using testing::_;
using testing::Invoke;

namespace {

const char kTestProfileName[] = "test-profile-name";

const char kTestWebUIResponse[] = "cr.webUIListenerCallback";

}  // namespace

class TestSigninCreateProfileHandler : public SigninCreateProfileHandler {
 public:
  explicit TestSigninCreateProfileHandler(
      content::WebUI* web_ui,
      TestingProfileManager* profile_manager)
          : profile_manager_(profile_manager) {
    set_web_ui(web_ui);
  }

  // Mock this method since it tries to create a profile asynchronously and the
  // test terminates before the callback gets called.
  MOCK_METHOD3(DoCreateProfile,
               void(const base::string16& name,
                    const std::string& icon_url,
                    bool create_shortcut));

  // Creates the profile synchronously, sets the appropriate flag and calls the
  // callback method to resume profile creation flow.
  void RealDoCreateProfile(const base::string16& name,
                           const std::string& icon_url,
                           bool create_shortcut) {
    // Create the profile synchronously.
    Profile* profile = profile_manager_->CreateTestingProfile(
        kTestProfileName,
        std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>(), name,
        0, std::string(), TestingProfile::TestingFactories());

    // Set the flag used to track the state of the creation flow.
    profile_path_being_created_ = profile->GetPath();

    // Call the callback method to resume profile creation flow.
    SigninCreateProfileHandler::OnProfileCreated(
        create_shortcut,
        profile,
        Profile::CREATE_STATUS_INITIALIZED);
  }

  // Mock this method to track when an attempt to open a new browser window for
  // the newly created profile is made.
  MOCK_METHOD2(OpenNewWindowForProfile,
               void(Profile* profile, Profile::CreateStatus status));

  // Mock this method so that we don't actually open the signin dialog during
  // the test.
  MOCK_METHOD1(OpenForceSigninDialogForProfile, void(Profile* profile));

 private:
  TestingProfileManager* profile_manager_;
  DISALLOW_COPY_AND_ASSIGN(TestSigninCreateProfileHandler);
};

class SigninCreateProfileHandlerTest : public BrowserWithTestWindowTest {
 public:
  SigninCreateProfileHandlerTest()
      : web_ui_(new content::TestWebUI) {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    profile_manager()->DeleteAllTestingProfiles();

    handler_ = std::make_unique<TestSigninCreateProfileHandler>(
        web_ui(), profile_manager());
  }

  void TearDown() override {
    handler_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  content::TestWebUI* web_ui() {
    return web_ui_.get();
  }

  TestSigninCreateProfileHandler* handler() {
    return handler_.get();
  }

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<TestSigninCreateProfileHandler> handler_;
};

TEST_F(SigninCreateProfileHandlerTest, ReturnDefaultProfileIcons) {
  // Request default profile information.
  base::ListValue list_args;
  handler()->RequestDefaultProfileIcons(&list_args);

  // Expect one JS callbacks for the profile avatar icons.
  EXPECT_EQ(1U, web_ui()->call_data().size());

  EXPECT_EQ(kTestWebUIResponse, web_ui()->call_data()[0]->function_name());

  std::string callback_name;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg1()->GetAsString(&callback_name));
  EXPECT_EQ("profile-icons-received", callback_name);

  const base::ListValue* profile_icons;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg2()->GetAsList(&profile_icons));
  EXPECT_NE(0U, profile_icons->GetSize());
}

TEST_F(SigninCreateProfileHandlerTest, CreateProfile) {
  // Expect the call to create the profile.
  EXPECT_CALL(*handler(), DoCreateProfile(_, _, _))
      .WillOnce(Invoke(handler(),
                       &TestSigninCreateProfileHandler::RealDoCreateProfile));

  // Expect a new browser window for the new profile to be opened.
  EXPECT_CALL(*handler(), OpenNewWindowForProfile(_, _));

  // Expect no signin dialog opened for the new profile.
  EXPECT_CALL(*handler(), OpenForceSigninDialogForProfile(_)).Times(0);

  // Create a profile.
  base::ListValue list_args;
  list_args.AppendString(kTestProfileName);
  list_args.AppendString(profiles::GetDefaultAvatarIconUrl(0));
  list_args.AppendBoolean(false);  // create_shortcut
  handler()->CreateProfile(&list_args);

  // Expect a JS callbacks with the new profile information.
  EXPECT_EQ(1U, web_ui()->call_data().size());

  EXPECT_EQ(kTestWebUIResponse, web_ui()->call_data()[0]->function_name());

  std::string callback_name;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg1()->GetAsString(&callback_name));
  EXPECT_EQ("create-profile-success", callback_name);
}

TEST_F(SigninCreateProfileHandlerTest, CreateProfileWithForceSignin) {
  signin_util::SetForceSigninForTesting(true);
  ASSERT_TRUE(signin_util::IsForceSigninEnabled());

  // Expect the call to create the profile.
  EXPECT_CALL(*handler(), DoCreateProfile(_, _, _))
      .WillOnce(Invoke(handler(),
                       &TestSigninCreateProfileHandler::RealDoCreateProfile));

  // Expect no new browser window for the new profile.
  EXPECT_CALL(*handler(), OpenNewWindowForProfile(_, _)).Times(0);

  // Expect a signin dialog opened for the new profile.
  EXPECT_CALL(*handler(), OpenForceSigninDialogForProfile(_)).Times(1);

  base::ListValue list_args;
  list_args.AppendString(kTestProfileName);
  list_args.AppendString(profiles::GetDefaultAvatarIconUrl(0));
  list_args.AppendBoolean(false);  // create_shortcut
  handler()->CreateProfile(&list_args);

  // Expect a JS callbacks with the new profile information.
  EXPECT_EQ(1U, web_ui()->call_data().size());

  EXPECT_EQ(kTestWebUIResponse, web_ui()->call_data()[0]->function_name());

  std::string callback_name;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg1()->GetAsString(&callback_name));
  EXPECT_EQ("create-profile-success", callback_name);

  signin_util::SetForceSigninForTesting(false);
}
