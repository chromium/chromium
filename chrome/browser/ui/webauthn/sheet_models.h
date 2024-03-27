// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_
#define CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "device/fido/pin.h"

// Base class for sheets, implementing the shared behavior used on most sheets,
// as well as maintaining a weak pointer to the dialog model.
class AuthenticatorSheetModelBase
    : public AuthenticatorRequestSheetModel,
      public AuthenticatorRequestDialogController::Observer {
 public:
  // Determines whether the button in the lower-left corner of the dialog, to
  // display other available mechanisms, is shown on a sheet.
  enum class OtherMechanismButtonVisibility {
    // The button is not shown (default).
    kHidden,
    // The button is shown if there is more than one mechanism to choose from.
    kVisible,
  };

  explicit AuthenticatorSheetModelBase(
      AuthenticatorRequestDialogController* dialog_model);
  AuthenticatorSheetModelBase(
      AuthenticatorRequestDialogController* dialog_model,
      OtherMechanismButtonVisibility other_mechanism_button_visibility);

  AuthenticatorSheetModelBase(const AuthenticatorSheetModelBase&) = delete;
  AuthenticatorSheetModelBase& operator=(const AuthenticatorSheetModelBase&) =
      delete;

  ~AuthenticatorSheetModelBase() override;

  AuthenticatorRequestDialogController* dialog_model() const {
    return dialog_model_;
  }

  // Returns a string containing the RP ID, styled as an origin, truncated to a
  // reasonable width.
  static std::u16string GetRelyingPartyIdString(
      const AuthenticatorRequestDialogController* dialog_model);

 protected:
  // AuthenticatorRequestSheetModel:
  bool IsActivityIndicatorVisible() const override;
  bool IsCancelButtonVisible() const override;
  bool IsOtherMechanismButtonVisible() const override;
  std::u16string GetOtherMechanismButtonLabel() const override;
  std::u16string GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnBack() override;
  void OnAccept() override;
  void OnCancel() override;

  // AuthenticatorRequestDialogController::Observer:
  void OnModelDestroyed(AuthenticatorRequestDialogController* model) override;

 private:
  raw_ptr<AuthenticatorRequestDialogController> dialog_model_;
  OtherMechanismButtonVisibility other_mechanism_button_visibility_ =
      OtherMechanismButtonVisibility::kHidden;
};

// The sheet shown for selecting the transport over which the security key
// should be accessed.
class AuthenticatorMechanismSelectorSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorMechanismSelectorSheetModel(
      AuthenticatorRequestDialogController* dialog_model);

  // AuthenticatorSheetModelBase:
  bool IsManageDevicesButtonVisible() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  void OnManageDevices() override;
};

class AuthenticatorInsertAndActivateUsbSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorInsertAndActivateUsbSheetModel(
      AuthenticatorRequestDialogController* dialog_model);

  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::u16string GetAdditionalDescription() const override;
};

class AuthenticatorTimeoutErrorModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorTimeoutErrorModel(
      AuthenticatorRequestDialogController* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetCancelButtonLabel() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorNoAvailableTransportsErrorModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorNoAvailableTransportsErrorModel(
      AuthenticatorRequestDialogController* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetCancelButtonLabel() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorNoPasskeysErrorModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorNoPasskeysErrorModel(
      AuthenticatorRequestDialogController* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetCancelButtonLabel() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorNotRegisteredErrorModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorNotRegisteredErrorModel(
      AuthenticatorRequestDialogController* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  void OnAccept() override;
};

class AuthenticatorAlreadyRegisteredErrorModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorAlreadyRegisteredErrorModel(
      AuthenticatorRequestDialogController* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  void OnAccept() override;
};

class AuthenticatorInternalUnrecognizedErrorSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorInternalUnrecognizedErrorSheetModel(
      AuthenticatorRequestDialogController* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  void OnAccept() override;
};

class AuthenticatorBlePowerOnManualSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorBlePowerOnManualSheetModel(
      AuthenticatorRequestDialogController* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;

  // AuthenticatorRequestDialogController::Observer:
  void OnBluetoothPoweredStateChanged() override;
};

class AuthenticatorBlePowerOnAutomaticSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorBlePowerOnAutomaticSheetModel(
      AuthenticatorRequestDialogController* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;

  bool busy_powering_on_ble_ = false;
};

#if BUILDFLAG(IS_MAC)

class AuthenticatorBlePermissionMacSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorBlePermissionMacSheetModel(
      AuthenticatorRequestDialogController* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  bool IsCancelButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;
};

#endif  // IS_MAC

class AuthenticatorOffTheRecordInterstitialSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorOffTheRecordInterstitialSheetModel(
      AuthenticatorRequestDialogController* dialog_model);

  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetCancelButtonLabel() const override;
  void OnAccept() override;
};

class AuthenticatorPaaskSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorPaaskSheetModel(
      AuthenticatorRequestDialogController* dialog_model);
  ~AuthenticatorPaaskSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorAndroidAccessorySheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorAndroidAccessorySheetModel(
      AuthenticatorRequestDialogController* dialog_model);
  ~AuthenticatorAndroidAccessorySheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorClientPinEntrySheetModel
    : public AuthenticatorSheetModelBase {
 public:
  // Indicates whether the view should accommodate changing an existing PIN,
  // setting up a new PIN or entering an existing one.
  enum class Mode { kPinChange, kPinEntry, kPinSetup };
  AuthenticatorClientPinEntrySheetModel(
      AuthenticatorRequestDialogController* dialog_model,
      Mode mode,
      device::pin::PINEntryError error);
  ~AuthenticatorClientPinEntrySheetModel() override;

  void SetPinCode(std::u16string pin_code);
  void SetPinConfirmation(std::u16string pin_confirmation);

  Mode mode() const { return mode_; }

 private:
  // AuthenticatorSheetModelBase:
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
      AuthenticatorRequestDialogController* dialog_model);
  ~AuthenticatorClientPinTapAgainSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::u16string GetAdditionalDescription() const override;
};

class AuthenticatorBioEnrollmentSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorBioEnrollmentSheetModel(
      AuthenticatorRequestDialogController* dialog_model);
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
      AuthenticatorRequestDialogController* dialog_model);
  ~AuthenticatorRetryUvSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::u16string GetError() const override;
};

// Generic error dialog that allows starting the request over.
class AuthenticatorGenericErrorSheetModel : public AuthenticatorSheetModelBase {
 public:
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel>
  ForClientPinErrorSoftBlock(
      AuthenticatorRequestDialogController* dialog_model);
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel>
  ForClientPinErrorHardBlock(
      AuthenticatorRequestDialogController* dialog_model);
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel>
  ForClientPinErrorAuthenticatorRemoved(
      AuthenticatorRequestDialogController* dialog_model);
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel>
  ForMissingCapability(AuthenticatorRequestDialogController* dialog_model);
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel> ForStorageFull(
      AuthenticatorRequestDialogController* dialog_model);
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel>
  ForWindowsHelloNotEnabled(AuthenticatorRequestDialogController* dialog_model);

 private:
  AuthenticatorGenericErrorSheetModel(
      AuthenticatorRequestDialogController* dialog_model,
      std::u16string title,
      std::u16string description);

