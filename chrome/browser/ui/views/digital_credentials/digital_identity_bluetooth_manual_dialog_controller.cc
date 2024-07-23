// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/digital_credentials/digital_identity_bluetooth_manual_dialog_controller.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/browser/digital_credentials/digital_identity_bluetooth_adapter_status_change_observer.h"
#include "chrome/browser/digital_credentials/digital_identity_fido_handler_observer.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_multi_step_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

using BleStatus = device::FidoRequestHandlerBase::BleStatus;

DigitalIdentityBluetoothManualDialogController::
    DigitalIdentityBluetoothManualDialogController(
        DigitalIdentityMultiStepDialog* dialog,
        DigitalIdentityFidoHandlerObserver* observer_registrar)
    : dialog_(dialog), observer_registrar_(observer_registrar) {
  observer_registrar_->AddBluetoothAdapterStatusChangeObserver(this);
}

DigitalIdentityBluetoothManualDialogController::
    ~DigitalIdentityBluetoothManualDialogController() {
  observer_registrar_->RemoveBluetoothAdapterStatusChangeObserver(this);
}

void DigitalIdentityBluetoothManualDialogController::Show(
    base::RepeatingClosure accept_bluetooth_powered_on_callback,
    base::RepeatingClosure cancel_callback) {
  accept_bluetooth_powered_on_callback_ =
      std::move(accept_bluetooth_powered_on_callback);
  cancel_callback_ = std::move(cancel_callback);
  UpdateDialog();
}

void DigitalIdentityBluetoothManualDialogController::
    OnBluetoothAdapterStatusChanged(BleStatus ble_status) {
  is_ble_powered_ = (ble_status == BleStatus::kOn);
  UpdateDialog();
}

void DigitalIdentityBluetoothManualDialogController::UpdateDialog() {
  CHECK(accept_bluetooth_powered_on_callback_);
  CHECK(cancel_callback_);

  std::u16string dialog_title = l10n_util::GetStringUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_BLUETOOTH_POWER_ON_MANUAL_TITLE);
  std::u16string dialog_body = l10n_util::GetStringUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_BLUETOOTH_POWER_ON_MANUAL_DESCRIPTION);
  std::u16string ok_button_text = l10n_util::GetStringUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_BLUETOOTH_POWER_ON_MANUAL_NEXT);
  std::optional<ui::DialogModel::Button::Params> ok_button_params =
      std::make_optional<ui::DialogModel::Button::Params>();
  ok_button_params->SetLabel(ok_button_text);
  ok_button_params->SetEnabled(is_ble_powered_);

  dialog_->TryShow(ok_button_params, accept_bluetooth_powered_on_callback_,
                   ui::DialogModel::Button::Params(), cancel_callback_,
                   dialog_title, dialog_body,
                   /*custom_body_field=*/nullptr);
}
