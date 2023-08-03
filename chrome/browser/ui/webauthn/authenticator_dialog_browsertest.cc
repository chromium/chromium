// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "content/public/test/browser_test.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/pin.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace {

constexpr char kPhoneName[] = "Elisa's Pixel 6 Pro";

}  // namespace

// Run with:
//
//   --gtest_filter=BrowserUiTest.Invoke --test-launcher-interactive \
//   --ui=AuthenticatorDialogTest.InvokeUi_${test_name}
//
// where test_name is the second arg to IN_PROC_BROWSER_TEST_F().

class AuthenticatorDialogTest : public DialogBrowserTest {
 public:
  AuthenticatorDialogTest() = default;
  AuthenticatorDialogTest(const AuthenticatorDialogTest&) = delete;
  AuthenticatorDialogTest& operator=(const AuthenticatorDialogTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Web modal dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    model_ = std::make_unique<AuthenticatorRequestDialogModel>(
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetPrimaryMainFrame());
    model_->set_relying_party_id("example.com");

    device::FidoRequestHandlerBase::TransportAvailabilityInfo&
        transport_availability = model_->transport_availability_for_testing();
    transport_availability.available_transports = {
        AuthenticatorTransport::kUsbHumanInterfaceDevice,
        AuthenticatorTransport::kInternal,
        AuthenticatorTransport::kHybrid,
        AuthenticatorTransport::kAndroidAccessory,
    };

    std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
    auto phone = std::make_unique<device::cablev2::Pairing>();
    phone->from_sync_deviceinfo = false;
    phone->name = kPhoneName;
    phones.emplace_back(std::move(phone));
    if (name == "cable_server_link_activate") {
      transport_availability.available_transports.insert(
          AuthenticatorTransport::kAndroidAccessory);
    }
    transport_availability.has_platform_authenticator_credential = device::
        FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential;
    transport_availability.request_type =
        device::FidoRequestType::kGetAssertion;

    // The dialog should immediately close as soon as it is displayed.
    if (name == "mechanisms") {
      // A phone is configured so that the "Manage devices" button is shown.
      model_->set_cable_transport_info(
          /*extension_is_v2=*/absl::nullopt, std::move(phones),
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kMechanismSelection);
    } else if (name == "activate_usb") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kUsbInsertAndActivate);
    } else if (name == "timeout") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kTimedOut);
    } else if (name == "no_available_transports") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kErrorNoAvailableTransports);
    } else if (name == "key_not_registered") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kKeyNotRegistered);
    } else if (name == "key_already_registered") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kKeyAlreadyRegistered);
    } else if (name == "windows_hello_not_enabled") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kErrorWindowsHelloNotEnabled);
    } else if (name == "internal_unrecognized_error") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kErrorInternalUnrecognized);
    } else if (name == "ble_power_on_manual") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kBlePowerOnManual);
    } else if (name == "touchid_incognito") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kOffTheRecordInterstitial);
    } else if (name == "cable_activate" ||
               name == "cable_server_link_activate") {
      model_->set_cable_transport_info(
          /*extension_is_v2=*/false, std::move(phones),
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      model_->ContactPhoneForTesting(kPhoneName);
    } else if (name == "cable_v2_activate") {
      model_->set_cable_transport_info(
          /*extension_is_v2=*/absl::nullopt, std::move(phones),
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      model_->ContactPhoneForTesting(kPhoneName);
    } else if (name == "cable_v2_pair") {
      model_->set_cable_transport_info(
          /*extension_is_v2=*/absl::nullopt,
          /*paired_phones=*/{},
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kCableV2QRCode);
    } else if (name == "cable_v2_connecting") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kCableV2Connecting);
    } else if (name == "cable_v2_connected") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kCableV2Connected);
    } else if (name == "cable_v2_error") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kCableV2Error);
    } else if (name == "phone_aoa") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kAndroidAccessory);
    } else if (name == "set_pin") {
      model_->CollectPIN(device::pin::PINEntryReason::kSet,
                         device::pin::PINEntryError::kNoError, 6, 0,
                         base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin") {
      model_->CollectPIN(device::pin::PINEntryReason::kChallenge,
                         device::pin::PINEntryError::kNoError, 6, 8,
                         base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin_two_tries_remaining") {
      model_->CollectPIN(device::pin::PINEntryReason::kChallenge,
                         device::pin::PINEntryError::kWrongPIN, 6, 2,
                         base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin_one_try_remaining") {
      model_->CollectPIN(device::pin::PINEntryReason::kChallenge,
                         device::pin::PINEntryError::kWrongPIN, 6, 1,
                         base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin_fallback") {
      model_->CollectPIN(device::pin::PINEntryReason::kChallenge,
                         device::pin::PINEntryError::kInternalUvLocked, 6, 8,
                         base::BindOnce([](std::u16string pin) {}));
    } else if (name == "inline_bio_enrollment") {
      model_->StartInlineBioEnrollment(base::DoNothing());
      timer_.Start(
          FROM_HERE, base::Seconds(2),
          base::BindLambdaForTesting([&, weak_model = model_->GetWeakPtr()] {
            if (!weak_model || weak_model->current_step() !=
                                   AuthenticatorRequestDialogModel::Step::
                                       kInlineBioEnrollment) {
              return;
            }
            weak_model->OnSampleCollected(--bio_samples_remaining_);
            if (bio_samples_remaining_ <= 0) {
              timer_.Stop();
            }
          }));
    } else if (name == "retry_uv") {
      model_->OnRetryUserVerification(5);
    } else if (name == "retry_uv_two_tries_remaining") {
      model_->OnRetryUserVerification(2);
    } else if (name == "retry_uv_one_try_remaining") {
      model_->OnRetryUserVerification(1);
    } else if (name == "force_pin_change") {
      model_->CollectPIN(device::pin::PINEntryReason::kChange,
                         device::pin::PINEntryError::kNoError, 6, 0,
                         base::BindOnce([](std::u16string pin) {}));
    } else if (name == "force_pin_change_same_as_current") {
      model_->CollectPIN(device::pin::PINEntryReason::kChange,
                         device::pin::PINEntryError::kSameAsCurrentPIN, 6, 0,
                         base::BindOnce([](std::u16string pin) {}));
    } else if (name == "second_tap") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kClientPinTapAgain);
    } else if (name == "soft_block") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kClientPinErrorSoftBlock);
    } else if (name == "hard_block") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kClientPinErrorHardBlock);
    } else if (name == "authenticator_removed") {
      model_->SetCurrentStepForTesting(AuthenticatorRequestDialogModel::Step::
                                           kClientPinErrorAuthenticatorRemoved);
    } else if (name == "missing_capability") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kMissingCapability);
    } else if (name == "storage_full") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kStorageFull);
    } else if (name == "single_account_select") {
      const std::vector<std::pair<std::string, std::string>> infos = {
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
                                            kSignatureCounter, absl::nullopt);
        device::AuthenticatorGetAssertionResponse response(
            std::move(auth_data), {10, 11, 12, 13} /* signature */,
            /*transport_used=*/absl::nullopt);
        device::PublicKeyCredentialUserEntity user({1, 2, 3, 4});
        user.name = info.first;
        user.display_name = info.second;
        response.credential = device::PublicKeyCredentialDescriptor(
            device::CredentialType::kPublicKey, {1, 2, 3, 4});
        response.user_entity = std::move(user);
        responses.emplace_back(std::move(response));
      }

      model_->SelectAccount(
          std::move(responses),
          base::BindOnce([](device::AuthenticatorGetAssertionResponse) {}));
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
          {"user name with\na line break", "display name\nwith a line break"},
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
                                            kSignatureCounter, absl::nullopt);
        device::AuthenticatorGetAssertionResponse response(
            std::move(auth_data), {10, 11, 12, 13} /* signature */,
            /*transport_used=*/absl::nullopt);
        device::PublicKeyCredentialUserEntity user({1, 2, 3, 4});
        user.name = info.first;
        user.display_name = info.second;
        response.credential = device::PublicKeyCredentialDescriptor(
            device::CredentialType::kPublicKey, {1, 2, 3, 4});
        response.user_entity = std::move(user);
        responses.emplace_back(std::move(response));
      }

      model_->SelectAccount(
          std::move(responses),
          base::BindOnce([](device::AuthenticatorGetAssertionResponse) {}));
    } else if (name == "request_attestation_permission") {
      model_->RequestAttestationPermission(false, base::DoNothing());
    } else if (name == "request_enterprise_attestation_permission") {
      model_->RequestAttestationPermission(true, base::DoNothing());
    } else if (name == "server_link_title_UNLOCK_YOUR_PHONE") {
      model_->set_cable_transport_info(
          /*extension_is_v2=*/true, /*paired_phones=*/{},
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kCableActivate);
    } else if (name == "create_passkey") {
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kCreatePasskey);
    } else if (name == "phone_confirmation") {
      model_->set_cable_transport_info(
          /*extension_is_v2=*/true, std::move(phones),
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kPhoneConfirmationSheet);
    }