  // AuthenticatorSheetModelBase:
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetCancelButtonLabel() const override;
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
      AuthenticatorRequestDialogController* dialog_model);
  ~AuthenticatorResidentCredentialConfirmationSheetView() override;

 private:
  // AuthenticatorSheetModelBase:
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
  // Indicates whether the corresponding view is presented before or after
  // gathering user verification and generating an assertion signature.
  // `kPreUserVerification` is only possible with platform authenticators
  // for which we can silently enumerate credentials.
  enum UserVerificationMode {
    kPreUserVerification,
    kPostUserVerification,
  };

  // Whether the user needs to select an account from a list of many or they
  // merely need to confirm a single possible choice.
  enum SelectionType {
    kSingleAccount,
    kMultipleAccounts,
  };

  AuthenticatorSelectAccountSheetModel(
      AuthenticatorRequestDialogController* dialog_model,
      UserVerificationMode mode,
      SelectionType type);
  ~AuthenticatorSelectAccountSheetModel() override;

  SelectionType selection_type() const;

  // Returns the single available credential if `type()` is
  // `kSingleAccount` and must not be called otherwise.
  const device::DiscoverableCredentialMetadata& SingleCredential() const;

  // Set the index of the currently selected row. Only valid to call for
  // `kMultipleAccount`.
  void SetCurrentSelection(int selected);

  // AuthenticatorSheetModelBase:
  void OnAccept() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  std::u16string GetAcceptButtonLabel() const override;

  const UserVerificationMode user_verification_mode_;
  const SelectionType selection_type_;
  size_t selected_ = 0;
};

class AttestationPermissionRequestSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AttestationPermissionRequestSheetModel(
      AuthenticatorRequestDialogController* dialog_model);
  ~AttestationPermissionRequestSheetModel() override;

  // AuthenticatorSheetModelBase:
  void OnAccept() override;
  void OnCancel() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
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
      AuthenticatorRequestDialogController* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorQRSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorQRSheetModel(
      AuthenticatorRequestDialogController* dialog_model);
  ~AuthenticatorQRSheetModel() override;

  // Returns true if a label indicating the user that a security key may be used
  // should be shown.
  bool ShowSecurityKeyLabel() const;

  // Returns the label that indicates the user they can insert and activate a
  // hardware security key.
  std::u16string GetSecurityKeyLabel() const;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::u16string GetOtherMechanismButtonLabel() const override;
};

class AuthenticatorConnectingSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorConnectingSheetModel(
      AuthenticatorRequestDialogController* dialog_model);
  ~AuthenticatorConnectingSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorConnectedSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorConnectedSheetModel(
      AuthenticatorRequestDialogController* dialog_model);
  ~AuthenticatorConnectedSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorCableErrorSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorCableErrorSheetModel(
      AuthenticatorRequestDialogController* dialog_model);
  ~AuthenticatorCableErrorSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsOtherMechanismButtonVisible() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::u16string GetCancelButtonLabel() const override;
};

class AuthenticatorCreatePasskeySheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorCreatePasskeySheetModel(
      AuthenticatorRequestDialogController* dialog_model);
  ~AuthenticatorCreatePasskeySheetModel() override;

  // An additional label that `AuthenticatorCreatePasskeySheetView` includes in
  // the `BuildStepSpecificContent()` view.
  std::u16string passkey_storage_description() const;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  void OnAccept() override;
  std::u16string GetAcceptButtonLabel() const override;
};

// A confirmation screen that can be shown instead of the mechanism selection
// screen when we are confident a request can be resolved using an already
// paired phone.
class AuthenticatorPhoneConfirmationSheet : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorPhoneConfirmationSheet(
      AuthenticatorRequestDialogController* dialog_model);
  ~AuthenticatorPhoneConfirmationSheet() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  void OnAccept() override;
  std::u16string GetAcceptButtonLabel() const override;
};

