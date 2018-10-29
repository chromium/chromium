// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/sheet_view_factory.h"

#include "base/logging.h"
#include "chrome/browser/ui/views/webauthn/authenticator_ble_pin_entry_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_transport_selector_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/ble_device_selection_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

namespace {

// A placeholder sheet to show in place of unimplemented sheets.
class PlaceholderSheetModel : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  gfx::ImageSkia* GetStepIllustration() const override { return nullptr; }
  base::string16 GetStepTitle() const override { return base::string16(); }
  base::string16 GetStepDescription() const override {
    return base::string16();
  }
};

}  // namespace

std::unique_ptr<AuthenticatorRequestSheetView> CreateSheetViewForCurrentStepOf(
    AuthenticatorRequestDialogModel* dialog_model) {
  using Step = AuthenticatorRequestDialogModel::Step;

  std::unique_ptr<AuthenticatorRequestSheetView> sheet_view;
  switch (dialog_model->current_step()) {
    case Step::kWelcomeScreen:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorWelcomeSheetModel>(dialog_model));
      break;
    case Step::kTransportSelection:
      sheet_view = std::make_unique<AuthenticatorTransportSelectorSheetView>(
          std::make_unique<AuthenticatorTransportSelectorSheetModel>(
              dialog_model));
      break;
    case Step::kUsbInsertAndActivate:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorInsertAndActivateUsbSheetModel>(
              dialog_model));
      break;
    case Step::kErrorNoAvailableTransports:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorNoAvailableTransportsErrorModel>(
              dialog_model));
      break;
    case Step::kPostMortemTimedOut:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorTimeoutErrorModel>(dialog_model));
      break;
    case Step::kPostMortemKeyNotRegistered:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorNotRegisteredErrorModel>(dialog_model));
      break;
    case Step::kPostMortemKeyAlreadyRegistered:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorAlreadyRegisteredErrorModel>(
              dialog_model));
      break;
    case Step::kErrorInternalUnrecognized:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorInternalUnrecognizedErrorSheetModel>(
              dialog_model));
      break;
    case Step::kBlePowerOnAutomatic:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorBlePowerOnAutomaticSheetModel>(
              dialog_model));
      break;
    case Step::kBlePowerOnManual:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorBlePowerOnManualSheetModel>(
              dialog_model));
      break;
    case Step::kBlePairingBegin:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorBlePairingBeginSheetModel>(
              dialog_model));
      break;
    case Step::kBleEnterPairingMode:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorBleEnterPairingModeSheetModel>(
              dialog_model));
      break;
    case Step::kBleDeviceSelection:
      sheet_view = std::make_unique<BleDeviceSelectionSheetView>(
          std::make_unique<AuthenticatorBleDeviceSelectionSheetModel>(
              dialog_model));
      break;
    case Step::kBlePinEntry:
      sheet_view = std::make_unique<AuthenticatorBlePinEntrySheetView>(
          std::make_unique<AuthenticatorBlePinEntrySheetModel>(dialog_model));
      break;
    case Step::kBleVerifying:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorBleVerifyingSheetModel>(dialog_model));
      break;
    case Step::kBleActivate:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorBleActivateSheetModel>(dialog_model));
      break;
    case Step::kTouchId:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorTouchIdSheetModel>(dialog_model));
      break;
    case Step::kCableActivate:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorPaaskSheetModel>(dialog_model));
      break;
    case Step::kNotStarted:
    case Step::kClosed:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<PlaceholderSheetModel>(dialog_model));
      break;
  }

  CHECK(sheet_view);
  return sheet_view;
}
