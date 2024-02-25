// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_client_helper.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using testing::AnyNumber;
using testing::NiceMock;
using testing::Return;

constexpr char kTestUsername[] = "user";
constexpr char kGaiaAccountEmail[] = "user@gmail.com";
constexpr char kTestPassword[] = "T3stP@$$w0rd";
constexpr char kTestOrigin[] = "https://example.com/";

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;

  MOCK_METHOD(bool, IsAutoSignInEnabled, (), (const, override));
  MOCK_METHOD(void,
              PromptUserToMovePasswordToAccount,
              (std::unique_ptr<PasswordFormManagerForUI>),
              (override));
  MOCK_METHOD(void, PromptUserToEnableAutosignin, (), (override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (const, override));
  MOCK_METHOD(bool, IsOffTheRecord, (), (const, override));
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
};

PasswordForm CreateForm(std::string username,
                        std::string password,
                        GURL origin) {
  PasswordForm form;
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  form.url = origin;
  form.signon_realm = origin.spec();
  return form;
}

std::unique_ptr<PasswordFormManagerForUI> CreateFormManager(
    const PasswordForm* form,
    bool is_movable) {
  auto manager = std::make_unique<NiceMock<MockPasswordFormManagerForUI>>();
  ON_CALL(*manager, GetPendingCredentials)
      .WillByDefault(testing::ReturnRef(*form));
  ON_CALL(*manager, IsMovableToAccountStore).WillByDefault(Return(is_movable));
  return manager;
}

}  // namespace

class PasswordManagerClientHelperTest : public testing::Test {
 public:
  PasswordManagerClientHelperTest() : helper_(&client_) {
    prefs_.registry()->RegisterBooleanPref(
        prefs::kWasAutoSignInFirstRunExperienceShown, false);
    prefs_.SetBoolean(prefs::kWasAutoSignInFirstRunExperienceShown, false);
    ON_CALL(client_, GetPrefs()).WillByDefault(Return(&prefs_));

    ON_CALL(*client(), GetIdentityManager)
        .WillByDefault(Return(identity_test_environment()->identity_manager()));
    identity_test_environment()->SetPrimaryAccount(
        kGaiaAccountEmail, signin::ConsentLevel::kSignin);
  }
  ~PasswordManagerClientHelperTest() override = default;

 protected:
  PasswordManagerClientHelper* helper() { return &helper_; }
  MockPasswordManagerClient* client() { return &client_; }
  signin::IdentityTestEnvironment* identity_test_environment() {
    return &identity_test_environment_;
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  NiceMock<MockPasswordManagerClient> client_;
  PasswordManagerClientHelper helper_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(PasswordManagerClientHelperTest, PromptAutosigninAfterSuccessfulLogin) {
  EXPECT_CALL(*client(), IsAutoSignInEnabled).WillOnce(Return(true));
  EXPECT_CALL(*client(), PromptUserToEnableAutosignin);
  EXPECT_CALL(*client(), PromptUserToMovePasswordToAccount).Times(0);

  const PasswordForm form =
      CreateForm(kTestUsername, kTestPassword, GURL(kTestOrigin));
  helper()->NotifyUserCouldBeAutoSignedIn(std::make_unique<PasswordForm>(form));
  helper()->NotifySuccessfulLoginWithExistingPassword(
      CreateFormManager(&form, /*is_movable=*/true));
}

TEST_F(PasswordManagerClientHelperTest,
       PromptAutosigninAndMoveDisabledInIncognito) {
  EXPECT_CALL(*client(), IsOffTheRecord)
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  // In Incognito, both the auto-signin and the "Move password to account?"
  // bubbles should be disabled.
  EXPECT_CALL(*client(), PromptUserToEnableAutosignin).Times(0);
  EXPECT_CALL(*client(), PromptUserToMovePasswordToAccount).Times(0);

  const PasswordForm form =
      CreateForm(kTestUsername, kTestPassword, GURL(kTestOrigin));
  helper()->NotifyUserCouldBeAutoSignedIn(std::make_unique<PasswordForm>(form));
  helper()->NotifySuccessfulLoginWithExistingPassword(
      CreateFormManager(&form, /*is_movable=*/true));
}

TEST_F(PasswordManagerClientHelperTest, PromptMoveForMovableFormInAccountMode) {
  ON_CALL(*client()->GetPasswordFeatureManager(),
          ShouldShowAccountStorageBubbleUi)
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore)
      .WillByDefault(Return(PasswordForm::Store::kAccountStore));
  EXPECT_CALL(*client(), PromptUserToEnableAutosignin).Times(0);
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(*client(), PromptUserToMovePasswordToAccount);
#else
  // On Android and iOS, prompting to move after using a password isn't
  // implemented.
  EXPECT_CALL(*client(), PromptUserToMovePasswordToAccount).Times(0);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

  // Indicate successful login.
  const PasswordForm form =
      CreateForm(kTestUsername, kTestPassword, GURL(kTestOrigin));
  helper()->NotifySuccessfulLoginWithExistingPassword(
      CreateFormManager(&form, /*is_movable=*/true));
}

TEST_F(PasswordManagerClientHelperTest,
       NoPromptToMoveForMovableFormInProfileMode) {
  ON_CALL(*client()->GetPasswordFeatureManager(),
          ShouldShowAccountStorageBubbleUi)
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore)
      .WillByDefault(Return(PasswordForm::Store::kProfileStore));
  EXPECT_CALL(*client(), PromptUserToMovePasswordToAccount).Times(0);
  EXPECT_CALL(*client(), PromptUserToEnableAutosignin).Times(0);

