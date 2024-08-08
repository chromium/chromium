// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/sheet_models.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/grit/generated_resources.h"
#include "device/fido/fido_types.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using Mechanism = AuthenticatorRequestDialogModel::Mechanism;
using MechanismVisibility =
    AuthenticatorGenericErrorSheetModel::OtherMechanismButtonVisibility;

class AuthenticatorSheetBaseTest : public testing::Test {};

class TestAuthenticatorSheetModel : public AuthenticatorSheetModelBase {
 public:
  TestAuthenticatorSheetModel(
      AuthenticatorRequestDialogModel* dialog_model,
      OtherMechanismButtonVisibility other_mechanism_button_visibility)
      : AuthenticatorSheetModelBase(dialog_model,
                                    other_mechanism_button_visibility) {
    vector_illustrations_.emplace(kPasskeyUsbDarkIcon,
                                  kPasskeyUsbDarkIcon);
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
  auto dialog_model = base::MakeRefCounted<AuthenticatorRequestDialogModel>(
      /*render_frame_host=*/nullptr);

  // No mechanisms present.
  {
    TestAuthenticatorSheetModel sheet_model(dialog_model.get(),
                                            MechanismVisibility::kVisible);
    dialog_model->mechanisms.clear();
    EXPECT_FALSE(sheet_model.IsOtherMechanismButtonVisible());
  }

  // Two mechanisms present.
  {
    TestAuthenticatorSheetModel sheet_model(dialog_model.get(),
                                            MechanismVisibility::kVisible);
    dialog_model->mechanisms.clear();
    dialog_model->mechanisms.emplace_back(Mechanism::Phone("phone"), u"phone",
                                          u"ph", kPasskeyAoaIcon,
                                          base::DoNothing());
    dialog_model->mechanisms.emplace_back(
        Mechanism::Transport(AuthenticatorTransport::kUsbHumanInterfaceDevice),
        u"security key", u"usb", kPasskeyAoaIcon, base::DoNothing());
    EXPECT_TRUE(sheet_model.IsOtherMechanismButtonVisible());
  }

  // Hidden button.
  {
    TestAuthenticatorSheetModel sheet_model(dialog_model.get(),
                                            MechanismVisibility::kHidden);
    dialog_model->mechanisms.clear();
    dialog_model->mechanisms.emplace_back(Mechanism::Phone("phone"), u"phone",
                                          u"ph", kPasskeyAoaIcon,
                                          base::DoNothing());
    EXPECT_FALSE(sheet_model.IsOtherMechanismButtonVisible());
  }
}

// Regression test for crbug.com/1408492.
TEST_F(AuthenticatorSheetBaseTest,
       IsOtherMechanismButtonVisible_NoDialogModel) {
  auto dialog_model = base::MakeRefCounted<AuthenticatorRequestDialogModel>(
      /*render_frame_host=*/nullptr);
  TestAuthenticatorSheetModel sheet_model(dialog_model.get(),
                                          MechanismVisibility::kVisible);
  dialog_model.reset();
  EXPECT_FALSE(sheet_model.IsOtherMechanismButtonVisible());
}

class AuthenticatorMultiSourcePickerSheetModelTest : public testing::Test {};

constexpr char16_t kPasskeyName1[] = u"yuki";
constexpr char16_t kPasskeyName2[] = u"kodai";
constexpr char16_t kPhoneName[] = u"pixel 7";

TEST_F(AuthenticatorMultiSourcePickerSheetModelTest, GPMPasskeysOnly) {
  auto dialog_model = base::MakeRefCounted<AuthenticatorRequestDialogModel>(
      /*render_frame_host=*/nullptr);
  dialog_model->paired_phone_names = {base::UTF16ToUTF8(kPhoneName)};
  dialog_model->priority_phone_name = dialog_model->paired_phone_names.at(0);
  dialog_model->mechanisms.emplace_back(
      Mechanism::Credential({device::AuthenticatorType::kPhone, {0}}),
      kPasskeyName1, kPasskeyName1, kPasskeyPhoneIcon, base::DoNothing());
  dialog_model->mechanisms.emplace_back(
      Mechanism::Credential({device::AuthenticatorType::kPhone, {1}}),
      kPasskeyName2, kPasskeyName2, kPasskeyPhoneIcon, base::DoNothing());
  dialog_model->mechanisms.emplace_back(
      Mechanism::Transport(AuthenticatorTransport::kUsbHumanInterfaceDevice),
      u"security key", u"usb", kPasskeyAoaIcon, base::DoNothing());

  AuthenticatorMultiSourcePickerSheetModel model(dialog_model.get());
  EXPECT_THAT(model.primary_passkey_indices(), testing::ElementsAre(0, 1));
  EXPECT_THAT(model.secondary_passkey_indices(), testing::ElementsAre(2));
  EXPECT_EQ(
      model.primary_passkeys_label(),
      l10n_util::GetStringFUTF16(IDS_WEBAUTHN_FROM_PHONE_LABEL, kPhoneName));
}

TEST_F(AuthenticatorMultiSourcePickerSheetModelTest,
       GPMPasskeysAndLocalPasskeys) {
  auto dialog_model = base::MakeRefCounted<AuthenticatorRequestDialogModel>(
      /*render_frame_host=*/nullptr);
  dialog_model->paired_phone_names = {base::UTF16ToUTF8(kPhoneName)};
  dialog_model->priority_phone_name = dialog_model->paired_phone_names.at(0);
  dialog_model->mechanisms.emplace_back(
      Mechanism::Credential({device::AuthenticatorType::kPhone, {0}}),
      kPasskeyName1, kPasskeyName1, kPasskeyPhoneIcon, base::DoNothing());
  dialog_model->mechanisms.emplace_back(
      Mechanism::Credential({device::AuthenticatorType::kTouchID, {1}}),
      kPasskeyName2, kPasskeyName2, kPasskeyAoaIcon, base::DoNothing());
  dialog_model->mechanisms.emplace_back(
      Mechanism::Transport(AuthenticatorTransport::kUsbHumanInterfaceDevice),
      u"security key", u"usb", kPasskeyAoaIcon, base::DoNothing());

  AuthenticatorMultiSourcePickerSheetModel model(dialog_model.get());
  EXPECT_THAT(model.primary_passkey_indices(), testing::ElementsAre(1));
  EXPECT_THAT(model.secondary_passkey_indices(), testing::ElementsAre(0, 2));
  EXPECT_EQ(model.primary_passkeys_label(),
            l10n_util::GetStringUTF16(IDS_WEBAUTHN_THIS_DEVICE_LABEL));
}

TEST_F(AuthenticatorMultiSourcePickerSheetModelTest, GPMMechanismAndPhones) {
  auto dialog_model = base::MakeRefCounted<AuthenticatorRequestDialogModel>(
      /*render_frame_host=*/nullptr);
  dialog_model->paired_phone_names = {base::UTF16ToUTF8(kPhoneName)};
  dialog_model->priority_phone_name = dialog_model->paired_phone_names.at(0);
  dialog_model->mechanisms.emplace_back(
      Mechanism::Credential({device::AuthenticatorType::kPhone, {0}}),
      kPasskeyName1, kPasskeyName1, kPasskeyPhoneIcon, base::DoNothing());
  dialog_model->mechanisms.emplace_back(Mechanism::Enclave(), u"enclave",
                                        u"enclave", kPasskeyAoaIcon,
                                        base::DoNothing());

  AuthenticatorMultiSourcePickerSheetModel model(dialog_model.get());
  EXPECT_THAT(model.primary_passkey_indices(), testing::ElementsAre(1));
  EXPECT_THAT(model.secondary_passkey_indices(), testing::ElementsAre(0));
  EXPECT_EQ(model.primary_passkeys_label(),
            l10n_util::GetStringUTF16(IDS_WEBAUTHN_THIS_DEVICE_LABEL));
}

TEST_F(AuthenticatorMultiSourcePickerSheetModelTest, NoDiscoveredPasskeys) {
  auto dialog_model = base::MakeRefCounted<AuthenticatorRequestDialogModel>(
      /*render_frame_host=*/nullptr);
  dialog_model->mechanisms.emplace_back(
      Mechanism::Transport(AuthenticatorTransport::kUsbHumanInterfaceDevice),
      u"security key", u"usb", kPasskeyAoaIcon, base::DoNothing());

  AuthenticatorMultiSourcePickerSheetModel model(dialog_model.get());
  EXPECT_TRUE(model.primary_passkey_indices().empty());
  EXPECT_THAT(model.secondary_passkey_indices(), testing::ElementsAre(0));
  EXPECT_EQ(model.primary_passkeys_label(), u"");
}

}  // namespace