// An account and mechanism picker that combines passkeys from multiple sources.
// Passkeys are grouped in two lists:
// * "Primary" passkeys. These are local passkeys if available, or GPM passkeys
//   if no local passkeys are available. Can be empty.
// * "Secondary" passkeys. These are all the other passkeys & mechanisms.
// AuthenticatorMultiSourcePickerSheetModel will filter these lists and
// present them as indices of AuthenticatorRequestDialogController's
// `mechanisms()` member.
class AuthenticatorMultiSourcePickerSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorMultiSourcePickerSheetModel(
      AuthenticatorRequestDialogController* dialog_model);
  ~AuthenticatorMultiSourcePickerSheetModel() override;

  // Returns a vector of indices to the "Primary" passkey mechanisms. Indices
  // correspond to AuthenticatorRequestDialogController's mechanisms().
  std::vector<int>& primary_passkey_indices() {
    return primary_passkey_indices_;
  }

  // Returns a vector of indices to the "Secondary" passkey mechanisms. Indices
  // correspond to AuthenticatorRequestDialogController's mechanisms().
  std::vector<int>& secondary_passkey_indices() {
    return secondary_passkey_indices_;
  }

  // Returns the user-visible label for the "Primary" passkey mechanisms.
  std::u16string& primary_passkeys_label() { return primary_passkeys_label_; }

 private:
  // AuthenticatorSheetModelBase:
  bool IsManageDevicesButtonVisible() const override;
  void OnManageDevices() override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;

  std::vector<int> primary_passkey_indices_;
  std::vector<int> secondary_passkey_indices_;
  std::u16string primary_passkeys_label_;
};

class AuthenticatorPriorityMechanismSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorPriorityMechanismSheetModel(
      AuthenticatorRequestDialogController* dialog_model);
  ~AuthenticatorPriorityMechanismSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonEnabled() const override;
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;
};

// The sheet shown when the user is entering a digit-only GPM pin.
class AuthenticatorGPMPinSheetModel : public AuthenticatorSheetModelBase {
 public:
  // Indicates whether the view should accommodate creating a new pin or
  // entering an existing one.
  enum class Mode { kPinCreate, kPinEntry };

  explicit AuthenticatorGPMPinSheetModel(
      AuthenticatorRequestDialogController* dialog_model,
      int pin_digits_count,
      Mode mode,
      AuthenticatorRequestDialogController::GpmPinError error);
  ~AuthenticatorGPMPinSheetModel() override;

  int pin_digits_count() const;

  // Sets currently typed pin in the sheet.
  void SetPin(std::u16string pin);

 private:
  bool FullPinTyped() const;

  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::u16string GetError() const override;
  bool IsAcceptButtonEnabled() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsForgotGPMPinButtonVisible() const override;
  bool IsGPMPinOptionsButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;
  void OnGPMPinOptionChosen(bool is_arbitrary) const override;

  std::u16string pin_;
  const int pin_digits_count_;
  const Mode mode_;
  const AuthenticatorRequestDialogController::GpmPinError error_;
};

// The sheet shown when the user is entering an arbitrary (alphanumeric) pin.
class AuthenticatorGPMArbitraryPinSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  // Indicates whether the view should accommodate creating a new pin or
  // entering an existing one.
  enum class Mode { kPinCreate, kPinEntry };

  explicit AuthenticatorGPMArbitraryPinSheetModel(
      AuthenticatorRequestDialogController* dialog_model,
      Mode mode,
      AuthenticatorRequestDialogController::GpmPinError error);
  ~AuthenticatorGPMArbitraryPinSheetModel() override;

  // Sets currently typed pin in the sheet.
  void SetPin(std::u16string pin);

  Mode mode() { return mode_; }

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::u16string GetError() const override;
  bool IsAcceptButtonEnabled() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsForgotGPMPinButtonVisible() const override;
  bool IsGPMPinOptionsButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;
  void OnGPMPinOptionChosen(bool is_arbitrary) const override;

  std::u16string pin_;
  const Mode mode_;
  const AuthenticatorRequestDialogController::GpmPinError error_;
};

// The sheet shown for bootstrapping Google Password Manager passkeys.
class AuthenticatorTrustThisComputerSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorTrustThisComputerSheetModel(
      AuthenticatorRequestDialogController* dialog_model);

  ~AuthenticatorTrustThisComputerSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsCancelButtonVisible() const override;
  std::u16string GetCancelButtonLabel() const override;
  void OnCancel() override;
  bool IsAcceptButtonEnabled() const override;
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_
