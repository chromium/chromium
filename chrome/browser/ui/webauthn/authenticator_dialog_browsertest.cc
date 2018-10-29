// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/webauthn/authenticator_reference.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

class AuthenticatorDialogTest : public DialogBrowserTest {
 public:
  AuthenticatorDialogTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Web modal dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    auto model = std::make_unique<AuthenticatorRequestDialogModel>();
    ::device::FidoRequestHandlerBase::TransportAvailabilityInfo
        transport_availability;
    transport_availability.rp_id = "example.com";
    transport_availability.available_transports = {
        AuthenticatorTransport::kBluetoothLowEnergy,
        AuthenticatorTransport::kUsbHumanInterfaceDevice,
        AuthenticatorTransport::kNearFieldCommunication,
        AuthenticatorTransport::kInternal,
        AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy};
    model->StartFlow(std::move(transport_availability), base::nullopt);

    // The dialog should immediately close as soon as it is displayed.
    if (name == "closed") {
      model->SetCurrentStep(AuthenticatorRequestDialogModel::Step::kClosed);
    } else if (name == "transports") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kTransportSelection);
    } else if (name == "activate_usb") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kUsbInsertAndActivate);
    } else if (name == "timeout") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kPostMortemTimedOut);
    } else if (name == "no_available_transports") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kErrorNoAvailableTransports);
    } else if (name == "key_not_registered") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kPostMortemKeyNotRegistered);
    } else if (name == "key_already_registered") {
      model->SetCurrentStep(AuthenticatorRequestDialogModel::Step::
                                kPostMortemKeyAlreadyRegistered);
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
          AuthenticatorTransport::kInternal, false /* is_in_pairing_mode */));
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kBlePinEntry);
    } else if (name == "ble_verifying") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kBleVerifying);
    } else if (name == "ble_activate") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kBleActivate);
    } else if (name == "touchid") {
      model->SetCurrentStep(AuthenticatorRequestDialogModel::Step::kTouchId);
    } else if (name == "cable_activate") {
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kCableActivate);
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

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_closed) {
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

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_touchid) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_cable_activate) {
  ShowAndVerifyUi();
}
