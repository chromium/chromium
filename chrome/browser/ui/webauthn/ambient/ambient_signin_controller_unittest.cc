// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/views/page_action/action_ids.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/passkey_credential.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/action_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/menus/simple_menu_model.h"

namespace ambient_signin {

namespace {

using password_manager::PasskeyCredential;
using password_manager::PasswordForm;
using testing::_;
using testing::Return;

}  // namespace

class AmbientSigninControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    mock_tab_interface_ = std::make_unique<tabs::MockTabInterface>();
    tabs::TabLookupFromWebContents::CreateForWebContents(
        web_contents(), mock_tab_interface_.get());

    AmbientSigninController::CreateForCurrentDocument(main_rfh());

    mock_page_action_controller_ = std::make_unique<
        testing::NiceMock<page_actions::MockPageActionController>>();
    controller()->SetPageActionControllerForTesting(
        mock_page_action_controller_.get());
  }

  AmbientSigninController* controller() {
    return AmbientSigninController::GetForCurrentDocument(main_rfh());
  }

  page_actions::MockPageActionController* page_action_controller() {
    return mock_page_action_controller_.get();
  }

  PasskeyCredential TestCredential() {
    return PasskeyCredential(PasskeyCredential::Source::kGooglePasswordManager,
                             PasskeyCredential::RpId("example.com"),
                             PasskeyCredential::CredentialId({1, 2, 3}),
                             PasskeyCredential::UserId({4, 5, 6}),
                             PasskeyCredential::Username("username"),
                             PasskeyCredential::DisplayName("display name"));
  }

 protected:
  std::unique_ptr<tabs::MockTabInterface> mock_tab_interface_;
  std::unique_ptr<page_actions::MockPageActionController>
      mock_page_action_controller_;
};

TEST_F(AmbientSigninControllerTest, ShowSinglePasskey) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  std::vector<PasskeyCredential> credentials;
  credentials.emplace_back(TestCredential());
  std::vector<std::unique_ptr<PasswordForm>> forms;

  EXPECT_CALL(*page_action_controller(), Show(kActionWebAuthnAmbientSignin));
  EXPECT_CALL(*page_action_controller(),
              ShowSuggestionChip(kActionWebAuthnAmbientSignin,
                                 page_actions::SuggestionChipConfig()));
  EXPECT_CALL(*page_action_controller(),
              OverrideText(kActionWebAuthnAmbientSignin,
                           l10n_util::GetStringFUTF16(
                               IDS_WEBAUTHN_SIGN_IN_AS_PROMPT, u"username")));

  controller()->Show(model.get(), std::move(credentials), std::move(forms),
                     base::DoNothing(), base::DoNothing());
}

TEST_F(AmbientSigninControllerTest, ShowSinglePassword) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  std::vector<PasskeyCredential> credentials;
  std::vector<std::unique_ptr<PasswordForm>> forms;
  auto form = std::make_unique<PasswordForm>();
  form->username_value = u"username";
  forms.push_back(std::move(form));

  EXPECT_CALL(*page_action_controller(), Show(kActionWebAuthnAmbientSignin));
  EXPECT_CALL(*page_action_controller(),
              ShowSuggestionChip(kActionWebAuthnAmbientSignin,
                                 page_actions::SuggestionChipConfig()));
  EXPECT_CALL(*page_action_controller(),
              OverrideText(kActionWebAuthnAmbientSignin,
                           l10n_util::GetStringFUTF16(
                               IDS_WEBAUTHN_SIGN_IN_AS_PROMPT, u"username")));

  controller()->Show(model.get(), std::move(credentials), std::move(forms),
                     base::DoNothing(), base::DoNothing());
}

TEST_F(AmbientSigninControllerTest, TriggerPageActionSignInPasskey) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  std::vector<PasskeyCredential> credentials;
  credentials.emplace_back(TestCredential());
  std::vector<std::unique_ptr<PasswordForm>> forms;

  base::MockOnceCallback<void(const std::vector<uint8_t>)> passkey_callback;
  EXPECT_CALL(passkey_callback, Run(std::vector<uint8_t>{1, 2, 3}));

  controller()->Show(model.get(), std::move(credentials), std::move(forms),
                     passkey_callback.Get(), base::NullCallback());

  EXPECT_CALL(*page_action_controller(), Hide(kActionWebAuthnAmbientSignin))
      .Times(testing::AtLeast(1));

  controller()->TriggerPageActionSignIn();
}

TEST_F(AmbientSigninControllerTest, TriggerPageActionSignInPassword) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  std::vector<PasskeyCredential> credentials;
  std::vector<std::unique_ptr<PasswordForm>> forms;
  auto form = std::make_unique<PasswordForm>();
  form->username_value = u"username";
  form->password_value = u"password";
  forms.push_back(std::move(form));

  base::MockOnceCallback<void(PasswordCredentialPair)> password_callback;
  EXPECT_CALL(password_callback,
              Run(std::make_pair(std::u16string(u"username"),
                                 std::u16string(u"password"))));

  controller()->Show(model.get(), std::move(credentials), std::move(forms),
                     base::NullCallback(), password_callback.Get());

  EXPECT_CALL(*page_action_controller(), Hide(kActionWebAuthnAmbientSignin))
      .Times(testing::AtLeast(1));

  controller()->TriggerPageActionSignIn();
}

TEST_F(AmbientSigninControllerTest, OnRequestCompleteClosesUI) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  std::vector<PasskeyCredential> credentials;
  credentials.emplace_back(TestCredential());
  std::vector<std::unique_ptr<PasswordForm>> forms;

  controller()->Show(model.get(), std::move(credentials), std::move(forms),
                     base::DoNothing(), base::DoNothing());

  EXPECT_CALL(*page_action_controller(), Hide(kActionWebAuthnAmbientSignin))
      .Times(testing::AtLeast(1));

  model->OnRequestComplete();
}

TEST_F(AmbientSigninControllerTest, GetSignInCallbackPasskey) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  std::vector<PasskeyCredential> credentials;
  credentials.emplace_back(TestCredential());
  std::vector<std::unique_ptr<PasswordForm>> forms;

  base::MockOnceCallback<void(const std::vector<uint8_t>)> passkey_callback;
  EXPECT_CALL(passkey_callback, Run(std::vector<uint8_t>{1, 2, 3}));

  controller()->Show(model.get(), std::move(credentials), std::move(forms),
                     passkey_callback.Get(), base::DoNothing());

  EXPECT_CALL(*page_action_controller(), Hide(kActionWebAuthnAmbientSignin))
      .Times(testing::AtLeast(1));

  auto callback = controller()->GetSignInCallback();
  std::move(callback).Run();
}

TEST_F(AmbientSigninControllerTest, GetSignInCallbackPassword) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  std::vector<PasskeyCredential> credentials;
  std::vector<std::unique_ptr<PasswordForm>> forms;
  auto form = std::make_unique<PasswordForm>();
  form->username_value = u"username";
  form->password_value = u"password";
  forms.push_back(std::move(form));

  base::MockOnceCallback<void(PasswordCredentialPair)> password_callback;
  EXPECT_CALL(password_callback,
              Run(std::make_pair(std::u16string(u"username"),
                                 std::u16string(u"password"))));

  controller()->Show(model.get(), std::move(credentials), std::move(forms),
                     base::DoNothing(), password_callback.Get());

  EXPECT_CALL(*page_action_controller(), Hide(kActionWebAuthnAmbientSignin))
      .Times(testing::AtLeast(1));

  auto callback = controller()->GetSignInCallback();
  std::move(callback).Run();
}

}  // namespace ambient_signin
