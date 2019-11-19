// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "components/cbor/values.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/cable_discovery_data.h"
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
        AuthenticatorTransport::kBluetoothLowEnergy,
        AuthenticatorTransport::kUsbHumanInterfaceDevice,
        AuthenticatorTransport::kNearFieldCommunication,
        AuthenticatorTransport::kInternal,
        AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy};
    model->set_cable_transport_info(/*cable_extension_provided=*/true,
                                    /*have_paired_phones=*/false,
                                    device::CableDiscoveryData::NewQRKey());
    model->StartFlow(std::move(transport_availability), base::nullopt, nullptr);

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
    } else if (name == "ble_pairing_begin") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kBlePairingBegin);
    } else if (name == "ble_enter_pairing_mode") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kBleEnterPairingMode);
    } else if (name == "ble_device_selection") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kBleDeviceSelection);
    } else if (name == "ble_pin_entry") {
      model->SetSelectedAuthenticatorForTesting(AuthenticatorReference(
          "test_authenticator_id" /* authenticator_id */,
          base::string16() /* authenticator_display_name */,
          AuthenticatorTransport::kInternal, false /* is_in_pairing_mode */,
          false /* is_paired */, true /* requires_ble_pairing_pin */));
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kBlePinEntry);
    } else if (name == "ble_verifying") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kBleVerifying);
    } else if (name == "ble_activate") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kBleActivate);
    } else if (name == "touchid_incognito") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kTouchIdIncognitoSpeedBump);
    } else if (name == "cable_activate") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kCableActivate);
    } else if (name == "set_pin") {
      model->CollectPIN(base::nullopt, base::Bind([](std::string pin) {}));
    } else if (name == "get_pin") {
      model->CollectPIN(8, base::Bind([](std::string pin) {}));
    } else if (name == "get_pin_two_tries_remaining") {
      model->set_has_attempted_pin_entry_for_testing();
      model->CollectPIN(2, base::Bind([](std::string pin) {}));
    } else if (name == "get_pin_one_try_remaining") {
      model->set_has_attempted_pin_entry_for_testing();
      model->CollectPIN(1, base::Bind([](std::string pin) {}));
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
      const std::vector<std::pair<std::string, std::string>> infos = {
          {"foo@example.com", "Test User 1"},
          {"", "Test User 2"},
          {"", ""},
          {"bat@example.com", "Test User 4"},
          {"verylong@reallylongreallylongreallylongreallylongreallylong.com",
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
        response.SetUserEntity(std::move(user));
        responses.emplace_back(std::move(response));
      }

      model->SelectAccount(
          std::move(responses),
          base::Bind([](device::AuthenticatorGetAssertionResponse) {}));
    } else if (name == "request_attestation_permission") {
      model->RequestAttestationPermission(base::DoNothing());
    } else if (name == "qr_code") {
      model->SetCurrentStep(AuthenticatorRequestDialogModel::Step::kQRCode);
    }

    ShowAuthenticatorRequestDialog(
        browser()->tab_strip_model()->GetActiveWebContents(), std::move(model));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorDialogTest);
};

// Run with:
//   --gtest_filter=BrowserUiTest.Invoke --test-launcher-interactive \
//   --ui=AuthenticatorDialogTest.InvokeUi_default
IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_default) {
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

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_ble_pairing_begin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest,
                       InvokeUi_ble_enter_pairing_mode) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_ble_device_selection) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_ble_pin_entry) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_ble_verifying) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_ble_activate) {
  ShowAndVerifyUi();
}

#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_touchid) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_touchid_incognito) {
  ShowAndVerifyUi();
}
#endif  // defined(OS_MACOSX)

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_cable_activate) {
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

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_qr_code) {
  ShowAndVerifyUi();
}
