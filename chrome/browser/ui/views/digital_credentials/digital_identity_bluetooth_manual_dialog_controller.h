// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_BLUETOOTH_MANUAL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_BLUETOOTH_MANUAL_DIALOG_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class DigitalIdentityMultiStepDialog;

// Displays step asking user to manually turn on bluetooth.
class DigitalIdentityBluetoothManualDialogController {
 public:
  explicit DigitalIdentityBluetoothManualDialogController(
      DigitalIdentityMultiStepDialog* dialog);
  ~DigitalIdentityBluetoothManualDialogController();

  void Show(base::OnceClosure user_requested_bluetooth_power_on_callback,
            base::OnceClosure cancel_callback);

 private:
  void UpdateDialog(bool enabled);
  void OnAccept();
  void OnCancel();

  // Owned by DigitalIdentityProviderDesktop.
  const raw_ptr<DigitalIdentityMultiStepDialog> dialog_;

  base::OnceClosure user_requested_blueooth_power_on_callback_;
  base::OnceClosure cancel_callback_;

  base::WeakPtrFactory<DigitalIdentityBluetoothManualDialogController>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_BLUETOOTH_MANUAL_DIALOG_CONTROLLER_H_