  // Indicate successful login.
  const PasswordForm form =
      CreateForm(kTestUsername, kTestPassword, GURL(kTestOrigin));
  helper()->NotifySuccessfulLoginWithExistingPassword(
      CreateFormManager(&form, /*is_movable=*/true));
}

TEST_F(PasswordManagerClientHelperTest, NoPromptToMoveForUnmovableForm) {
  ON_CALL(*client()->GetPasswordFeatureManager(),
          ShouldShowAccountStorageBubbleUi)
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore)
      .WillByDefault(Return(PasswordForm::Store::kAccountStore));
  EXPECT_CALL(*client(), PromptUserToMovePasswordToAccount).Times(0);
  EXPECT_CALL(*client(), PromptUserToEnableAutosignin).Times(0);

  // Indicate successful login without matching form.
  const PasswordForm form =
      CreateForm(kTestUsername, kTestPassword, GURL(kTestOrigin));
  helper()->NotifySuccessfulLoginWithExistingPassword(
      CreateFormManager(&form, /*is_movable=*/false));
}

TEST_F(PasswordManagerClientHelperTest, NoPromptToMoveForGaiaAccountForm) {
  ON_CALL(*client()->GetPasswordFeatureManager(),
          ShouldShowAccountStorageBubbleUi)
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore)
      .WillByDefault(Return(PasswordForm::Store::kAccountStore));

  EXPECT_CALL(*client(), PromptUserToMovePasswordToAccount).Times(0);

  const PasswordForm gaia_account_form = CreateForm(
      kGaiaAccountEmail, kTestPassword, GaiaUrls::GetInstance()->gaia_url());
  helper()->NotifySuccessfulLoginWithExistingPassword(
      CreateFormManager(&gaia_account_form, /*is_movable=*/true));
}

TEST_F(PasswordManagerClientHelperTest, NoPromptToMoveForNonOptedInUser) {
  ON_CALL(*client()->GetPasswordFeatureManager(),
          ShouldShowAccountStorageBubbleUi)
      .WillByDefault(Return(true));
  ON_CALL(*client()->GetPasswordFeatureManager(), IsOptedInForAccountStorage)
      .WillByDefault(Return(false));
  ON_CALL(*client()->GetPasswordFeatureManager(), GetDefaultPasswordStore)
      .WillByDefault(Return(PasswordForm::Store::kProfileStore));

  EXPECT_CALL(*client(), PromptUserToMovePasswordToAccount).Times(0);
  const PasswordForm form =
      CreateForm(kTestUsername, kTestPassword, GURL(kTestOrigin));
  helper()->NotifySuccessfulLoginWithExistingPassword(
      CreateFormManager(&form, /*is_movable=*/true));
}

}  // namespace password_manager
