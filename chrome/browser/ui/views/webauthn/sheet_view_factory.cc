// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/sheet_view_factory.h"

#include "base/logging.h"
#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_model.h"
#include "chrome/browser/ui/views/webauthn/authenticator_ble_pin_entry_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_client_pin_entry_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_qr_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_select_account_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_transport_selector_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/ble_device_selection_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// A placeholder sheet to show in place of unimplemented sheets.
class PlaceholderSheetModel : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override {
    return gfx::kNoneIcon;
  }
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
    case Step::kTimedOut:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorTimeoutErrorModel>(dialog_model));
      break;
    case Step::kKeyNotRegistered:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorNotRegisteredErrorModel>(dialog_model));
      break;
    case Step::kKeyAlreadyRegistered:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorAlreadyRegisteredErrorModel>(
              dialog_model));
      break;
    case Step::kMissingCapability:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          AuthenticatorGenericErrorSheetModel::ForMissingCapability(
              dialog_model));
      break;
    case Step::kStorageFull:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          AuthenticatorGenericErrorSheetModel::ForStorageFull(dialog_model));
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
    case Step::kTouchIdIncognitoSpeedBump:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorTouchIdIncognitoBumpSheetModel>(
              dialog_model));
      break;
    case Step::kCableActivate:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorPaaskSheetModel>(dialog_model));
      break;
    case Step::kClientPinEntry:
      sheet_view = std::make_unique<AuthenticatorClientPinEntrySheetView>(
          std::make_unique<AuthenticatorClientPinEntrySheetModel>(
              dialog_model,
              AuthenticatorClientPinEntrySheetModel::Mode::kPinEntry));
      break;
    case Step::kClientPinSetup:
      sheet_view = std::make_unique<AuthenticatorClientPinEntrySheetView>(
          std::make_unique<AuthenticatorClientPinEntrySheetModel>(
              dialog_model,
              AuthenticatorClientPinEntrySheetModel::Mode::kPinSetup));
      break;
    case Step::kClientPinTapAgain:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorClientPinTapAgainSheetModel>(
              dialog_model));
      break;
    case Step::kClientPinErrorSoftBlock:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          AuthenticatorGenericErrorSheetModel::ForClientPinErrorSoftBlock(
              dialog_model));
      break;
    case Step::kClientPinErrorHardBlock:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          AuthenticatorGenericErrorSheetModel::ForClientPinErrorHardBlock(
              dialog_model));
      break;
    case Step::kClientPinErrorAuthenticatorRemoved:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          AuthenticatorGenericErrorSheetModel::
              ForClientPinErrorAuthenticatorRemoved(dialog_model));
      break;
    case Step::kResidentCredentialConfirmation:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<
              AuthenticatorResidentCredentialConfirmationSheetView>(
              dialog_model));
      break;
    case Step::kSelectAccount:
      sheet_view = std::make_unique<AuthenticatorSelectAccountSheetView>(
          std::make_unique<AuthenticatorSelectAccountSheetModel>(dialog_model));
      break;
    case Step::kAttestationPermissionRequest:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AttestationPermissionRequestSheetModel>(
              dialog_model));
      break;
    case Step::kQRCode:
      sheet_view = std::make_unique<AuthenticatorQRSheetView>(
          std::make_unique<AuthenticatorQRSheetModel>(dialog_model));
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

std::unique_ptr<AuthenticatorRequestSheetView>
CreateSheetViewForAutofillWebAuthn(
    std::unique_ptr<autofill::WebauthnOfferDialogModel> model) {
  return std::make_unique<AuthenticatorRequestSheetView>(std::move(model));
}