#if BUILDFLAG(IS_MAC)
    else if (name == "ble_permission_mac") {  // NOLINT
      model_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kBlePermissionMac);
    }
#endif

    model_->StartFlow(std::move(transport_availability),
                      /*is_conditional_mediation=*/false);
  }

 private:
  std::unique_ptr<AuthenticatorRequestDialogModel> model_;
  base::RepeatingTimer timer_;
  int bio_samples_remaining_ = 5;
};

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

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_mechanisms) {
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
                       InvokeUi_windows_hello_not_enabled) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_internal_unrecognized_error) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_ble_power_on_manual) {
  ShowAndVerifyUi();
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_touchid) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_touchid_incognito) {
  ShowAndVerifyUi();
}
#endif  // BUILDFLAG(IS_MAC)

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

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_cable_v2_connecting) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_cable_v2_connected) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_cable_v2_error) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_phone_aoa) {
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

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_single_account_select) {
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

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_server_link_title_UNLOCK_YOUR_PHONE) {
  ShowAndVerifyUi();
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_ble_permission_mac) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_create_passkey) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_phone_confirmation) {
  ShowAndVerifyUi();
}

// Run with:
//
//   --gtest_filter=BrowserUiTest.Invoke --test-launcher-interactive \
//   --ui=GPMPasskeysAuthenticatorDialogTest.InvokeUi_${test_name}
//
// where test_name is the second arg to IN_PROC_BROWSER_TEST_F().
class GPMPasskeysAuthenticatorDialogTest : public AuthenticatorDialogTest {
 public:
  // AuthenticatorDialogTest:
  void ShowUi(const std::string& name) override {
    // Web modal dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    model_ = std::make_unique<AuthenticatorRequestDialogModel>(
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetPrimaryMainFrame());
    model_->set_relying_party_id("example.com");

