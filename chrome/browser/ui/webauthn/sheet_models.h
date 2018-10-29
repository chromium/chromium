// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_
#define CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

namespace ui {
class MenuModel;
}

class OtherTransportsMenuModel;

// Base class for sheets, implementing the shared behavior used on most sheets,
// as well as maintaining a weak pointer to the dialog model.
class AuthenticatorSheetModelBase
    : public AuthenticatorRequestSheetModel,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit AuthenticatorSheetModelBase(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorSheetModelBase() override;

  AuthenticatorRequestDialogModel* dialog_model() const {
    return dialog_model_;
  }

 protected:
  // Pulls the image with the given |resource_id| from the resource bundle and
  // loads it in Skia format.
  static gfx::ImageSkia* GetImage(int resource_id);

  // Returns a string containing the relying party id for this request.
  base::string16 GetRelyingPartyIdString() const;

  // AuthenticatorRequestSheetModel:
  bool IsActivityIndicatorVisible() const override;
  bool IsBackButtonVisible() const override;
  bool IsCancelButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;
  void OnBack() override;
  void OnAccept() override;
  void OnCancel() override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed() override;

 private:
  AuthenticatorRequestDialogModel* dialog_model_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorSheetModelBase);
};

// The initial sheet shown when the UX flow starts.
class AuthenticatorWelcomeSheetModel : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  void OnAccept() override;
};

// The sheet shown for selecting the transport over which the security key
// should be accessed.
class AuthenticatorTransportSelectorSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

  // Initiates the step-by-step flow with the the transport at the given |index|
  // selected by the user.
  void OnTransportSelected(AuthenticatorTransport transport);

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

class AuthenticatorInsertAndActivateUsbSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorInsertAndActivateUsbSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorInsertAndActivateUsbSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;

  std::unique_ptr<OtherTransportsMenuModel> other_transports_menu_model_;
};

class AuthenticatorTimeoutErrorModel : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

class AuthenticatorNoAvailableTransportsErrorModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

class AuthenticatorNotRegisteredErrorModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

class AuthenticatorAlreadyRegisteredErrorModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

class AuthenticatorInternalUnrecognizedErrorSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

class AuthenticatorBlePowerOnManualSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  void OnAccept() override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnBluetoothPoweredStateChanged() override;
};

class AuthenticatorBlePowerOnAutomaticSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  void OnAccept() override;

  bool busy_powering_on_ble_ = false;
};

class AuthenticatorBlePairingBeginSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
};

class AuthenticatorBleEnterPairingModeSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

class AuthenticatorBleDeviceSelectionSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

class AuthenticatorBlePinEntrySheetModel : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

  void SetPinCode(base::string16 pin_code);

 private:
  // AuthenticatorSheetModelBase:
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  void OnAccept() override;

  base::string16 pin_code_;
};

class AuthenticatorBleVerifyingSheetModel : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

class AuthenticatorBleActivateSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorBleActivateSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorBleActivateSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;

  std::unique_ptr<OtherTransportsMenuModel> other_transports_menu_model_;
};

class AuthenticatorTouchIdSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorTouchIdSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorTouchIdSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  bool IsBackButtonVisible() const override;
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;

  std::unique_ptr<OtherTransportsMenuModel> other_transports_menu_model_;
};

class AuthenticatorPaaskSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorPaaskSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorPaaskSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  gfx::ImageSkia* GetStepIllustration() const override;
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;

  std::unique_ptr<OtherTransportsMenuModel> other_transports_menu_model_;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_
