// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "components/cbor/values.h"
#include "content/public/test/browser_test.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/pin.h"
#include "device/fido/public_key_credential_user_entity.h"

class AuthenticatorDialogTest : public DialogBrowserTest {
 public:
  AuthenticatorDialogTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Web modal dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    auto model = std::make_unique<AuthenticatorRequestDialogModel>(
        /*relying_party_id=*/"example.com");
    ::device::FidoRequestHandlerBase::TransportAvailabilityInfo
        transport_availability;
    transport_availability.available_transports = {
        AuthenticatorTransport::kUsbHumanInterfaceDevice,
        AuthenticatorTransport::kInternal,
        AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy};
    if (name == "cable_server_link_activate") {
      transport_availability.available_transports.insert(
          AuthenticatorTransport::kAndroidAccessory);
    }
    model->set_cable_transport_info(/*cable_extension_provided=*/true,
                                    /*has_paired_phones=*/false,
                                    "fido://qrcode");
    model->StartFlow(std::move(transport_availability),
                     /*use_location_bar_bubble=*/false);

    // The dialog should immediately close as soon as it is displayed.
    if (name == "transports") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kTransportSelection);
    } else if (name == "activate_usb") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kUsbInsertAndActivate);
    } else if (name == "timeout") {
      model->SetCurrentStep(AuthenticatorRequestDialogModel::Step::kTimedOut);
    } else if (name == "no_available_transports") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kErrorNoAvailableTransports);
    } else if (name == "key_not_registered") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kKeyNotRegistered);
    } else if (name == "key_already_registered") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kKeyAlreadyRegistered);
    } else if (name == "internal_unrecognized_error") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kErrorInternalUnrecognized);
    } else if (name == "ble_power_on_manual") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kBlePowerOnManual);
    } else if (name == "touchid_incognito") {
      model->SetCurrentStep(AuthenticatorRequestDialogModel::Step::
                                kPlatformAuthenticatorOffTheRecordInterstitial);
    } else if (name == "cable_activate" ||
               name == "cable_server_link_activate") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kCableActivate);
    } else if (name == "cable_v2_activate") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kCableV2Activate);
    } else if (name == "cable_v2_pair") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kCableV2QRCode);
    } else if (name == "set_pin") {
      model->CollectPIN(device::pin::PINEntryReason::kSet,
                        device::pin::PINEntryError::kNoError, 6, 0,
                        base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin") {
      model->CollectPIN(device::pin::PINEntryReason::kChallenge,
                        device::pin::PINEntryError::kNoError, 6, 8,
                        base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin_two_tries_remaining") {
      model->CollectPIN(device::pin::PINEntryReason::kChallenge,
                        device::pin::PINEntryError::kWrongPIN, 6, 2,
                        base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin_one_try_remaining") {
      model->CollectPIN(device::pin::PINEntryReason::kChallenge,
                        device::pin::PINEntryError::kWrongPIN, 6, 1,
                        base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin_fallback") {
      model->CollectPIN(device::pin::PINEntryReason::kChallenge,
                        device::pin::PINEntryError::kInternalUvLocked, 6, 8,
                        base::BindOnce([](std::u16string pin) {}));
    } else if (name == "inline_bio_enrollment") {
      model->StartInlineBioEnrollment(base::DoNothing());
      timer_.Start(
          FROM_HERE, base::TimeDelta::FromSeconds(2),
          base::BindLambdaForTesting([&, weak_model = model->GetWeakPtr()] {
            if (!weak_model || weak_model->current_step() !=
                                   AuthenticatorRequestDialogModel::Step::
                                       kInlineBioEnrollment) {
              return;
            }
            weak_model->OnSampleCollected(--bio_samples_remaining_);
            if (bio_samples_remaining_ <= 0)
              timer_.Stop();
          }));
    } else if (name == "retry_uv") {
      model->OnRetryUserVerification(5);
    } else if (name == "retry_uv_two_tries_remaining") {
      model->OnRetryUserVerification(2);
    } else if (name == "retry_uv_one_try_remaining") {
      model->OnRetryUserVerification(1);
    } else if (name == "force_pin_change") {
      model->CollectPIN(device::pin::PINEntryReason::kChange,
                        device::pin::PINEntryError::kNoError, 6, 0,
                        base::BindOnce([](std::u16string pin) {}));
    } else if (name == "force_pin_change_same_as_current") {
      model->CollectPIN(device::pin::PINEntryReason::kChange,
                        device::pin::PINEntryError::kSameAsCurrentPIN, 6, 0,
                        base::BindOnce([](std::u16string pin) {}));
    } else if (name == "second_tap") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kClientPinTapAgain);
    } else if (name == "soft_block") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kClientPinErrorSoftBlock);
    } else if (name == "hard_block") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kClientPinErrorHardBlock);
    } else if (name == "authenticator_removed") {
      model->SetCurrentStep(AuthenticatorRequestDialogModel::Step::
                                kClientPinErrorAuthenticatorRemoved);
    } else if (name == "missing_capability") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kMissingCapability);
    } else if (name == "storage_full") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kStorageFull);
    } else if (name == "account_select") {
      // These strings attempt to exercise the encoding of direction and
      // language from https://github.com/w3c/webauthn/pull/1530.

      // lang_and_dir_encoded contains a string with right-to-left and ar-SA
      // tags. It's the UTF-8 encoding of the code points {0xE0001, 0xE0061,
      // 0xE0072, 0xE002D, 0xE0053, 0xE0041, 0x200F, 0xFEA2, 0xFE92, 0xFBFF,
      // 0xFE91, 0x20, 0xFE8E, 0xFEDF, 0xFEAE, 0xFEA4, 0xFEE3, 0xFE8E, 0xFEE7}.
      const std::string lang_and_dir_encoded =
          "\xf3\xa0\x80\x81\xf3\xa0\x81\xa1\xf3\xa0\x81\xb2\xf3\xa0\x80\xad\xf3"
          "\xa0\x81\x93\xf3\xa0\x81\x81\xe2\x80\x8f\xef\xba\xa2\xef\xba\x92\xef"
          "\xaf\xbf\xef\xba\x91\x20\xef\xba\x8e\xef\xbb\x9f\xef\xba\xae\xef\xba"
          "\xa4\xef\xbb\xa3\xef\xba\x8e\xef\xbb\xa7";
      // lang_jp_encoded specifies a kanji with language jp. This is the middle
      // glyph from the example given in
      // https://www.w3.org/TR/string-meta/#capturing-the-text-processing-language.
      // It's the UTF-8 encoding of the code points {0xE0001, 0xE006a, 0xE0070,
      // 0x76f4}.
      const std::string lang_jp_encoded =
          "\xf3\xa0\x80\x81\xf3\xa0\x81\xaa\xf3\xa0\x81\xb0\xe7\x9b\xb4";
      // lang_zh_hant_encoded specifies the same code point as
      // |lang_jp_encoded|, but with the language set to zh-Hant. According to
      // the W3C document referenced above, this should display differently.
      // It's the UTF-8 encoding of the code points {0xE0001, 0xe007a, 0xe0068,
      // 0xe002d, 0xe0048, 0xe0061, 0xe006e, 0xe0074}.
      const std::string lang_zh_hant_encoded =
          "\xf3\xa0\x80\x81\xf3\xa0\x81\xba\xf3\xa0\x81\xa8\xf3\xa0\x80\xad\xf3"
          "\xa0\x81\x88\xf3\xa0\x81\xa1\xf3\xa0\x81\xae\xf3\xa0\x81\xb4";

      const std::vector<std::pair<std::string, std::string>> infos = {
          {"foo@example.com", "Test User 1"},
          {"", "Test User 2"},
          {"", ""},
          {"bat@example.com", "Test User 4"},
          {"encoded@example.com", lang_and_dir_encoded},
          {"encoded2@example.com", lang_jp_encoded},
          {"encoded3@example.com", lang_zh_hant_encoded},
          {"verylong@"
           "reallylongreallylongreallylongreallylongreallylongreallylong.com",
           "Very Long String Very Long String Very Long String Very Long "
           "String Very Long String Very Long String "},
      };
      std::vector<device::AuthenticatorGetAssertionResponse> responses;

      for (const auto& info : infos) {
        static const uint8_t kAppParam[32] = {0};
        static const uint8_t kSignatureCounter[4] = {0};
        device::AuthenticatorData auth_data(kAppParam, 0 /* flags */,
                                            kSignatureCounter, base::nullopt);
        device::AuthenticatorGetAssertionResponse response(
            std::move(auth_data), {10, 11, 12, 13} /* signature */);
        device::PublicKeyCredentialUserEntity user({1, 2, 3, 4});
        user.name = info.first;
        user.display_name = info.second;
        response.user_entity = std::move(user);
        responses.emplace_back(std::move(response));
      }

      model->SelectAccount(
          std::move(responses),
          base::BindOnce([](device::AuthenticatorGetAssertionResponse) {}));
    } else if (name == "request_attestation_permission") {
      model->RequestAttestationPermission(false, base::DoNothing());
    } else if (name == "request_enterprise_attestation_permission") {
      model->RequestAttestationPermission(true, base::DoNothing());
    }

    ShowAuthenticatorRequestDialog(
        browser()->tab_strip_model()->GetActiveWebContents(), std::move(model));
  }

 private:
  base::RepeatingTimer timer_;
  int bio_samples_remaining_ = 5;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorDialogTest);
};

// Run with:
//   --gtest_filter=BrowserUiTest.Invoke --test-launcher-interactive \
//   --ui=AuthenticatorDialogTest.InvokeUi_default
IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_force_pin_change) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_force_pin_change_same_as_current) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_transports) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_activate_usb) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_timeout) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_no_available_transports) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_key_not_registered) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_key_already_registered) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_internal_unrecognized_error) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_ble_power_on_manual) {
  ShowAndVerifyUi();
}

#if defined(OS_MAC)
IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_touchid) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_touchid_incognito) {
  ShowAndVerifyUi();
}
#endif  // defined(OS_MAC)

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_cable_activate) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_cable_server_link_activate) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_cable_v2_activate) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_cable_v2_pair) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_set_pin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_get_pin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_get_pin_two_tries_remaining) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_get_pin_one_try_remaining) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_get_pin_fallback) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_inline_bio_enrollment) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_retry_uv) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_retry_uv_two_tries_remaining) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_retry_uv_one_try_remaining) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_second_tap) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_soft_block) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_hard_block) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_authenticator_removed) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_missing_capability) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_storage_full) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_resident_credential_confirm) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_account_select) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_request_attestation_permission) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_request_enterprise_attestation_permission) {
  ShowAndVerifyUi();
}
