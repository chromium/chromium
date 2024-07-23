// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_BLUETOOTH_MANUAL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_BLUETOOTH_MANUAL_DIALOG_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/digital_credentials/digital_identity_bluetooth_adapter_status_change_observer.h"
#include "device/fido/digital_identity_request_handler.h"
#include "device/fido/fido_request_handler_base.h"

class DigitalIdentityMultiStepDialog;
class DigitalIdentityFidoHandlerObserver;

// Displays step asking user to manually turn on bluetooth.
class DigitalIdentityBluetoothManualDialogController
    : public DigitalIdentityBluetoothAdapterStatusChangeObserver {
 public:
  DigitalIdentityBluetoothManualDialogController(
      DigitalIdentityMultiStepDialog* dialog,
      DigitalIdentityFidoHandlerObserver* observer_registrar);
  ~DigitalIdentityBluetoothManualDialogController() override;

  void Show(base::RepeatingClosure accept_bluetooth_powered_on_callback,
            base::RepeatingClosure cancel_callback);

  // DigitalIdentityBluetoothAdapterPowerChangedObserver:
  void OnBluetoothAdapterStatusChanged(
      device::FidoRequestHandlerBase::BleStatus ble_status) override;

 private:
  void UpdateDialog();

  // Whether bluetooth is powered.
  bool is_ble_powered_ = false;

  base::RepeatingClosure accept_bluetooth_powered_on_callback_;
  base::RepeatingClosure cancel_callback_;

  // Owned by DigitalIdentityProviderDesktop.
  raw_ptr<DigitalIdentityMultiStepDialog> dialog_;

  // Owned by DigitalIdentityProviderDesktop.
  raw_ptr<DigitalIdentityFidoHandlerObserver> observer_registrar_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_BLUETOOTH_MANUAL_DIALOG_CONTROLLER_H_
