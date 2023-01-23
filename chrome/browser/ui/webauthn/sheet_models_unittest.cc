// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/sheet_models.h"

#include "base/functional/callback_helpers.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using Mechanism = AuthenticatorRequestDialogModel::Mechanism;
using MechanismVisibility =
    AuthenticatorGenericErrorSheetModel::OtherMechanismButtonVisibility;

class AuthenticatorSheetBaseTest : public testing::Test {};

class MockDialogModel : public AuthenticatorRequestDialogModel {
 public:
  MockDialogModel() : AuthenticatorRequestDialogModel(nullptr) {}

  MOCK_METHOD(base::span<const Mechanism>, mechanisms, (), (const));
};

class TestAuthenticatorSheetModel : public AuthenticatorSheetModelBase {
 public:
  TestAuthenticatorSheetModel(
      AuthenticatorRequestDialogModel* dialog_model,
      OtherMechanismButtonVisibility other_mechanism_button_visibility)
      : AuthenticatorSheetModelBase(dialog_model,
                                    other_mechanism_button_visibility) {}

  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override {
    return kPasskeyHeaderDarkIcon;
  }

  std::u16string GetStepTitle() const override { return u"Step title"; }

  std::u16string GetStepDescription() const override {
    return u"Step description";
  }

  bool IsOtherMechanismButtonVisible() const override {
    return AuthenticatorSheetModelBase::IsOtherMechanismButtonVisible();
  }
};

TEST_F(AuthenticatorSheetBaseTest, IsOtherMechanismButtonVisible) {
  MockDialogModel dialog_model;

  // No mechanisms present.
  {
    TestAuthenticatorSheetModel sheet_model(&dialog_model,
                                            MechanismVisibility::kVisible);
    std::vector<Mechanism> mechanisms;
    EXPECT_CALL(dialog_model, mechanisms)
        .WillOnce(testing::Return(base::make_span(mechanisms)));
    EXPECT_FALSE(sheet_model.IsOtherMechanismButtonVisible());
    testing::Mock::VerifyAndClearExpectations(&dialog_model);
  }

  // Two mechanisms present.
  {
    TestAuthenticatorSheetModel sheet_model(&dialog_model,
                                            MechanismVisibility::kVisible);
    std::vector<Mechanism> mechanisms;
    mechanisms.emplace_back(Mechanism::Phone("phone"), u"phone", u"ph",
                            kPasskeyAoaIcon, base::DoNothing(), false);
    mechanisms.emplace_back(
        Mechanism::Transport(AuthenticatorTransport::kUsbHumanInterfaceDevice),
        u"security key", u"usb", kPasskeyAoaIcon, base::DoNothing(), false);
    EXPECT_CALL(dialog_model, mechanisms)
        .WillOnce(testing::Return(base::make_span(mechanisms)));
    EXPECT_TRUE(sheet_model.IsOtherMechanismButtonVisible());
    testing::Mock::VerifyAndClearExpectations(&dialog_model);
  }

  // Hidden button.
  {
    TestAuthenticatorSheetModel sheet_model(&dialog_model,
                                            MechanismVisibility::kHidden);
    EXPECT_CALL(dialog_model, mechanisms).Times(0);
    EXPECT_FALSE(sheet_model.IsOtherMechanismButtonVisible());
    testing::Mock::VerifyAndClearExpectations(&dialog_model);
  }
}

// Regression test for crbug.com/1408492.
TEST_F(AuthenticatorSheetBaseTest,
       IsOtherMechanismButtonVisible_NoDialogModel) {
  auto dialog_model = std::make_unique<MockDialogModel>();
  TestAuthenticatorSheetModel sheet_model(dialog_model.get(),
                                          MechanismVisibility::kVisible);
  dialog_model.reset();
  EXPECT_FALSE(sheet_model.IsOtherMechanismButtonVisible());
}

}  // namespace
