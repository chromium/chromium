// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/trusted_vault/features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/pin.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

constexpr char kPhoneName[] = "Elisa's Pixel 6 Pro";
using BleStatus = device::FidoRequestHandlerBase::BleStatus;

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

  void SetUpOnMainThread() override {
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(browser()->profile()),
        "user@example.com", signin::ConsentLevel::kSync);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Web modal dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    content::RenderFrameHost* rfh = browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetPrimaryMainFrame();
    model_ = base::MakeRefCounted<AuthenticatorRequestDialogModel>(rfh);
    model_->relying_party_id = "example.com";
    // Since this code tests UI, it is possible to do everything by configuring
    // just the Model. However, it's easier to do that via a Controller.
    controller_ = std::make_unique<AuthenticatorRequestDialogController>(
        model_.get(), rfh);

    device::FidoRequestHandlerBase::TransportAvailabilityInfo&
        transport_availability =
            controller_->transport_availability_for_testing();
    transport_availability.available_transports = {
        AuthenticatorTransport::kUsbHumanInterfaceDevice,
        AuthenticatorTransport::kInternal,
        AuthenticatorTransport::kHybrid,
    };

    std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
    auto phone = std::make_unique<device::cablev2::Pairing>();
    phone->from_sync_deviceinfo = false;
    phone->name = kPhoneName;
    phones.emplace_back(std::move(phone));
    transport_availability.has_platform_authenticator_credential = device::
        FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential;
    transport_availability.request_type =
        device::FidoRequestType::kGetAssertion;

    // The dialog should immediately close as soon as it is displayed.
    if (name == "mechanisms" || name == "mechanisms_disabled") {
      // A phone is configured so that the "Manage devices" button is shown.
      controller_->set_cable_transport_info(
          /*extension_is_v2=*/std::nullopt, std::move(phones),
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kMechanismSelection);
    } else if (name == "mechanisms_create" ||
               name == "mechanisms_create_disabled") {
      transport_availability.make_credential_attachment =
          device::AuthenticatorAttachment::kAny;
      transport_availability.request_type =
          device::FidoRequestType::kMakeCredential;
      transport_availability.attestation_conveyance_preference =
          device::AttestationConveyancePreference::kNone;
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kMechanismSelection);
    } else if (name == "activate_usb") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kUsbInsertAndActivate);
    } else if (name == "timeout") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kTimedOut);
    } else if (name == "no_available_transports") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kErrorNoAvailableTransports);
    } else if (name == "key_not_registered") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kKeyNotRegistered);
    } else if (name == "key_already_registered") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kKeyAlreadyRegistered);
    } else if (name == "windows_hello_not_enabled") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kErrorWindowsHelloNotEnabled);
    } else if (name == "internal_unrecognized_error") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kErrorInternalUnrecognized);
    } else if (name == "ble_power_on_manual") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kBlePowerOnManual);
    } else if (name == "touchid_incognito") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kOffTheRecordInterstitial);
    } else if (name == "cable_activate") {
      controller_->set_cable_transport_info(
          /*extension_is_v2=*/false, std::move(phones),
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      controller_->ContactPhoneForTesting(kPhoneName);
    } else if (name == "cable_v2_activate") {
      controller_->set_cable_transport_info(
          /*extension_is_v2=*/std::nullopt, std::move(phones),
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      controller_->ContactPhoneForTesting(kPhoneName);
    } else if (name == "cable_v2_pair") {
      controller_->set_cable_transport_info(
          /*extension_is_v2=*/std::nullopt,
          /*paired_phones=*/{},
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kCableV2QRCode);
    } else if (name == "cable_v2_connecting") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kCableV2Connecting);
    } else if (name == "cable_v2_connected") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kCableV2Connected);
    } else if (name == "cable_v2_error") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kCableV2Error);
    } else if (name == "set_pin") {
      controller_->CollectPIN(device::pin::PINEntryReason::kSet,
                              device::pin::PINEntryError::kNoError, 6, 0,
                              base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin") {
      controller_->CollectPIN(device::pin::PINEntryReason::kChallenge,
                              device::pin::PINEntryError::kNoError, 6, 8,
                              base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin_two_tries_remaining") {
      controller_->CollectPIN(device::pin::PINEntryReason::kChallenge,
                              device::pin::PINEntryError::kWrongPIN, 6, 2,
                              base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin_one_try_remaining") {
      controller_->CollectPIN(device::pin::PINEntryReason::kChallenge,
                              device::pin::PINEntryError::kWrongPIN, 6, 1,
                              base::BindOnce([](std::u16string pin) {}));
    } else if (name == "get_pin_fallback") {
      controller_->CollectPIN(device::pin::PINEntryReason::kChallenge,
                              device::pin::PINEntryError::kInternalUvLocked, 6,
                              8, base::BindOnce([](std::u16string pin) {}));
    } else if (name == "inline_bio_enrollment") {
      controller_->StartInlineBioEnrollment(base::DoNothing());
      timer_.Start(
          FROM_HERE, base::Seconds(2),
          base::BindLambdaForTesting(
              [&, weak_controller = controller_->GetWeakPtr()] {
                if (!weak_controller || weak_controller->model()->step() !=
                                            AuthenticatorRequestDialogModel::
                                                Step::kInlineBioEnrollment) {
                  return;
                }
                weak_controller->OnSampleCollected(--bio_samples_remaining_);
                if (bio_samples_remaining_ <= 0) {
                  timer_.Stop();
                }
              }));
    } else if (name == "retry_uv") {
      controller_->OnRetryUserVerification(5);
    } else if (name == "retry_uv_two_tries_remaining") {
      controller_->OnRetryUserVerification(2);
    } else if (name == "retry_uv_one_try_remaining") {
      controller_->OnRetryUserVerification(1);
    } else if (name == "force_pin_change") {
      controller_->CollectPIN(device::pin::PINEntryReason::kChange,
                              device::pin::PINEntryError::kNoError, 6, 0,
                              base::BindOnce([](std::u16string pin) {}));
    } else if (name == "force_pin_change_same_as_current") {
      controller_->CollectPIN(device::pin::PINEntryReason::kChange,
                              device::pin::PINEntryError::kSameAsCurrentPIN, 6,
                              0, base::BindOnce([](std::u16string pin) {}));
    } else if (name == "second_tap") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kClientPinTapAgain);
    } else if (name == "soft_block") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kClientPinErrorSoftBlock);
    } else if (name == "hard_block") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kClientPinErrorHardBlock);
    } else if (name == "authenticator_removed") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::
              kClientPinErrorAuthenticatorRemoved);
    } else if (name == "missing_capability") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kMissingCapability);
    } else if (name == "storage_full") {
      controller_->SetCurrentStepForTesting(
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
                                            kSignatureCounter, std::nullopt);
        device::AuthenticatorGetAssertionResponse response(
            std::move(auth_data), {10, 11, 12, 13} /* signature */,
            /*transport_used=*/std::nullopt);
        device::PublicKeyCredentialUserEntity user({1, 2, 3, 4});
        user.name = info.first;
        user.display_name = info.second;
        response.credential = device::PublicKeyCredentialDescriptor(
            device::CredentialType::kPublicKey, {1, 2, 3, 4});
        response.user_entity = std::move(user);
        responses.emplace_back(std::move(response));
      }

      controller_->SelectAccount(
          std::move(responses),
          base::BindOnce([](device::AuthenticatorGetAssertionResponse) {}));
    } else if (name == "account_select" || name == "account_select_disabled") {
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
                                            kSignatureCounter, std::nullopt);
        device::AuthenticatorGetAssertionResponse response(
            std::move(auth_data), {10, 11, 12, 13} /* signature */,
            /*transport_used=*/std::nullopt);
        device::PublicKeyCredentialUserEntity user({1, 2, 3, 4});
        user.name = info.first;
        user.display_name = info.second;
        response.credential = device::PublicKeyCredentialDescriptor(
            device::CredentialType::kPublicKey, {1, 2, 3, 4});
        response.user_entity = std::move(user);
        responses.emplace_back(std::move(response));
      }

      controller_->SelectAccount(
          std::move(responses),
          base::BindOnce([](device::AuthenticatorGetAssertionResponse) {}));
    } else if (name == "server_link_title_UNLOCK_YOUR_PHONE") {
      controller_->set_cable_transport_info(
          /*extension_is_v2=*/true, /*paired_phones=*/{},
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kCableActivate);
    } else if (name == "create_passkey") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kCreatePasskey);
    } else if (name == "phone_confirmation") {
      // The phone must be from Sync.
      phones[0]->from_sync_deviceinfo = true;
      controller_->set_cable_transport_info(
          /*extension_is_v2=*/true, std::move(phones),
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kPhoneConfirmationSheet);
    }
