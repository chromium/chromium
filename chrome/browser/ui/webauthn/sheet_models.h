// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_
#define CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/browser/ui/webauthn/transport_hover_list_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "device/fido/pin.h"

namespace gfx {
struct VectorIcon;
}

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

  // Returns a string containing the RP ID, styled as an origin, truncated to a
  // reasonable width.
  static std::u16string GetRelyingPartyIdString(
      const AuthenticatorRequestDialogModel* dialog_model);

 protected:
  // AuthenticatorRequestSheetModel:
  bool IsActivityIndicatorVisible() const override;
  bool IsBackButtonVisible() const override;
  bool IsCancelButtonVisible() const override;
  std::u16string GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnBack() override;
  void OnAccept() override;
  void OnCancel() override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;

 private:
  AuthenticatorRequestDialogModel* dialog_model_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorSheetModelBase);
};

// The sheet shown for selecting the transport over which the security key
// should be accessed.
class AuthenticatorTransportSelectorSheetModel
    : public AuthenticatorSheetModelBase,
      public TransportHoverListModel::Delegate {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

  // TransportHoverListModel::Delegate:
  void OnTransportSelected(AuthenticatorTransport transport) override;
  void StartWinNativeApi() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
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
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::u16string GetAdditionalDescription() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;

  std::unique_ptr<OtherTransportsMenuModel> other_transports_menu_model_;
};

class AuthenticatorTimeoutErrorModel : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  std::u16string GetCancelButtonLabel() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorNoAvailableTransportsErrorModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  std::u16string GetCancelButtonLabel() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorNotRegisteredErrorModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  std::u16string GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  void OnAccept() override;
};

class AuthenticatorAlreadyRegisteredErrorModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  std::u16string GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  void OnAccept() override;
};

class AuthenticatorInternalUnrecognizedErrorSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  void OnAccept() override;
};

class AuthenticatorBlePowerOnManualSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
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
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;

  bool busy_powering_on_ble_ = false;
};

class AuthenticatorOffTheRecordInterstitialSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorOffTheRecordInterstitialSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorOffTheRecordInterstitialSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetCancelButtonLabel() const override;
  void OnAccept() override;

  std::unique_ptr<OtherTransportsMenuModel> other_transports_menu_model_;
};

class AuthenticatorPaaskSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorPaaskSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorPaaskSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  bool IsActivityIndicatorVisible() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;

  std::unique_ptr<OtherTransportsMenuModel> other_transports_menu_model_;
};

class AuthenticatorAndroidAccessorySheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorAndroidAccessorySheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorAndroidAccessorySheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  bool IsActivityIndicatorVisible() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;

  std::unique_ptr<OtherTransportsMenuModel> other_transports_menu_model_;
};

class AuthenticatorPaaskV2SheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorPaaskV2SheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorPaaskV2SheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsBackButtonVisible() const override;
  bool IsActivityIndicatorVisible() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  ui::MenuModel* GetOtherTransportsMenuModel() override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;

  std::unique_ptr<OtherTransportsMenuModel> other_transports_menu_model_;
};

class AuthenticatorClientPinEntrySheetModel
    : public AuthenticatorSheetModelBase {
 public:
  // Indicates whether the view should accommodate changing an existing PIN,
  // setting up a new PIN or entering an existing one.
  enum class Mode { kPinChange, kPinEntry, kPinSetup };
  AuthenticatorClientPinEntrySheetModel(
      AuthenticatorRequestDialogModel* dialog_model,
      Mode mode,
      device::pin::PINEntryError error);
  ~AuthenticatorClientPinEntrySheetModel() override;

  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

  void SetPinCode(std::u16string pin_code);
  void SetPinConfirmation(std::u16string pin_confirmation);

  Mode mode() const { return mode_; }

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::u16string GetError() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;

  std::u16string pin_code_;
  std::u16string pin_confirmation_;
  std::u16string error_;
  const Mode mode_;
};

class AuthenticatorClientPinTapAgainSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorClientPinTapAgainSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorClientPinTapAgainSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::u16string GetAdditionalDescription() const override;
};

class AuthenticatorBioEnrollmentSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorBioEnrollmentSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorBioEnrollmentSheetModel() override;

  int max_bio_samples() {
    return dialog_model()->max_bio_samples().value_or(1);
  }
  int bio_samples_remaining() {
    return dialog_model()->bio_samples_remaining().value_or(1);
  }

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonEnabled() const override;
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  bool IsCancelButtonVisible() const override;
  std::u16string GetCancelButtonLabel() const override;
  void OnAccept() override;
  void OnCancel() override;
};

class AuthenticatorRetryUvSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorRetryUvSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorRetryUvSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::u16string GetError() const override;
};

// Generic error dialog that allows starting the request over.
class AuthenticatorGenericErrorSheetModel : public AuthenticatorSheetModelBase {
 public:
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel>
  ForClientPinErrorSoftBlock(AuthenticatorRequestDialogModel* dialog_model);
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel>
  ForClientPinErrorHardBlock(AuthenticatorRequestDialogModel* dialog_model);
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel>
  ForClientPinErrorAuthenticatorRemoved(
      AuthenticatorRequestDialogModel* dialog_model);
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel>
  ForMissingCapability(AuthenticatorRequestDialogModel* dialog_model);
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel> ForStorageFull(
      AuthenticatorRequestDialogModel* dialog_model);

 private:
  AuthenticatorGenericErrorSheetModel(
      AuthenticatorRequestDialogModel* dialog_model,
      std::u16string title,
      std::u16string description);

  // AuthenticatorSheetModelBase:
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  bool IsBackButtonVisible() const override;
  std::u16string GetCancelButtonLabel() const override;
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  void OnAccept() override;

  std::u16string title_;
  std::u16string description_;
};

class AuthenticatorResidentCredentialConfirmationSheetView
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorResidentCredentialConfirmationSheetView(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorResidentCredentialConfirmationSheetView() override;

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  bool IsBackButtonVisible() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  void OnAccept() override;
};

// The sheet shown when the user needs to select an account.
class AuthenticatorSelectAccountSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorSelectAccountSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorSelectAccountSheetModel() override;

  // Set the index of the currently selected row.
  void SetCurrentSelection(int selected);

  // AuthenticatorSheetModelBase:
  void OnAccept() override;

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;

  size_t selected_ = 0;
};

class AttestationPermissionRequestSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AttestationPermissionRequestSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AttestationPermissionRequestSheetModel() override;

  // AuthenticatorSheetModelBase:
  void OnAccept() override;
  void OnCancel() override;

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsBackButtonVisible() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  bool IsCancelButtonVisible() const override;
  std::u16string GetCancelButtonLabel() const override;
};

class EnterpriseAttestationPermissionRequestSheetModel
    : public AttestationPermissionRequestSheetModel {
 public:
  explicit EnterpriseAttestationPermissionRequestSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorQRSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorQRSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorQRSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  const gfx::VectorIcon& GetStepIllustration(
      ImageColorScheme color_scheme) const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_
