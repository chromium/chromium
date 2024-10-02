// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_
#define CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "device/fido/pin.h"

namespace gfx {
class Image;
}  // namespace gfx

// Base class for sheets, implementing the shared behavior used on most sheets,
// as well as maintaining a weak pointer to the dialog model.
class AuthenticatorSheetModelBase
    : public AuthenticatorRequestSheetModel,
      public AuthenticatorRequestDialogModel::Observer {
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
      AuthenticatorRequestDialogModel* dialog_model);
  AuthenticatorSheetModelBase(
      AuthenticatorRequestDialogModel* dialog_model,
      OtherMechanismButtonVisibility other_mechanism_button_visibility);

  AuthenticatorSheetModelBase(const AuthenticatorSheetModelBase&) = delete;
  AuthenticatorSheetModelBase& operator=(const AuthenticatorSheetModelBase&) =
      delete;

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

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override;

 private:
  raw_ptr<AuthenticatorRequestDialogModel> dialog_model_;
  OtherMechanismButtonVisibility other_mechanism_button_visibility_ =
      OtherMechanismButtonVisibility::kHidden;
};

// The sheet shown for selecting the transport over which the security key
// should be accessed.
class AuthenticatorMechanismSelectorSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorMechanismSelectorSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);

  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorInsertAndActivateUsbSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorInsertAndActivateUsbSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);

  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::vector<std::u16string> GetAdditionalDescriptions() const override;
};

class AuthenticatorTimeoutErrorModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorTimeoutErrorModel(
      AuthenticatorRequestDialogModel* dialog_model);

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
      AuthenticatorRequestDialogModel* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetCancelButtonLabel() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorNoPasskeysErrorModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorNoPasskeysErrorModel(
      AuthenticatorRequestDialogModel* dialog_model);

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
      AuthenticatorRequestDialogModel* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  void OnAccept() override;
};

class AuthenticatorAlreadyRegisteredErrorModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorAlreadyRegisteredErrorModel(
      AuthenticatorRequestDialogModel* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  void OnAccept() override;
};

class AuthenticatorInternalUnrecognizedErrorSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorInternalUnrecognizedErrorSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  void OnAccept() override;
};

class AuthenticatorBlePowerOnManualSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorBlePowerOnManualSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
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
  explicit AuthenticatorBlePowerOnAutomaticSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);

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
      AuthenticatorRequestDialogModel* dialog_model);

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsCancelButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;
};

class AuthenticatorTouchIdSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorTouchIdSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);

  // Called after the user taps their Touch ID sensor.
  void OnTouchIDSensorTapped(std::optional<crypto::ScopedLAContext> lacontext);

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsCancelButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;

  bool touch_id_completed_ = false;
};

#endif  // IS_MAC

class AuthenticatorOffTheRecordInterstitialSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorOffTheRecordInterstitialSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);

  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetCancelButtonLabel() const override;
  void OnAccept() override;
};

class AuthenticatorPaaskSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorPaaskSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorPaaskSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsManageDevicesButtonVisible() const override;
  void OnManageDevices() override;
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

  void SetPinCode(std::u16string pin_code);
  void SetPinConfirmation(std::u16string pin_confirmation);

  Mode mode() const { return mode_; }

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::u16string GetError() const override;
  bool IsAcceptButtonVisible() const override;
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
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::vector<std::u16string> GetAdditionalDescriptions() const override;
};

class AuthenticatorBioEnrollmentSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorBioEnrollmentSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorBioEnrollmentSheetModel() override;

  int max_bio_samples() { return dialog_model()->max_bio_samples.value_or(1); }
  int bio_samples_remaining() {
    return dialog_model()->bio_samples_remaining.value_or(1);
  }

 private:
  // AuthenticatorSheetModelBase:
  bool IsActivityIndicatorVisible() const override;
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
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
  static std::unique_ptr<AuthenticatorGenericErrorSheetModel>
  ForWindowsHelloNotEnabled(AuthenticatorRequestDialogModel* dialog_model);

 private:
  AuthenticatorGenericErrorSheetModel(
      AuthenticatorRequestDialogModel* dialog_model,
      std::u16string title,
      std::u16string description);

  // AuthenticatorSheetModelBase:
  bool IsAcceptButtonVisible() const override;
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
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorResidentCredentialConfirmationSheetView() override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsAcceptButtonVisible() const override;
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
      AuthenticatorRequestDialogModel* dialog_model,
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
  std::u16string GetAcceptButtonLabel() const override;

  const UserVerificationMode user_verification_mode_;
  const SelectionType selection_type_;
  size_t selected_ = 0;
};

class AuthenticatorQRSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorQRSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorQRSheetModel() override;

  // Returns the labels that indicate to the user they can insert and activate
  // a hardware security key. If empty then a security key cannot be used.
  std::vector<std::u16string> GetSecurityKeyLabels() const;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorConnectingSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorConnectingSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorConnectingSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorConnectedSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorConnectedSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
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
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorCableErrorSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::u16string GetCancelButtonLabel() const override;
};

class AuthenticatorCreatePasskeySheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorCreatePasskeySheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorCreatePasskeySheetModel() override;

  // An additional label that `AuthenticatorCreatePasskeySheetView` includes in
  // the `BuildStepSpecificContent()` view.
  std::u16string passkey_storage_description() const;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  void OnAccept() override;
  std::u16string GetAcceptButtonLabel() const override;
};

class AuthenticatorGPMErrorSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorGPMErrorSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorGPMErrorSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

class AuthenticatorGPMConnectingSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorGPMConnectingSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorGPMConnectingSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
};

// A confirmation screen that can be shown instead of the mechanism selection
// screen when we are confident a request can be resolved using an already
// paired phone.
class AuthenticatorPhoneConfirmationSheet : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorPhoneConfirmationSheet(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorPhoneConfirmationSheet() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  void OnAccept() override;
  std::u16string GetAcceptButtonLabel() const override;
};

// An account and mechanism picker that combines passkeys from multiple sources.
// Passkeys are grouped in two lists:
// * "Primary" passkeys. These are local passkeys if available, or GPM passkeys
//   if no local passkeys are available. Can be empty.
// * "Secondary" passkeys. These are all the other passkeys & mechanisms.
// AuthenticatorMultiSourcePickerSheetModel will filter these lists and
// present them as indices of AuthenticatorRequestDialogModel's
// `mechanisms()` member.
class AuthenticatorMultiSourcePickerSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorMultiSourcePickerSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorMultiSourcePickerSheetModel() override;

  // Returns a vector of indices to the "Primary" passkey mechanisms. Indices
  // correspond to AuthenticatorRequestDialogModel's mechanisms().
  std::vector<int>& primary_passkey_indices() {
    return primary_passkey_indices_;
  }

  // Returns a vector of indices to the "Secondary" passkey mechanisms. Indices
  // correspond to AuthenticatorRequestDialogModel's mechanisms().
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
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorPriorityMechanismSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;
};

class AuthenticatorGpmPinSheetModelBase : public AuthenticatorSheetModelBase {
 public:
  // Indicates whether the view should accommodate creating a new pin or
  // entering an existing one.
  enum class Mode { kPinCreate, kPinEntry };

  explicit AuthenticatorGpmPinSheetModelBase(
      AuthenticatorRequestDialogModel* dialog_model,
      Mode mode);
  ~AuthenticatorGpmPinSheetModelBase() override;

  std::u16string GetGpmAccountEmail() const;
  std::u16string GetGpmAccountName() const;
  gfx::Image GetGpmAccountImage() const;
  std::u16string GetAccessibleDescription() const;

  std::u16string pin() const { return pin_; }
  Mode mode() const { return mode_; }
  bool ui_disabled() const;

  // Sets currently typed pin in the sheet.
  virtual void SetPin(std::u16string pin) = 0;

  // Returns the accessibility label of the pin view based on its type and mode.
  virtual std::u16string GetAccessibleName() const = 0;