    device::FidoRequestHandlerBase::TransportAvailabilityInfo&
        transport_availability = model_->transport_availability_for_testing();
    transport_availability.request_type =
        device::FidoRequestType::kGetAssertion;
    transport_availability.ble_access_denied = false;
    transport_availability.available_transports = {
        AuthenticatorTransport::kUsbHumanInterfaceDevice,
        AuthenticatorTransport::kInternal,
        AuthenticatorTransport::kHybrid,
        AuthenticatorTransport::kAndroidAccessory,
    };

    device::DiscoverableCredentialMetadata local_cred1(
        device::AuthenticatorType::kTouchID, "example.com", {1},
        device::PublicKeyCredentialUserEntity({1}, "elisa.g.beckett@gmail.com",
                                              "Elisa Beckett"));
    device::DiscoverableCredentialMetadata local_cred2(
        device::AuthenticatorType::kTouchID, "example.com", {2},
        device::PublicKeyCredentialUserEntity({2}, "elisa.beckett@ink-42.com",
                                              "Elisa Beckett"));
    device::DiscoverableCredentialMetadata phone_cred1(
        device::AuthenticatorType::kPhone, "example.com", {3},
        device::PublicKeyCredentialUserEntity({1}, "elisa.g.beckett@gmail.com",
                                              "Elisa Beckett"));
    device::DiscoverableCredentialMetadata phone_cred2(
        device::AuthenticatorType::kPhone, "example.com", {4},
        device::PublicKeyCredentialUserEntity({2}, "elisa.beckett@ink-42.com",
                                              "Elisa Beckett"));

