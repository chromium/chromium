// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/digital_credentials/digital_identity_bluetooth_manual_dialog_controller.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_multi_step_dialog.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_safety_interstitial_controller_desktop.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

DigitalIdentityBluetoothManualDialogController::
    DigitalIdentityBluetoothManualDialogController(
        DigitalIdentityMultiStepDialog* dialog)
    : dialog_(dialog) {}

DigitalIdentityBluetoothManualDialogController::
    ~DigitalIdentityBluetoothManualDialogController() = default;

void DigitalIdentityBluetoothManualDialogController::Show(
    base::OnceClosure user_requested_bluetooth_power_on_callback,
    base::OnceClosure cancel_callback) {
  user_requested_blueooth_power_on_callback_ =
      std::move(user_requested_bluetooth_power_on_callback);
  cancel_callback_ = std::move(cancel_callback);

  UpdateDialog(/*enabled=*/true);
}

void DigitalIdentityBluetoothManualDialogController::UpdateDialog(
    bool is_ok_button_enabled) {
  std::u16string dialog_title = l10n_util::GetStringUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_BLUETOOTH_POWER_ON_MANUAL_TITLE);
  std::u16string dialog_body = l10n_util::GetStringUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_BLUETOOTH_POWER_ON_MANUAL_DESCRIPTION);
  std::u16string ok_button_text = l10n_util::GetStringUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_BLUETOOTH_POWER_ON_MANUAL_NEXT);
  std::optional<ui::DialogModel::Button::Params> ok_button_params =
      std::make_optional<ui::DialogModel::Button::Params>();
  ok_button_params->SetLabel(ok_button_text);
  ok_button_params->SetEnabled(is_ok_button_enabled);

  dialog_->TryShow(
      ok_button_params,
      base::BindOnce(&DigitalIdentityBluetoothManualDialogController::OnAccept,
                     weak_factory_.GetWeakPtr()),
      ui::DialogModel::Button::Params(),
      base::BindOnce(&DigitalIdentityBluetoothManualDialogController::OnCancel,
                     weak_factory_.GetWeakPtr()),
      dialog_title, dialog_body,
      /*custom_body_field=*/nullptr);
}

void DigitalIdentityBluetoothManualDialogController::OnAccept() {
  CHECK(user_requested_blueooth_power_on_callback_);

  // Disable the dialog so that the user can't click the button twice.
  UpdateDialog(/*enabled=*/false);

  std::move(user_requested_blueooth_power_on_callback_).Run();
}

void DigitalIdentityBluetoothManualDialogController::OnCancel() {
  // The owner of this object should ensure that the user can't click other
  // buttons after the cancel callback, e.g. by destroying the dialog.
  CHECK(cancel_callback_);
  std::move(cancel_callback_).Run();
}