 protected:
  std::u16string pin_;
  const Mode mode_;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  std::u16string GetError() const override;
  bool IsForgotGPMPinButtonVisible() const override;
  bool IsGPMPinOptionsButtonVisible() const override;
  void OnAccept() override;
  void OnForgotGPMPin() const override;
  void OnGPMPinOptionChosen(bool is_arbitrary) const override;
};

// The sheet shown when the user is entering a digit-only GPM pin.
class AuthenticatorGpmPinSheetModel : public AuthenticatorGpmPinSheetModelBase {
 public:
  explicit AuthenticatorGpmPinSheetModel(
      AuthenticatorRequestDialogModel* dialog_model,
      int pin_digits_count,
      Mode mode);
  ~AuthenticatorGpmPinSheetModel() override;

  void PinCharTyped(bool is_digit);

  int pin_digits_count() const;

  // AuthenticatorGpmPinSheetModelBase:
  void SetPin(std::u16string pin) override;
  std::u16string GetAccessibleName() const override;

 private:
  bool FullPinTyped() const;

  // AuthenticatorSheetModelBase:
  bool IsAcceptButtonEnabled() const override;
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetHint() const override;

  const int pin_digits_count_;

  // Whether a hint explaining that only digits are allowed should be shown.
  bool show_digit_hint_ = false;
};

// The sheet shown when the user is entering an arbitrary (alphanumeric) pin.
class AuthenticatorGpmArbitraryPinSheetModel
    : public AuthenticatorGpmPinSheetModelBase {
 public:
  explicit AuthenticatorGpmArbitraryPinSheetModel(
      AuthenticatorRequestDialogModel* dialog_model,
      Mode mode);
  ~AuthenticatorGpmArbitraryPinSheetModel() override;

  // AuthenticatorGpmPinSheetModelBase:
  void SetPin(std::u16string pin) override;
  std::u16string GetAccessibleName() const override;

 private:
  // AuthenticatorSheetModelBase:
  bool IsAcceptButtonEnabled() const override;
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  std::u16string GetHint() const override;
};

// The sheet shown for bootstrapping Google Password Manager passkeys during
// sign-in.
class AuthenticatorTrustThisComputerAssertionSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorTrustThisComputerAssertionSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);

  ~AuthenticatorTrustThisComputerAssertionSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsCancelButtonVisible() const override;
  std::u16string GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  bool IsOtherMechanismButtonVisible() const override;
  std::u16string GetOtherMechanismButtonLabel() const override;
  void OnBack() override;
  void OnAccept() override;
};

// The sheet shown for creating a passkey in Google Password Manager.
class AuthenticatorCreateGpmPasskeySheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorCreateGpmPasskeySheetModel(
      AuthenticatorRequestDialogModel* dialog_model);

  ~AuthenticatorCreateGpmPasskeySheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsCancelButtonVisible() const override;
  std::u16string GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;
};

// The sheet shown for warning a user about creating a Google Password Manager
// passkey in incognito mode.
class AuthenticatorGpmIncognitoCreateSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorGpmIncognitoCreateSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);

  ~AuthenticatorGpmIncognitoCreateSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsCancelButtonVisible() const override;
  std::u16string GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;
};

// The sheet shown for bootstrapping Google Password Manager passkeys during
// passkey creation.
class AuthenticatorTrustThisComputerCreationSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorTrustThisComputerCreationSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);

  ~AuthenticatorTrustThisComputerCreationSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsCancelButtonVisible() const override;
  std::u16string GetCancelButtonLabel() const override;
  std::u16string GetOtherMechanismButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;
};

// The sheet shown when the maximum amount of GPM pin entry attempts has been
// reached informing that the PIN needs to be reset.
class AuthenticatorGPMLockedPinSheetModel : public AuthenticatorSheetModelBase {
 public:
  explicit AuthenticatorGPMLockedPinSheetModel(
      AuthenticatorRequestDialogModel* dialog_model);

  ~AuthenticatorGPMLockedPinSheetModel() override;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override;
  std::u16string GetStepDescription() const override;
  bool IsAcceptButtonEnabled() const override;
  bool IsAcceptButtonVisible() const override;
  std::u16string GetAcceptButtonLabel() const override;
  void OnAccept() override;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_