#if BUILDFLAG(IS_MAC)
    else if (name == "ble_permission_mac") {  // NOLINT
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kBlePermissionMac);
    }
#endif

    controller_->StartFlow(std::move(transport_availability),
                           /*is_conditional_mediation=*/false);
    if (name.ends_with("_disabled")) {
      model_->ui_disabled_ = true;
      model_->OnSheetModelChanged();
    }
  }

 private:
  scoped_refptr<AuthenticatorRequestDialogModel> model_;
  std::unique_ptr<AuthenticatorRequestDialogController> controller_;
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

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_mechanisms_create) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_mechanisms_disabled) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_mechanisms_create_disabled) {
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
                       InvokeUi_account_select_disabled) {
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
  GPMPasskeysAuthenticatorDialogTest() {
    scoped_feature_list_.InitWithFeatures(
        {syncer::kSyncWebauthnCredentials,
         device::kWebAuthnEnclaveAuthenticator},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(browser()->profile()),
        "user@example.com", signin::ConsentLevel::kSync);
  }

  // AuthenticatorDialogTest:
  void ShowUi(const std::string& name) override {
    // Web modal dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    content::RenderFrameHost* rfh = browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetPrimaryMainFrame();
    model_ = base::MakeRefCounted<AuthenticatorRequestDialogModel>(rfh);
    model_->relying_party_id = "example.com";
    controller_ = std::make_unique<AuthenticatorRequestDialogController>(
        model_.get(), rfh);
    controller_->SetAccountPreselectedCallback(base::DoNothing());

    device::FidoRequestHandlerBase::TransportAvailabilityInfo&
        transport_availability =
            controller_->transport_availability_for_testing();
    transport_availability.request_type =
        device::FidoRequestType::kGetAssertion;
    transport_availability.available_transports = {
        AuthenticatorTransport::kUsbHumanInterfaceDevice,
        AuthenticatorTransport::kInternal,
        AuthenticatorTransport::kHybrid,
    };

    device::DiscoverableCredentialMetadata gpm_cred(
        device::AuthenticatorType::kEnclave, "example.com", {1},
        device::PublicKeyCredentialUserEntity({1}, "elisa.g.beckett@gmail.com",
                                              "Elisa Beckett"));
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
    model_->user_entity = local_cred1.user;

    // Configure a phone from sync.
    std::vector<std::unique_ptr<device::cablev2::Pairing>> phones;
    auto phone = std::make_unique<device::cablev2::Pairing>();
    phone->from_sync_deviceinfo = true;
    phone->name = kPhoneName;
    phones.emplace_back(std::move(phone));
    controller_->set_cable_transport_info(
        /*extension_is_v2=*/std::nullopt, std::move(phones),
        /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");

    if (name == "no_passkeys_discovered") {
      transport_availability.recognized_credentials = {};
    } else if (name == "local_and_phone") {
      transport_availability.recognized_credentials = {
          std::move(local_cred1),
          std::move(local_cred2),
          std::move(phone_cred1),
          std::move(phone_cred2),
      };
    } else if (name == "local_only" || name == "local_only_disabled") {
      transport_availability.recognized_credentials = {
          std::move(local_cred1),
          std::move(local_cred2),
      };
    } else if (name == "local_no_other_devices") {
      transport_availability.recognized_credentials = {
          std::move(local_cred1),
          std::move(local_cred2),
      };
      transport_availability.available_transports = {
          device::FidoTransportProtocol::kInternal};
    } else if (name == "phone_only") {
      transport_availability.recognized_credentials = {
          std::move(phone_cred1),
          std::move(phone_cred2),
      };
    } else if (name == "priority_mech" || name == "priority_mech_disabled") {
      transport_availability.has_empty_allow_list = true;
      transport_availability.recognized_credentials = {
          std::move(gpm_cred),
      };
    } else if (name == "one_phone_cred") {
      transport_availability.recognized_credentials = {
          std::move(phone_cred1),
      };
    } else if (name == "get_assertion_qr_with_usb") {
      controller_->set_cable_transport_info(
          /*extension_is_v2=*/std::nullopt,
          /*paired_phones=*/{},
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      transport_availability.ble_status = BleStatus::kOn;
      transport_availability.available_transports = {
          AuthenticatorTransport::kHybrid,
          AuthenticatorTransport::kUsbHumanInterfaceDevice,
      };
    } else if (name == "get_assertion_qr_without_usb") {
      controller_->set_cable_transport_info(
          /*extension_is_v2=*/std::nullopt,
          /*paired_phones=*/{},
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      transport_availability.ble_status = BleStatus::kOn;
      transport_availability.available_transports = {
          AuthenticatorTransport::kHybrid,
      };
    } else if (name == "make_credential_qr_with_usb") {
      controller_->set_cable_transport_info(
          /*extension_is_v2=*/std::nullopt,
          /*paired_phones=*/{},
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      transport_availability.request_type =
          device::FidoRequestType::kMakeCredential;
      transport_availability.attestation_conveyance_preference =
          device::AttestationConveyancePreference::kDirect;
      transport_availability.ble_status = BleStatus::kOn;
      transport_availability.available_transports = {
          AuthenticatorTransport::kHybrid,
          AuthenticatorTransport::kUsbHumanInterfaceDevice,
      };
    } else if (name == "make_credential_qr_without_usb") {
      controller_->set_cable_transport_info(
          /*extension_is_v2=*/std::nullopt,
          /*paired_phones=*/{},
          /*contact_phone_callback=*/base::DoNothing(), "fido://qrcode");
      transport_availability.request_type =
          device::FidoRequestType::kMakeCredential;
      transport_availability.attestation_conveyance_preference =
          device::AttestationConveyancePreference::kDirect;
      transport_availability.ble_status = BleStatus::kOn;
      transport_availability.available_transports = {
          AuthenticatorTransport::kHybrid,
      };
    } else if (name == "trust_this_computer_assertion") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kTrustThisComputerAssertion);
    } else if (name == "trust_this_computer_creation") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kTrustThisComputerCreation);
    } else if (name == "gpm_create_passkey") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey);
    } else if (name == "touchid") {
      transport_availability.request_type =
          device::FidoRequestType::kMakeCredential;
      transport_availability.attestation_conveyance_preference =
          device::AttestationConveyancePreference::kNone;
      transport_availability.make_credential_attachment =
          device::AuthenticatorAttachment::kAny;
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kGPMTouchID);
    } else if (name == "gpm_change_pin" || name == "gpm_change_pin_disabled") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kGPMChangePin);
    } else if (name == "gpm_create_pin") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
    } else if (name == "gpm_enter_pin") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kGPMEnterPin);
    } else if (name == "gpm_change_arbitrary_pin") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kGPMChangeArbitraryPin);
    } else if (name == "gpm_create_arbitrary_pin") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kGPMCreateArbitraryPin);
    } else if (name == "gpm_enter_arbitrary_pin") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kGPMEnterArbitraryPin);
    } else if (name == "gpm_error") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kGPMError);
    } else if (name == "gpm_connecting") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kGPMConnecting);
    } else if (name == "gpm_confirm_incognito_create") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kGPMConfirmOffTheRecordCreate);
    } else if (name == "gpm_locked_pin") {
      controller_->SetCurrentStepForTesting(
          AuthenticatorRequestDialogModel::Step::kGPMLockedPin);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
    controller_->StartFlow(std::move(transport_availability),
                           /*is_conditional_mediation=*/false);
    if (name.ends_with("_disabled")) {
      model_->ui_disabled_ = true;
      model_->OnSheetModelChanged();
    }
  }

 private:
  scoped_refptr<AuthenticatorRequestDialogModel> model_;
  std::unique_ptr<AuthenticatorRequestDialogController> controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_no_passkeys_discovered) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_local_and_phone) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_local_only) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_local_only_disabled) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_local_no_other_devices) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_phone_only) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_priority_mech) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_priority_mech_disabled) {
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

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_trust_this_computer_assertion) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_trust_this_computer_creation) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_gpm_create_passkey) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_gpm_change_pin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_gpm_change_pin_disabled) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_gpm_create_pin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_gpm_enter_pin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_gpm_change_arbitrary_pin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_gpm_create_arbitrary_pin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_gpm_enter_arbitrary_pin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest, InvokeUi_gpm_error) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_gpm_connecting) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_gpm_confirm_incognito_create) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest,
                       InvokeUi_gpm_locked_pin) {
  ShowAndVerifyUi();
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(GPMPasskeysAuthenticatorDialogTest, InvokeUi_touchid) {
  if (__builtin_available(macos 12, *)) {
    ShowAndVerifyUi();
  }
}
#endif  // BUILDFLAG(IS_MAC)

// Tests the UI steps that show a pop-up window.
class AuthenticatorWindowTest : public InProcessBrowserTest {
 public:
  AuthenticatorWindowTest() {
    scoped_feature_list_.InitWithFeatures(
        {device::kWebAuthnEnclaveAuthenticator},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&AuthenticatorWindowTest::HandleNetworkRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(https_server_.InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    command_line->AppendSwitchASCII(switches::kGaiaUrl,
                                    https_server_.base_url().spec());
    command_line->AppendSwitchASCII(
        webauthn::switches::kGpmPinResetReauthUrlSwitch,
        https_server_.GetURL("/encryption/pin/reset").spec());
  }

  void SetUpOnMainThread() override {
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    https_server_.StartAcceptingConnections();
    host_resolver()->AddRule("*", "127.0.0.1");

    model_ = base::MakeRefCounted<AuthenticatorRequestDialogModel>(
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetPrimaryMainFrame());
  }

 protected:
  scoped_refptr<AuthenticatorRequestDialogModel> model_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleNetworkRequest(
      const net::test_server::HttpRequest& request) {
    const GURL url = request.GetURL();
    const std::string_view path = url.path_piece();
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();

    if (path == "/encryption/unlock/desktop") {
      response->set_code(net::HTTP_OK);
      response->set_content(R"(<html><head><title>Test MagicArch</title>
<script>
document.addEventListener('DOMContentLoaded', function() {
  chrome.setClientEncryptionKeys(
      function() {},
      "1234",
      new Map([["hw_protected", [{epoch: 1, key: new ArrayBuffer(32)}]]]));
});
</script></head><body><p>Test MagicArch</p></body></html>)");
    } else if (path == "/encryption/pin/reset") {
      response->set_code(net::HTTP_OK);
      response->set_content(R"(<html><head><title>Test Reauth</title>
<script>
document.addEventListener('DOMContentLoaded', function() {
  const url = new URL(window.location.href);
  if (url.searchParams.get("rapt") === null) {
    url.searchParams.set("rapt", "RAPT");
    window.location.href = url.href;
  }
});
</script></head><body><p>Test Reauth</p></body></html>)");
    } else {
      LOG(ERROR) << "Unknown network request: " << url.spec();
      response->set_code(net::HTTP_NOT_FOUND);
    }

    return response;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

#if !BUILDFLAG(IS_CHROMEOS)
// This test doesn't work on Chrome OS because
// `trusted_vault_encryption_key_tab_helper.cc` will not send the keys to the
// EnclaveManager, since Chrome OS doesn't use the enclave.

// Quits the browser (and thus finishes the test) when keys are received by the
// EnclaveManager.
class QuitBrowserWhenKeysStored : public EnclaveManager::Observer {
 public:
  explicit QuitBrowserWhenKeysStored(Browser* browser) : browser_(browser) {
    EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser_->profile())
        ->AddObserver(this);
  }

  // EnclaveManager::Observer
  void OnKeysStored() override {
    LOG(INFO) << "QuitBrowserWhenKeysStored::OnKeysStored";
    EnclaveManagerFactory::GetAsEnclaveManagerForProfile(browser_->profile())
        ->RemoveObserver(this);
    browser_ = nullptr;

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&chrome::AttemptExit));
  }

 private:
  raw_ptr<Browser> browser_;
};

IN_PROC_BROWSER_TEST_F(AuthenticatorWindowTest, RecoverSecurityDomain) {
  QuitBrowserWhenKeysStored observer(browser());

  // This should open a pop-up to MagicArch. The fake MagicArch, configured
  // by this test class, will immediately return keys, which will cause the
  // browser to exit.
  model_->SetStep(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);

  RunUntilBrowserProcessQuits();
}
#endif

class QuitBrowserWhenReauthTokenReceived
    : public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit QuitBrowserWhenReauthTokenReceived(
      AuthenticatorRequestDialogModel* model)
      : model_(model) {
    model_->observers.AddObserver(this);
  }

  // AuthenticatorRequestDialogModel::Observer
  void OnReauthComplete(std::string token) override {
    LOG(INFO) << "QuitBrowserWhenKeysStored::OnReauthComplete";
    CHECK_EQ(token, "RAPT");
    model_->observers.RemoveObserver(this);
    model_ = nullptr;

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&chrome::AttemptExit));
  }

 private:
  raw_ptr<AuthenticatorRequestDialogModel> model_;
};

IN_PROC_BROWSER_TEST_F(AuthenticatorWindowTest, ReauthForPinReset) {
  QuitBrowserWhenReauthTokenReceived observer(model_.get());

  // This should open a pop-up to a GAIA reauth page. That page will be faked
  // by this test class and the fake will immediately complete with a token
  // with the value "RAPT". That will cause `QuitBrowserWhenReauthTokenReceived`
  // to close the browser and complete the test.
  model_->SetStep(AuthenticatorRequestDialogModel::Step::kGPMReauthForPinReset);

  RunUntilBrowserProcessQuits();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorWindowTest, UINavigatesAway) {
  // Test that closing the window (e.g. due to a timeout) doesn't cause any
  // issues.
  model_->SetStep(
      AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain);
  model_->SetStep(AuthenticatorRequestDialogModel::Step::kNotStarted);
}