    // Configure a phone from sync.
    std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
    auto phone = std::make_unique<device::cablev2::Pairing>();
    phone->from_sync_deviceinfo = true;
    phone->name = kPhoneName;
    phones.emplace_back(std::move(phone));
    model_->set_cable_transport_info(
        /*extension_is_v2=*/absl::nullopt, std::move(phones),
        /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");

    if (name == "local_and_phone") {
      transport_availability.recognized_credentials = {
          std::move(local_cred1),
          std::move(local_cred2),
          std::move(phone_cred1),
          std::move(phone_cred2),
      };
    } else if (name == "local_only") {
      transport_availability.recognized_credentials = {
          std::move(local_cred1),
          std::move(local_cred2),
      };
    } else if (name == "phone_only") {
      transport_availability.recognized_credentials = {
          std::move(phone_cred1),
          std::move(phone_cred2),
      };
    } else if (name == "one_local_cred") {
      transport_availability.recognized_credentials = {
          std::move(local_cred1),
      };
    } else if (name == "one_phone_cred") {
      transport_availability.recognized_credentials = {
          std::move(phone_cred1),
      };
    } else if (name == "get_assertion_qr_with_usb") {
      model_->set_cable_transport_info(
          /*extension_is_v2=*/absl::nullopt,
          /*paired_phones=*/{},
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      transport_availability.is_ble_powered = true;
      transport_availability.available_transports = {
          AuthenticatorTransport::kHybrid,
          AuthenticatorTransport::kUsbHumanInterfaceDevice,
      };
    } else if (name == "get_assertion_qr_without_usb") {
      model_->set_cable_transport_info(
          /*extension_is_v2=*/absl::nullopt,
          /*paired_phones=*/{},
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      transport_availability.is_ble_powered = true;
      transport_availability.available_transports = {
          AuthenticatorTransport::kHybrid,
      };
    } else if (name == "make_credential_qr_with_usb") {
      model_->set_cable_transport_info(
          /*extension_is_v2=*/absl::nullopt,
          /*paired_phones=*/{},
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      transport_availability.request_type =
          device::FidoRequestType::kMakeCredential;
      transport_availability.is_ble_powered = true;
      transport_availability.available_transports = {
          AuthenticatorTransport::kHybrid,
          AuthenticatorTransport::kUsbHumanInterfaceDevice,
      };
    } else if (name == "make_credential_qr_without_usb") {
      model_->set_cable_transport_info(
          /*extension_is_v2=*/absl::nullopt,
          /*paired_phones=*/{},
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      transport_availability.request_type =
          device::FidoRequestType::kMakeCredential;
      transport_availability.is_ble_powered = true;
      transport_availability.available_transports = {
          AuthenticatorTransport::kHybrid,
      };
    }
    model_->StartFlow(std::move(transport_availability),
                      /*is_conditional_mediation=*/false);
  }

 private:
  std::unique_ptr<AuthenticatorRequestDialogModel> model_;
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnListSyncedPasskeys};
};

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_local_and_phone) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_local_only) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_phone_only) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_one_local_cred) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_one_phone_cred) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_get_assertion_qr_with_usb) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_get_assertion_qr_without_usb) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_make_credential_qr_with_usb) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_make_credential_qr_without_usb) {
  ShowAndVerifyUi();
}
