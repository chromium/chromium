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
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/vector_icons/vector_icons.h"
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

 protected:
  std::unique_ptr<tabs::MockTabInterface> mock_tab_interface_;
  std::unique_ptr<page_actions::MockPageActionController>
      mock_page_action_controller_;
};

TEST_F(AmbientSigninControllerTest, ShowSinglePasskey) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"username", vector_icons::kPasskeyIcon, base::DoNothing());

  EXPECT_CALL(*page_action_controller(), Show(kActionWebAuthnAmbientSignin));
  EXPECT_CALL(*page_action_controller(),
              ShowSuggestionChip(kActionWebAuthnAmbientSignin,
                                 page_actions::SuggestionChipConfig()));
  EXPECT_CALL(*page_action_controller(),
              OverrideText(kActionWebAuthnAmbientSignin,
                           l10n_util::GetStringFUTF16(
                               IDS_WEBAUTHN_SIGN_IN_AS_PROMPT, u"username")));

  controller()->Show(model.get());
}

TEST_F(AmbientSigninControllerTest, ShowSinglePassword) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Password(
          AuthenticatorRequestDialogModel::Mechanism::PasswordInfo(
              std::nullopt)),
      u"username", kPasswordFieldIcon, base::DoNothing());

  EXPECT_CALL(*page_action_controller(), Show(kActionWebAuthnAmbientSignin));
  EXPECT_CALL(*page_action_controller(),
              ShowSuggestionChip(kActionWebAuthnAmbientSignin,
                                 page_actions::SuggestionChipConfig()));
  EXPECT_CALL(*page_action_controller(),
              OverrideText(kActionWebAuthnAmbientSignin,
                           l10n_util::GetStringFUTF16(
                               IDS_WEBAUTHN_SIGN_IN_AS_PROMPT, u"username")));

  controller()->Show(model.get());
}

TEST_F(AmbientSigninControllerTest, TriggerPageActionSignInPasskey) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  base::MockRepeatingClosure passkey_callback;
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"username", vector_icons::kPasskeyIcon, passkey_callback.Get());

  EXPECT_CALL(passkey_callback, Run());

  controller()->Show(model.get());

  EXPECT_CALL(*page_action_controller(), Hide(kActionWebAuthnAmbientSignin))
      .Times(testing::AtLeast(1));

  controller()->TriggerPageActionSignIn();
}

TEST_F(AmbientSigninControllerTest, TriggerPageActionSignInPassword) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  base::MockRepeatingClosure password_callback;
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Password(
          AuthenticatorRequestDialogModel::Mechanism::PasswordInfo(
              std::nullopt)),
      u"username", kPasswordFieldIcon, password_callback.Get());

  EXPECT_CALL(password_callback, Run());

  controller()->Show(model.get());

  EXPECT_CALL(*page_action_controller(), Hide(kActionWebAuthnAmbientSignin))
      .Times(testing::AtLeast(1));

  controller()->TriggerPageActionSignIn();
}

TEST_F(AmbientSigninControllerTest, OnRequestCompleteClosesUI) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"username", vector_icons::kPasskeyIcon, base::DoNothing());

  controller()->Show(model.get());

  EXPECT_CALL(*page_action_controller(), Hide(kActionWebAuthnAmbientSignin))
      .Times(testing::AtLeast(1));

  model->OnRequestComplete();
}

TEST_F(AmbientSigninControllerTest, GetSignInCallbackPasskey) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  base::MockRepeatingClosure passkey_callback;
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"username", vector_icons::kPasskeyIcon, passkey_callback.Get());

  EXPECT_CALL(passkey_callback, Run());

  controller()->Show(model.get());

  EXPECT_CALL(*page_action_controller(), Hide(kActionWebAuthnAmbientSignin))
      .Times(testing::AtLeast(1));

  auto callback = controller()->GetSignInCallback();
  std::move(callback).Run();
}

TEST_F(AmbientSigninControllerTest, GetSignInCallbackPassword) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  base::MockRepeatingClosure password_callback;
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Password(
          AuthenticatorRequestDialogModel::Mechanism::PasswordInfo(
              std::nullopt)),
      u"username", kPasswordFieldIcon, password_callback.Get());

  EXPECT_CALL(password_callback, Run());

  controller()->Show(model.get());

  EXPECT_CALL(*page_action_controller(), Hide(kActionWebAuthnAmbientSignin))
      .Times(testing::AtLeast(1));

  auto callback = controller()->GetSignInCallback();
  std::move(callback).Run();
}

}  // namespace ambient_signin
