// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/sheet_models.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/webauthn/webauthn_ui_helpers.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_types.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

// Possibly returns a resident key warning if the model indicates that it's
// needed.
std::u16string PossibleResidentKeyWarning(
    AuthenticatorRequestDialogModel* dialog_model) {
  switch (dialog_model->resident_key_requirement()) {
    case device::ResidentKeyRequirement::kDiscouraged:
      return std::u16string();
    case device::ResidentKeyRequirement::kPreferred:
      return l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_RESIDENT_KEY_PREFERRED_PRIVACY);
    case device::ResidentKeyRequirement::kRequired:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_RESIDENT_KEY_PRIVACY);
  }
  NOTREACHED();
  return std::u16string();
}

}  // namespace

// AuthenticatorSheetModelBase ------------------------------------------------

AuthenticatorSheetModelBase::AuthenticatorSheetModelBase(
    AuthenticatorRequestDialogModel* dialog_model)
    : dialog_model_(dialog_model) {
  DCHECK(dialog_model);
  dialog_model_->AddObserver(this);
}

AuthenticatorSheetModelBase::AuthenticatorSheetModelBase(
    AuthenticatorRequestDialogModel* dialog_model,
    OtherMechanismButtonVisibility other_mechanism_button_visibility)
    : AuthenticatorSheetModelBase(dialog_model) {
  other_mechanism_button_visibility_ = other_mechanism_button_visibility;
}

AuthenticatorSheetModelBase::~AuthenticatorSheetModelBase() {
  if (dialog_model_) {
    dialog_model_->RemoveObserver(this);
    dialog_model_ = nullptr;
  }
}

// static
std::u16string AuthenticatorSheetModelBase::GetRelyingPartyIdString(
    const AuthenticatorRequestDialogModel* dialog_model) {
  // The preferred width of medium snap point modal dialog view is 448 dp, but
  // we leave some room for padding between the text and the modal views.
  static constexpr int kDialogWidth = 300;
  return webauthn_ui_helpers::RpIdToElidedHost(dialog_model->relying_party_id(),
                                               kDialogWidth);
}

bool AuthenticatorSheetModelBase::IsActivityIndicatorVisible() const {
  return false;
}

bool AuthenticatorSheetModelBase::IsBackButtonVisible() const {
  return true;
}

bool AuthenticatorSheetModelBase::IsCancelButtonVisible() const {
  return true;
}

bool AuthenticatorSheetModelBase::IsOtherMechanismButtonVisible() const {
  return other_mechanism_button_visibility_ ==
             OtherMechanismButtonVisibility::kVisible &&
         dialog_model_ && dialog_model_->mechanisms().size() > 1;
}

std::u16string AuthenticatorSheetModelBase::GetOtherMechanismButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_USE_A_DIFFERENT_DEVICE);
}

std::u16string AuthenticatorSheetModelBase::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool AuthenticatorSheetModelBase::IsAcceptButtonVisible() const {
  return false;
}

bool AuthenticatorSheetModelBase::IsAcceptButtonEnabled() const {
  return false;
}

std::u16string AuthenticatorSheetModelBase::GetAcceptButtonLabel() const {
  return std::u16string();
}

void AuthenticatorSheetModelBase::OnBack() {
  if (dialog_model()) {
    dialog_model()->StartOver();
  }
}

void AuthenticatorSheetModelBase::OnAccept() {
  NOTREACHED();
}

void AuthenticatorSheetModelBase::OnCancel() {
  if (dialog_model()) {
    dialog_model()->Cancel();
  }
}

void AuthenticatorSheetModelBase::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  DCHECK(model == dialog_model_);
  dialog_model_ = nullptr;
}

// AuthenticatorMechanismSelectorSheetModel -----------------------------------

AuthenticatorMechanismSelectorSheetModel::
    AuthenticatorMechanismSelectorSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  vector_illustrations_.emplace(kPasskeyHeaderIcon, kPasskeyHeaderDarkIcon);
}

bool AuthenticatorMechanismSelectorSheetModel::IsBackButtonVisible() const {
  return false;
}

std::u16string AuthenticatorMechanismSelectorSheetModel::GetStepTitle() const {
  switch (dialog_model()->transport_availability()->request_type) {
    case device::FidoRequestType::kMakeCredential:
      return l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_CREATE_PASSKEY_CHOOSE_DEVICE_TITLE);
    case device::FidoRequestType::kGetAssertion:
      return l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_USE_PASSKEY_CHOOSE_DEVICE_TITLE);
  }
}

std::u16string AuthenticatorMechanismSelectorSheetModel::GetStepDescription()
    const {
  switch (dialog_model()->transport_availability()->request_type) {
    case device::FidoRequestType::kMakeCredential:
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_CREATE_PASSKEY_CHOOSE_DEVICE_BODY,
          GetRelyingPartyIdString(dialog_model()));
    case device::FidoRequestType::kGetAssertion:
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_USE_PASSKEY_CHOOSE_DEVICE_BODY,
          GetRelyingPartyIdString(dialog_model()));
  }
}

bool AuthenticatorMechanismSelectorSheetModel::IsManageDevicesButtonVisible()
    const {
  // If any phones are shown then also show a button that goes to the settings
  // page to manage them.
  return base::ranges::any_of(
      dialog_model()->mechanisms(),
      [](const AuthenticatorRequestDialogModel::Mechanism& mechanism) {
        return absl::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Phone>(mechanism.type);
      });
}

void AuthenticatorMechanismSelectorSheetModel::OnManageDevices() {
  if (dialog_model()) {
    dialog_model()->ManageDevices();
  }
}

// AuthenticatorInsertAndActivateUsbSheetModel ----------------------

AuthenticatorInsertAndActivateUsbSheetModel::
    AuthenticatorInsertAndActivateUsbSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {
  vector_illustrations_.emplace(kPasskeyUsbIcon, kPasskeyUsbDarkIcon);
}

bool AuthenticatorInsertAndActivateUsbSheetModel::IsActivityIndicatorVisible()
    const {
  return true;
}

std::u16string AuthenticatorInsertAndActivateUsbSheetModel::GetStepTitle()
    const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_GENERIC_TITLE,
                                    GetRelyingPartyIdString(dialog_model()));
}

std::u16string AuthenticatorInsertAndActivateUsbSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_USB_ACTIVATE_DESCRIPTION);
}

std::u16string
AuthenticatorInsertAndActivateUsbSheetModel::GetAdditionalDescription() const {
  return PossibleResidentKeyWarning(dialog_model());
}

// AuthenticatorTimeoutErrorModel ---------------------------------------------

AuthenticatorTimeoutErrorModel::AuthenticatorTimeoutErrorModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  vector_illustrations_.emplace(kPasskeyErrorIcon, kPasskeyErrorDarkIcon);
}

bool AuthenticatorTimeoutErrorModel::IsBackButtonVisible() const {
  return false;
}

std::u16string AuthenticatorTimeoutErrorModel::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

std::u16string AuthenticatorTimeoutErrorModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_GENERIC_TITLE);
}

std::u16string AuthenticatorTimeoutErrorModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_TIMEOUT_DESCRIPTION);
}

// AuthenticatorNoAvailableTransportsErrorModel -------------------------------

AuthenticatorNoAvailableTransportsErrorModel::
    AuthenticatorNoAvailableTransportsErrorModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  vector_illustrations_.emplace(kPasskeyErrorIcon, kPasskeyErrorDarkIcon);
}

bool AuthenticatorNoAvailableTransportsErrorModel::IsBackButtonVisible() const {
  return false;
}

std::u16string
AuthenticatorNoAvailableTransportsErrorModel::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

std::u16string AuthenticatorNoAvailableTransportsErrorModel::GetStepTitle()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_MISSING_CAPABILITY_TITLE);
}

std::u16string
AuthenticatorNoAvailableTransportsErrorModel::GetStepDescription() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_ERROR_MISSING_CAPABILITY_DESC,
                                    GetRelyingPartyIdString(dialog_model()));
}

// AuthenticatorNoPasskeysErrorModel ------------------------------------------

AuthenticatorNoPasskeysErrorModel::AuthenticatorNoPasskeysErrorModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  vector_illustrations_.emplace(kPasskeyErrorIcon, kPasskeyErrorDarkIcon);
}

bool AuthenticatorNoPasskeysErrorModel::IsBackButtonVisible() const {
  return false;
}

std::u16string AuthenticatorNoPasskeysErrorModel::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}
std::u16string AuthenticatorNoPasskeysErrorModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_NO_PASSKEYS_TITLE);
}

std::u16string AuthenticatorNoPasskeysErrorModel::GetStepDescription() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_ERROR_NO_PASSKEYS_DESCRIPTION,
                                    GetRelyingPartyIdString(dialog_model()));
}

// AuthenticatorNotRegisteredErrorModel ---------------------------------------

AuthenticatorNotRegisteredErrorModel::AuthenticatorNotRegisteredErrorModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  vector_illustrations_.emplace(kPasskeyErrorIcon, kPasskeyErrorDarkIcon);
}

bool AuthenticatorNotRegisteredErrorModel::IsBackButtonVisible() const {
  return false;
}

std::u16string AuthenticatorNotRegisteredErrorModel::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

bool AuthenticatorNotRegisteredErrorModel::IsAcceptButtonVisible() const {
  return dialog_model()->offer_try_again_in_ui();
}

bool AuthenticatorNotRegisteredErrorModel::IsAcceptButtonEnabled() const {
  return true;
}

std::u16string AuthenticatorNotRegisteredErrorModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_RETRY);
}

std::u16string AuthenticatorNotRegisteredErrorModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_WRONG_KEY_TITLE);
}

std::u16string AuthenticatorNotRegisteredErrorModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_ERROR_WRONG_KEY_SIGN_DESCRIPTION);
}

void AuthenticatorNotRegisteredErrorModel::OnAccept() {
  dialog_model()->StartOver();
}

// AuthenticatorAlreadyRegisteredErrorModel -----------------------------------

AuthenticatorAlreadyRegisteredErrorModel::
    AuthenticatorAlreadyRegisteredErrorModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  vector_illustrations_.emplace(kPasskeyErrorIcon, kPasskeyErrorDarkIcon);
}

bool AuthenticatorAlreadyRegisteredErrorModel::IsBackButtonVisible() const {
  return false;
}

std::u16string AuthenticatorAlreadyRegisteredErrorModel::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

bool AuthenticatorAlreadyRegisteredErrorModel::IsAcceptButtonVisible() const {
  return dialog_model()->offer_try_again_in_ui();
}

bool AuthenticatorAlreadyRegisteredErrorModel::IsAcceptButtonEnabled() const {
  return true;
}

std::u16string AuthenticatorAlreadyRegisteredErrorModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_RETRY);
}

std::u16string AuthenticatorAlreadyRegisteredErrorModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_WRONG_DEVICE_TITLE);
}

std::u16string AuthenticatorAlreadyRegisteredErrorModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_ERROR_WRONG_DEVICE_REGISTER_DESCRIPTION);
}

void AuthenticatorAlreadyRegisteredErrorModel::OnAccept() {
  dialog_model()->StartOver();
}

// AuthenticatorInternalUnrecognizedErrorSheetModel ---------------------------

AuthenticatorInternalUnrecognizedErrorSheetModel::
    AuthenticatorInternalUnrecognizedErrorSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  vector_illustrations_.emplace(kPasskeyErrorIcon, kPasskeyErrorDarkIcon);
}

bool AuthenticatorInternalUnrecognizedErrorSheetModel::IsBackButtonVisible()
    const {
  return dialog_model()->offer_try_again_in_ui();
}

bool AuthenticatorInternalUnrecognizedErrorSheetModel::IsAcceptButtonVisible()
    const {
  return dialog_model()->offer_try_again_in_ui();
}

bool AuthenticatorInternalUnrecognizedErrorSheetModel::IsAcceptButtonEnabled()
    const {
  return true;
}

std::u16string
AuthenticatorInternalUnrecognizedErrorSheetModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_RETRY);
}

std::u16string AuthenticatorInternalUnrecognizedErrorSheetModel::GetStepTitle()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_ERROR_INTERNAL_UNRECOGNIZED_TITLE);
}

std::u16string
AuthenticatorInternalUnrecognizedErrorSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_ERROR_INTERNAL_UNRECOGNIZED_DESCRIPTION);
}

void AuthenticatorInternalUnrecognizedErrorSheetModel::OnAccept() {
  dialog_model()->StartOver();
}

// AuthenticatorBlePowerOnManualSheetModel ------------------------------------

AuthenticatorBlePowerOnManualSheetModel::
    AuthenticatorBlePowerOnManualSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {
  vector_illustrations_.emplace(kPasskeyErrorBluetoothIcon,
                                kPasskeyErrorBluetoothDarkIcon);
}

std::u16string AuthenticatorBlePowerOnManualSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_BLUETOOTH_POWER_ON_MANUAL_TITLE);
}

std::u16string AuthenticatorBlePowerOnManualSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_BLUETOOTH_POWER_ON_MANUAL_DESCRIPTION);
}

bool AuthenticatorBlePowerOnManualSheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorBlePowerOnManualSheetModel::IsAcceptButtonEnabled() const {
  return dialog_model()->ble_adapter_is_powered();
}

std::u16string AuthenticatorBlePowerOnManualSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLUETOOTH_POWER_ON_MANUAL_NEXT);
}

void AuthenticatorBlePowerOnManualSheetModel::OnBluetoothPoweredStateChanged() {
  dialog_model()->OnSheetModelDidChange();
}

void AuthenticatorBlePowerOnManualSheetModel::OnAccept() {
  dialog_model()->ContinueWithFlowAfterBleAdapterPowered();
}

// AuthenticatorBlePowerOnAutomaticSheetModel
// ------------------------------------

AuthenticatorBlePowerOnAutomaticSheetModel::
    AuthenticatorBlePowerOnAutomaticSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {
  vector_illustrations_.emplace(kPasskeyErrorBluetoothIcon,
                                kPasskeyErrorBluetoothDarkIcon);
}

bool AuthenticatorBlePowerOnAutomaticSheetModel::IsActivityIndicatorVisible()
    const {
  return busy_powering_on_ble_;
}

std::u16string AuthenticatorBlePowerOnAutomaticSheetModel::GetStepTitle()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLUETOOTH_POWER_ON_AUTO_TITLE);
}

std::u16string AuthenticatorBlePowerOnAutomaticSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_BLUETOOTH_POWER_ON_AUTO_DESCRIPTION);
}

bool AuthenticatorBlePowerOnAutomaticSheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorBlePowerOnAutomaticSheetModel::IsAcceptButtonEnabled() const {
  return !busy_powering_on_ble_;
}

std::u16string
AuthenticatorBlePowerOnAutomaticSheetModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLUETOOTH_POWER_ON_AUTO_NEXT);
}

void AuthenticatorBlePowerOnAutomaticSheetModel::OnAccept() {
  busy_powering_on_ble_ = true;
  dialog_model()->OnSheetModelDidChange();
  dialog_model()->PowerOnBleAdapter();
}

#if BUILDFLAG(IS_MAC)

// AuthenticatorBlePermissionMacSheetModel
// ------------------------------------

AuthenticatorBlePermissionMacSheetModel::
    AuthenticatorBlePermissionMacSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {
  vector_illustrations_.emplace(kPasskeyErrorBluetoothIcon,
                                kPasskeyErrorBluetoothDarkIcon);
}

bool AuthenticatorBlePermissionMacSheetModel::ShouldFocusBackArrow() const {
  return true;
}

std::u16string AuthenticatorBlePermissionMacSheetModel::GetStepTitle() const {
  // An empty title causes the title View to be omitted.
  return u"";
}

std::u16string AuthenticatorBlePermissionMacSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLUETOOTH_PERMISSION);
}

bool AuthenticatorBlePermissionMacSheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorBlePermissionMacSheetModel::IsAcceptButtonEnabled() const {
  return true;
}

bool AuthenticatorBlePermissionMacSheetModel::IsCancelButtonVisible() const {
  return true;
}

std::u16string AuthenticatorBlePermissionMacSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_OPEN_SETTINGS_LINK);
}

void AuthenticatorBlePermissionMacSheetModel::OnAccept() {
  dialog_model()->OpenBlePreferences();
}

#endif  // IS_MAC

// AuthenticatorOffTheRecordInterstitialSheetModel
// -----------------------------------------

AuthenticatorOffTheRecordInterstitialSheetModel::
    AuthenticatorOffTheRecordInterstitialSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  // TODO(1358719): Add more specific illustration once available. The
  // "error" graphic is a large question mark, so it looks visually very
  // similar.
  vector_illustrations_.emplace(kPasskeyErrorIcon, kPasskeyErrorDarkIcon);
}

std::u16string AuthenticatorOffTheRecordInterstitialSheetModel::GetStepTitle()
    const {
  return l10n_util::GetStringFUTF16(
      IDS_WEBAUTHN_PLATFORM_AUTHENTICATOR_OFF_THE_RECORD_INTERSTITIAL_TITLE,
      GetRelyingPartyIdString(dialog_model()));
}

std::u16string
AuthenticatorOffTheRecordInterstitialSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_PLATFORM_AUTHENTICATOR_OFF_THE_RECORD_INTERSTITIAL_DESCRIPTION);
}

bool AuthenticatorOffTheRecordInterstitialSheetModel::IsAcceptButtonVisible()
    const {
  return true;
}

bool AuthenticatorOffTheRecordInterstitialSheetModel::IsAcceptButtonEnabled()
    const {
  return true;
}

std::u16string
AuthenticatorOffTheRecordInterstitialSheetModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CONTINUE);
}

void AuthenticatorOffTheRecordInterstitialSheetModel::OnAccept() {
  dialog_model()->OnOffTheRecordInterstitialAccepted();
}

std::u16string
AuthenticatorOffTheRecordInterstitialSheetModel::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_PLATFORM_AUTHENTICATOR_OFF_THE_RECORD_INTERSTITIAL_DENY);
}

// AuthenticatorPaaskSheetModel -----------------------------------------

AuthenticatorPaaskSheetModel::AuthenticatorPaaskSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {
  vector_illustrations_.emplace(kPasskeyPhoneIcon, kPasskeyPhoneDarkIcon);
}

AuthenticatorPaaskSheetModel::~AuthenticatorPaaskSheetModel() = default;

bool AuthenticatorPaaskSheetModel::IsActivityIndicatorVisible() const {
  return true;
}

std::u16string AuthenticatorPaaskSheetModel::GetStepTitle() const {
  switch (dialog_model()->cable_ui_type()) {
    case AuthenticatorRequestDialogModel::CableUIType::CABLE_V1:
    case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_SERVER_LINK:
      // caBLEv1 and v2 server-link don't include device names.
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLE_ACTIVATE_TITLE);
    case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_2ND_FACTOR:
      return l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_CABLE_ACTIVATE_TITLE_DEVICE);
  }
}

std::u16string AuthenticatorPaaskSheetModel::GetStepDescription() const {
  switch (dialog_model()->cable_ui_type()) {
    case AuthenticatorRequestDialogModel::CableUIType::CABLE_V1:
    case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_SERVER_LINK:
      // caBLEv1 and v2 server-link don't include device names.
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLE_ACTIVATE_DESCRIPTION);
    case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_2ND_FACTOR: {
      DCHECK(dialog_model()->selected_phone_name());
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_CABLE_ACTIVATE_DEVICE_NAME_DESCRIPTION,
          base::UTF8ToUTF16(
              dialog_model()->selected_phone_name().value_or("")));
    }
  }
}

// AuthenticatorAndroidAccessorySheetModel ------------------------------------

AuthenticatorAndroidAccessorySheetModel::
    AuthenticatorAndroidAccessorySheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {
  vector_illustrations_.emplace(kPasskeyAoaIcon, kPasskeyAoaDarkIcon);
}

AuthenticatorAndroidAccessorySheetModel::
    ~AuthenticatorAndroidAccessorySheetModel() = default;

bool AuthenticatorAndroidAccessorySheetModel::IsBackButtonVisible() const {
  return true;
}

bool AuthenticatorAndroidAccessorySheetModel::IsActivityIndicatorVisible()
    const {
  return true;
}

std::u16string AuthenticatorAndroidAccessorySheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLEV2_AOA_TITLE);
}

std::u16string AuthenticatorAndroidAccessorySheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLEV2_AOA_DESCRIPTION);
}

// AuthenticatorClientPinEntrySheetModel
// -----------------------------------------

AuthenticatorClientPinEntrySheetModel::AuthenticatorClientPinEntrySheetModel(
    AuthenticatorRequestDialogModel* dialog_model,
    Mode mode,
    device::pin::PINEntryError error)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible),
      mode_(mode) {
  vector_illustrations_.emplace(kPasskeyUsbIcon, kPasskeyUsbDarkIcon);
  switch (error) {
    case device::pin::PINEntryError::kNoError:
      break;
    case device::pin::PINEntryError::kInternalUvLocked:
      error_ = l10n_util::GetStringUTF16(IDS_WEBAUTHN_UV_ERROR_LOCKED);
      break;
    case device::pin::PINEntryError::kInvalidCharacters:
      error_ = l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_PIN_ENTRY_ERROR_INVALID_CHARACTERS);
      break;
    case device::pin::PINEntryError::kSameAsCurrentPIN:
      error_ = l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_PIN_ENTRY_ERROR_SAME_AS_CURRENT);
      break;
    case device::pin::PINEntryError::kTooShort:
      error_ = l10n_util::GetPluralStringFUTF16(
          IDS_WEBAUTHN_PIN_ENTRY_ERROR_TOO_SHORT,
          dialog_model->min_pin_length());
      break;
    case device::pin::PINEntryError::kWrongPIN:
      absl::optional<int> attempts = dialog_model->pin_attempts();
      error_ =
          attempts && *attempts <= 3
              ? l10n_util::GetPluralStringFUTF16(
                    IDS_WEBAUTHN_PIN_ENTRY_ERROR_FAILED_RETRIES, *attempts)
              : l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_ERROR_FAILED);
      break;
  }
}

AuthenticatorClientPinEntrySheetModel::
    ~AuthenticatorClientPinEntrySheetModel() = default;

void AuthenticatorClientPinEntrySheetModel::SetPinCode(
    std::u16string pin_code) {
  pin_code_ = std::move(pin_code);
}

void AuthenticatorClientPinEntrySheetModel::SetPinConfirmation(
    std::u16string pin_confirmation) {
  DCHECK(mode_ == Mode::kPinSetup || mode_ == Mode::kPinChange);
  pin_confirmation_ = std::move(pin_confirmation);
}

std::u16string AuthenticatorClientPinEntrySheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_TITLE);
}

std::u16string AuthenticatorClientPinEntrySheetModel::GetStepDescription()
    const {
  switch (mode_) {
    case Mode::kPinChange:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_FORCE_PIN_CHANGE);
    case Mode::kPinEntry:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_DESCRIPTION);
    case Mode::kPinSetup:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_SETUP_DESCRIPTION);
  }
}

std::u16string AuthenticatorClientPinEntrySheetModel::GetError() const {
  return error_;
}

bool AuthenticatorClientPinEntrySheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorClientPinEntrySheetModel::IsAcceptButtonEnabled() const {
  return true;
}

std::u16string AuthenticatorClientPinEntrySheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_NEXT);
}

void AuthenticatorClientPinEntrySheetModel::OnAccept() {
  if ((mode_ == Mode::kPinChange || mode_ == Mode::kPinSetup) &&
      pin_code_ != pin_confirmation_) {
    error_ = l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_ERROR_MISMATCH);
    dialog_model()->OnSheetModelDidChange();
    return;
  }

  if (dialog_model()) {
    dialog_model()->OnHavePIN(pin_code_);
  }
}

// AuthenticatorClientPinTapAgainSheetModel ----------------------

AuthenticatorClientPinTapAgainSheetModel::
    AuthenticatorClientPinTapAgainSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  vector_illustrations_.emplace(kPasskeyUsbIcon, kPasskeyUsbDarkIcon);
}

AuthenticatorClientPinTapAgainSheetModel::
    ~AuthenticatorClientPinTapAgainSheetModel() = default;

bool AuthenticatorClientPinTapAgainSheetModel::IsActivityIndicatorVisible()
    const {
  return true;
}

std::u16string AuthenticatorClientPinTapAgainSheetModel::GetStepTitle() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_GENERIC_TITLE,
                                    GetRelyingPartyIdString(dialog_model()));
}

std::u16string AuthenticatorClientPinTapAgainSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_TAP_AGAIN_DESCRIPTION);
}

std::u16string
AuthenticatorClientPinTapAgainSheetModel::GetAdditionalDescription() const {
  return PossibleResidentKeyWarning(dialog_model());
}

// AuthenticatorBioEnrollmentSheetModel ----------------------------------

// No illustration since the content already has a large animated
// fingerprint icon.
AuthenticatorBioEnrollmentSheetModel::AuthenticatorBioEnrollmentSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {}

AuthenticatorBioEnrollmentSheetModel::~AuthenticatorBioEnrollmentSheetModel() =
    default;

bool AuthenticatorBioEnrollmentSheetModel::IsActivityIndicatorVisible() const {
  return !IsAcceptButtonVisible();
}

std::u16string AuthenticatorBioEnrollmentSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_ADD_TITLE);
}

std::u16string AuthenticatorBioEnrollmentSheetModel::GetStepDescription()
    const {
  return IsAcceptButtonVisible()
             ? l10n_util::GetStringUTF16(
                   IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_ENROLLING_COMPLETE_LABEL)
             : l10n_util::GetStringUTF16(
                   IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_ENROLLING_LABEL);
}

bool AuthenticatorBioEnrollmentSheetModel::IsAcceptButtonEnabled() const {
  return true;
}

bool AuthenticatorBioEnrollmentSheetModel::IsAcceptButtonVisible() const {
  return dialog_model()->bio_samples_remaining() &&
         dialog_model()->bio_samples_remaining() <= 0;
}

std::u16string AuthenticatorBioEnrollmentSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_NEXT);
}

bool AuthenticatorBioEnrollmentSheetModel::IsCancelButtonVisible() const {
  return !IsAcceptButtonVisible();
}

std::u16string AuthenticatorBioEnrollmentSheetModel::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_INLINE_ENROLLMENT_CANCEL_LABEL);
}

void AuthenticatorBioEnrollmentSheetModel::OnAccept() {
  dialog_model()->OnBioEnrollmentDone();
}

void AuthenticatorBioEnrollmentSheetModel::OnCancel() {
  OnAccept();
}

// AuthenticatorRetryUvSheetModel -------------------------------------

AuthenticatorRetryUvSheetModel::AuthenticatorRetryUvSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {
  vector_illustrations_.emplace(kPasskeyFingerprintIcon,
                                kPasskeyFingerprintDarkIcon);
}

AuthenticatorRetryUvSheetModel::~AuthenticatorRetryUvSheetModel() = default;

bool AuthenticatorRetryUvSheetModel::IsActivityIndicatorVisible() const {
  return true;
}

std::u16string AuthenticatorRetryUvSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_UV_RETRY_TITLE);
}

std::u16string AuthenticatorRetryUvSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_UV_RETRY_DESCRIPTION);
}

std::u16string AuthenticatorRetryUvSheetModel::GetError() const {
  int attempts = *dialog_model()->uv_attempts();
  if (attempts > 3) {
    return std::u16string();
  }
  return l10n_util::GetPluralStringFUTF16(
      IDS_WEBAUTHN_UV_RETRY_ERROR_FAILED_RETRIES, attempts);
}

// AuthenticatorGenericErrorSheetModel -----------------------------------

// static
std::unique_ptr<AuthenticatorGenericErrorSheetModel>
AuthenticatorGenericErrorSheetModel::ForClientPinErrorSoftBlock(
    AuthenticatorRequestDialogModel* dialog_model) {
  return base::WrapUnique(new AuthenticatorGenericErrorSheetModel(
      dialog_model, l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_GENERIC_TITLE),
      l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_CLIENT_PIN_SOFT_BLOCK_DESCRIPTION)));
}

// static
std::unique_ptr<AuthenticatorGenericErrorSheetModel>
AuthenticatorGenericErrorSheetModel::ForClientPinErrorHardBlock(
    AuthenticatorRequestDialogModel* dialog_model) {
  return base::WrapUnique(new AuthenticatorGenericErrorSheetModel(
      dialog_model, l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_GENERIC_TITLE),
      l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_CLIENT_PIN_HARD_BLOCK_DESCRIPTION)));
}

// static
std::unique_ptr<AuthenticatorGenericErrorSheetModel>
AuthenticatorGenericErrorSheetModel::ForClientPinErrorAuthenticatorRemoved(
    AuthenticatorRequestDialogModel* dialog_model) {
  return base::WrapUnique(new AuthenticatorGenericErrorSheetModel(
      dialog_model, l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_GENERIC_TITLE),
      l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_CLIENT_PIN_AUTHENTICATOR_REMOVED_DESCRIPTION)));
}

// static
std::unique_ptr<AuthenticatorGenericErrorSheetModel>
AuthenticatorGenericErrorSheetModel::ForMissingCapability(
    AuthenticatorRequestDialogModel* dialog_model) {
  return base::WrapUnique(new AuthenticatorGenericErrorSheetModel(
      dialog_model,
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_MISSING_CAPABILITY_TITLE),
      l10n_util::GetStringFUTF16(IDS_WEBAUTHN_ERROR_MISSING_CAPABILITY_DESC,
                                 GetRelyingPartyIdString(dialog_model))));
}

// static
std::unique_ptr<AuthenticatorGenericErrorSheetModel>
AuthenticatorGenericErrorSheetModel::ForStorageFull(
    AuthenticatorRequestDialogModel* dialog_model) {
  return base::WrapUnique(new AuthenticatorGenericErrorSheetModel(
      dialog_model,
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_MISSING_CAPABILITY_TITLE),
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_STORAGE_FULL_DESC)));
}

std::unique_ptr<AuthenticatorGenericErrorSheetModel>
AuthenticatorGenericErrorSheetModel::ForWindowsHelloNotEnabled(
    AuthenticatorRequestDialogModel* dialog_model) {
  return base::WrapUnique(new AuthenticatorGenericErrorSheetModel(
      dialog_model,
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_WINDOWS_HELLO_NOT_ENABLED_TITLE),
      l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_WINDOWS_HELLO_NOT_ENABLED_DESCRIPTION)));
}

AuthenticatorGenericErrorSheetModel::AuthenticatorGenericErrorSheetModel(
    AuthenticatorRequestDialogModel* dialog_model,
    std::u16string title,
    std::u16string description)
    : AuthenticatorSheetModelBase(dialog_model),
      title_(std::move(title)),
      description_(std::move(description)) {
  vector_illustrations_.emplace(kPasskeyErrorIcon, kPasskeyErrorDarkIcon);
}

bool AuthenticatorGenericErrorSheetModel::IsBackButtonVisible() const {
  return false;
}

std::u16string AuthenticatorGenericErrorSheetModel::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

bool AuthenticatorGenericErrorSheetModel::IsAcceptButtonVisible() const {
  return dialog_model()->offer_try_again_in_ui();
}

bool AuthenticatorGenericErrorSheetModel::IsAcceptButtonEnabled() const {
  return true;
}

std::u16string AuthenticatorGenericErrorSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_RETRY);
}

std::u16string AuthenticatorGenericErrorSheetModel::GetStepTitle() const {
  return title_;
}

std::u16string AuthenticatorGenericErrorSheetModel::GetStepDescription() const {
  return description_;
}

void AuthenticatorGenericErrorSheetModel::OnAccept() {
  dialog_model()->StartOver();
}

// AuthenticatorResidentCredentialConfirmationSheetView -----------------------

// TODO(1358719): Add more specific illustration once available. The "error"
// graphic is a large question mark, so it looks visually very similar.
AuthenticatorResidentCredentialConfirmationSheetView::
    AuthenticatorResidentCredentialConfirmationSheetView(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  vector_illustrations_.emplace(kPasskeyErrorIcon, kPasskeyErrorDarkIcon);
}

AuthenticatorResidentCredentialConfirmationSheetView::
    ~AuthenticatorResidentCredentialConfirmationSheetView() = default;

bool AuthenticatorResidentCredentialConfirmationSheetView::IsBackButtonVisible()
    const {
  return false;
}

bool AuthenticatorResidentCredentialConfirmationSheetView::
    IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorResidentCredentialConfirmationSheetView::
    IsAcceptButtonEnabled() const {
  return true;
}

std::u16string
AuthenticatorResidentCredentialConfirmationSheetView::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CONTINUE);
}

std::u16string
AuthenticatorResidentCredentialConfirmationSheetView::GetStepTitle() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_GENERIC_TITLE,
                                    GetRelyingPartyIdString(dialog_model()));
}

std::u16string
AuthenticatorResidentCredentialConfirmationSheetView::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_RESIDENT_KEY_PRIVACY);
}

void AuthenticatorResidentCredentialConfirmationSheetView::OnAccept() {
  dialog_model()->OnResidentCredentialConfirmed();
}

// AuthenticatorSelectAccountSheetModel ---------------------------------------

AuthenticatorSelectAccountSheetModel::AuthenticatorSelectAccountSheetModel(
    AuthenticatorRequestDialogModel* dialog_model,
    UserVerificationMode mode,
    SelectionType type)
    : AuthenticatorSheetModelBase(
          dialog_model,
          mode == kPreUserVerification
              ? OtherMechanismButtonVisibility::kVisible
              : OtherMechanismButtonVisibility::kHidden),
      user_verification_mode_(mode),
      selection_type_(type) {
  vector_illustrations_.emplace(kPasskeyHeaderIcon, kPasskeyHeaderDarkIcon);
}

AuthenticatorSelectAccountSheetModel::~AuthenticatorSelectAccountSheetModel() =
    default;

AuthenticatorSelectAccountSheetModel::SelectionType
AuthenticatorSelectAccountSheetModel::selection_type() const {
  return selection_type_;
}

const device::DiscoverableCredentialMetadata&
AuthenticatorSelectAccountSheetModel::SingleCredential() const {
  DCHECK_EQ(selection_type_, kSingleAccount);
  DCHECK_EQ(dialog_model()->creds().size(), 1u);
  return dialog_model()->creds().at(0);
}

void AuthenticatorSelectAccountSheetModel::SetCurrentSelection(int selected) {
  DCHECK_EQ(selection_type_, kMultipleAccounts);
  DCHECK_LE(0, selected);
  DCHECK_LT(static_cast<size_t>(selected), dialog_model()->creds().size());
  selected_ = selected;
}

void AuthenticatorSelectAccountSheetModel::OnAccept() {
  const size_t index = selection_type_ == kMultipleAccounts ? selected_ : 0;
  switch (user_verification_mode_) {
    case kPreUserVerification:
      dialog_model()->OnAccountPreselectedIndex(index);
      break;
    case kPostUserVerification:
      dialog_model()->OnAccountSelected(index);
      break;
  }
}

std::u16string AuthenticatorSelectAccountSheetModel::GetStepTitle() const {
  switch (selection_type_) {
    case kSingleAccount:
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_USE_PASSKEY_TITLE,
          GetRelyingPartyIdString(dialog_model()));
    case kMultipleAccounts:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CHOOSE_PASSKEY_TITLE);
  }
}

std::u16string AuthenticatorSelectAccountSheetModel::GetStepDescription()
    const {
  switch (selection_type_) {
    case kSingleAccount:
      return u"";
    case kMultipleAccounts:
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_CHOOSE_PASSKEY_BODY,
          GetRelyingPartyIdString(dialog_model()));
  }
}

bool AuthenticatorSelectAccountSheetModel::IsAcceptButtonVisible() const {
  return selection_type_ == kSingleAccount;
}

bool AuthenticatorSelectAccountSheetModel::IsAcceptButtonEnabled() const {
  return selection_type_ == kSingleAccount;
}

std::u16string AuthenticatorSelectAccountSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CONTINUE);
}

// AttestationPermissionRequestSheetModel -------------------------------------

// TODO(1358719): Add more specific illustration once available.
AttestationPermissionRequestSheetModel::AttestationPermissionRequestSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  vector_illustrations_.emplace(kPasskeyUsbIcon, kPasskeyUsbDarkIcon);
}

AttestationPermissionRequestSheetModel::
    ~AttestationPermissionRequestSheetModel() = default;

void AttestationPermissionRequestSheetModel::OnAccept() {
  dialog_model()->OnAttestationPermissionResponse(true);
}

void AttestationPermissionRequestSheetModel::OnCancel() {
  dialog_model()->OnAttestationPermissionResponse(false);
}

std::u16string AttestationPermissionRequestSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_REQUEST_ATTESTATION_PERMISSION_TITLE);
}

std::u16string AttestationPermissionRequestSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringFUTF16(
      IDS_WEBAUTHN_REQUEST_ATTESTATION_PERMISSION_DESC,
      GetRelyingPartyIdString(dialog_model()));
}

bool AttestationPermissionRequestSheetModel::IsBackButtonVisible() const {
  return false;
}

bool AttestationPermissionRequestSheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AttestationPermissionRequestSheetModel::IsAcceptButtonEnabled() const {
  return true;
}

std::u16string AttestationPermissionRequestSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ALLOW_ATTESTATION);
}

bool AttestationPermissionRequestSheetModel::IsCancelButtonVisible() const {
  return true;
}

std::u16string AttestationPermissionRequestSheetModel::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_DENY_ATTESTATION);
}

// EnterpriseAttestationPermissionRequestSheetModel ---------------------------

EnterpriseAttestationPermissionRequestSheetModel::
    EnterpriseAttestationPermissionRequestSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AttestationPermissionRequestSheetModel(dialog_model) {}

std::u16string EnterpriseAttestationPermissionRequestSheetModel::GetStepTitle()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_REQUEST_ENTERPRISE_ATTESTATION_PERMISSION_TITLE);
}

std::u16string
EnterpriseAttestationPermissionRequestSheetModel::GetStepDescription() const {
  return l10n_util::GetStringFUTF16(
      IDS_WEBAUTHN_REQUEST_ENTERPRISE_ATTESTATION_PERMISSION_DESC,
      GetRelyingPartyIdString(dialog_model()));
}

// AuthenticatorQRSheetModel --------------------------------------------------

// No illustration since there already is the QR code.
AuthenticatorQRSheetModel::AuthenticatorQRSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {}

AuthenticatorQRSheetModel::~AuthenticatorQRSheetModel() = default;

std::u16string AuthenticatorQRSheetModel::GetStepTitle() const {
  switch (dialog_model()->transport_availability()->request_type) {
    case device::FidoRequestType::kMakeCredential:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CREATE_PASSKEY_QR_TITLE);
    case device::FidoRequestType::kGetAssertion:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_USE_PASSKEY_QR_TITLE);
  }
}

std::u16string AuthenticatorQRSheetModel::GetStepDescription() const {
  switch (dialog_model()->transport_availability()->request_type) {
    case device::FidoRequestType::kMakeCredential:
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_CREATE_PASSKEY_QR_BODY,
          GetRelyingPartyIdString(dialog_model()));
    case device::FidoRequestType::kGetAssertion:
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_USE_PASSKEY_QR_BODY,
          GetRelyingPartyIdString(dialog_model()));
  }
}

bool AuthenticatorQRSheetModel::ShowSecurityKeyLabel() const {
  return base::FeatureList::IsEnabled(device::kWebAuthnListSyncedPasskeys) &&
         base::Contains(
             dialog_model()->transport_availability()->available_transports,
             device::FidoTransportProtocol::kUsbHumanInterfaceDevice);
}

std::u16string AuthenticatorQRSheetModel::GetSecurityKeyLabel() const {
  // TODO(crbug.com/1459273): i18n.
  switch (dialog_model()->transport_availability()->request_type) {
    case device::FidoRequestType::kMakeCredential:
      return u"If you want to create passkey for " +
             base::UTF8ToUTF16(dialog_model()->relying_party_id()) +
             u" on a hardware security key, insert and touch it now "
             u"(UNTRANSLATED)";
    case device::FidoRequestType::kGetAssertion:
      return u"If your passkey for " +
             base::UTF8ToUTF16(dialog_model()->relying_party_id()) +
             u" is on a hardware security key, insert and touch it now "
             u"(UNTRANSLATED)";
  }
}

std::u16string AuthenticatorQRSheetModel::GetOtherMechanismButtonLabel() const {
  return base::FeatureList::IsEnabled(device::kWebAuthnListSyncedPasskeys)
             ? u"Back (UNTRANSLATED)"
             : l10n_util::GetStringUTF16(IDS_WEBAUTHN_USE_A_DIFFERENT_DEVICE);
}

// AuthenticatorConnectingSheetModel ------------------------------------------

AuthenticatorConnectingSheetModel::AuthenticatorConnectingSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kHidden) {
  lottie_illustrations_.emplace(IDR_WEBAUTHN_HYBRID_CONNECTING_LIGHT,
                                IDR_WEBAUTHN_HYBRID_CONNECTING_DARK);
}

AuthenticatorConnectingSheetModel::~AuthenticatorConnectingSheetModel() =
    default;

std::u16string AuthenticatorConnectingSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLEV2_CONNECTING_TITLE);
}

std::u16string AuthenticatorConnectingSheetModel::GetStepDescription() const {
  return u"";
}

// AuthenticatorConnectedSheetModel ------------------------------------------

AuthenticatorConnectedSheetModel::AuthenticatorConnectedSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kHidden) {
  vector_illustrations_.emplace(kPasskeyPhoneIcon, kPasskeyPhoneDarkIcon);
}

AuthenticatorConnectedSheetModel::~AuthenticatorConnectedSheetModel() = default;

bool AuthenticatorConnectedSheetModel::IsActivityIndicatorVisible() const {
  return false;
}

std::u16string AuthenticatorConnectedSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLEV2_CONNECTED_DESCRIPTION);
}

std::u16string AuthenticatorConnectedSheetModel::GetStepDescription() const {
  return u"";
}

// AuthenticatorCableErrorSheetModel ------------------------------------------

AuthenticatorCableErrorSheetModel::AuthenticatorCableErrorSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kHidden) {
  vector_illustrations_.emplace(kPasskeyErrorIcon, kPasskeyErrorDarkIcon);
}

AuthenticatorCableErrorSheetModel::~AuthenticatorCableErrorSheetModel() =
    default;

bool AuthenticatorCableErrorSheetModel::IsOtherMechanismButtonVisible() const {
  return false;
}

std::u16string AuthenticatorCableErrorSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_GENERIC_TITLE);
}

std::u16string AuthenticatorCableErrorSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLEV2_ERROR_DESCRIPTION);
}

std::u16string AuthenticatorCableErrorSheetModel::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLEV2_ERROR_CLOSE);
}

// AuthenticatorCreatePasskeySheetModel
// --------------------------------------------------

AuthenticatorCreatePasskeySheetModel::AuthenticatorCreatePasskeySheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {
  vector_illustrations_.emplace(kPasskeyHeaderIcon, kPasskeyHeaderDarkIcon);
}

AuthenticatorCreatePasskeySheetModel::~AuthenticatorCreatePasskeySheetModel() =
    default;

std::u16string AuthenticatorCreatePasskeySheetModel::GetStepTitle() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_CREATE_PASSKEY_TITLE,
                                    GetRelyingPartyIdString(dialog_model()));
}

std::u16string AuthenticatorCreatePasskeySheetModel::GetStepDescription()
    const {
  return u"";
}

std::u16string
AuthenticatorCreatePasskeySheetModel::passkey_storage_description() const {
#if BUILDFLAG(IS_WIN)
  return l10n_util::GetStringUTF16(
      dialog_model()->transport_availability()->is_off_the_record_context
          ? IDS_WEBAUTHN_CREATE_PASSKEY_EXTRA_WIN_INCOGNITO
          : IDS_WEBAUTHN_CREATE_PASSKEY_EXTRA_WIN);
#else
  return l10n_util::GetStringUTF16(
      dialog_model()->transport_availability()->is_off_the_record_context
          ? IDS_WEBAUTHN_CREATE_PASSKEY_EXTRA_INCOGNITO
          : IDS_WEBAUTHN_CREATE_PASSKEY_EXTRA);
#endif
}

bool AuthenticatorCreatePasskeySheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorCreatePasskeySheetModel::IsAcceptButtonEnabled() const {
  return true;
}

std::u16string AuthenticatorCreatePasskeySheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CONTINUE);
}

void AuthenticatorCreatePasskeySheetModel::OnAccept() {
  dialog_model()->HideDialogAndDispatchToPlatformAuthenticator();
}

// AuthenticatorPhoneConfirmationSheet --------------------------------

AuthenticatorPhoneConfirmationSheet::AuthenticatorPhoneConfirmationSheet(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {
  vector_illustrations_.emplace(kPasskeyPhoneIcon, kPasskeyPhoneDarkIcon);
}

AuthenticatorPhoneConfirmationSheet::~AuthenticatorPhoneConfirmationSheet() =
    default;

std::u16string AuthenticatorPhoneConfirmationSheet::GetStepTitle() const {
  return l10n_util::GetStringFUTF16(
      IDS_WEBAUTHN_PHONE_CONFIRMATION_TITLE,
      base::UTF8ToUTF16(dialog_model()->paired_phone_names().at(0)),
      GetRelyingPartyIdString(dialog_model()));
}

std::u16string AuthenticatorPhoneConfirmationSheet::GetStepDescription() const {
  return u"";
}

bool AuthenticatorPhoneConfirmationSheet::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorPhoneConfirmationSheet::IsAcceptButtonEnabled() const {
  return true;
}

std::u16string AuthenticatorPhoneConfirmationSheet::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CONTINUE);
}

void AuthenticatorPhoneConfirmationSheet::OnAccept() {
  dialog_model()->ContactPriorityPhone();
}

// AuthenticatorMultiSourcePickerSheetModel --------------------------------

AuthenticatorMultiSourcePickerSheetModel::
    AuthenticatorMultiSourcePickerSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  vector_illustrations_.emplace(kPasskeyHeaderIcon, kPasskeyHeaderDarkIcon);

  using CredentialMech = AuthenticatorRequestDialogModel::Mechanism::Credential;
  bool has_local_passkeys =
      std::ranges::any_of(dialog_model->mechanisms(), [](const auto& mech) {
        return absl::holds_alternative<CredentialMech>(mech.type) &&
               absl::get<CredentialMech>(mech.type).value() !=
                   device::AuthenticatorType::kPhone;
      });
  // TODO(crbug.com/1459273): i18n.
  if (has_local_passkeys) {
    primary_passkeys_label_ = u"From this device (UNTRANSLATED)";
    for (size_t i = 0; i < dialog_model->mechanisms().size(); ++i) {
      const AuthenticatorRequestDialogModel::Mechanism& mech =
          dialog_model->mechanisms()[i];
      if (absl::holds_alternative<CredentialMech>(mech.type) &&
          absl::get<CredentialMech>(mech.type).value() !=
              device::AuthenticatorType::kPhone) {
        primary_passkey_indices_.push_back(i);
      } else {
        secondary_passkey_indices_.push_back(i);
      }
    }
    return;
  }

  const absl::optional<std::u16string>& phone_name =
      dialog_model->GetPrioritySyncedPhoneName();
  if (phone_name) {
    primary_passkeys_label_ =
        std::u16string(u"From \"") + *phone_name + u"\" (UNTRANSLATED)";
  }
  for (size_t i = 0; i < dialog_model->mechanisms().size(); ++i) {
    const AuthenticatorRequestDialogModel::Mechanism& mech =
        dialog_model->mechanisms()[i];
    if (absl::holds_alternative<CredentialMech>(mech.type) &&
        absl::get<CredentialMech>(mech.type).value() ==
            device::AuthenticatorType::kPhone) {
      // There should not be any phone passkeys if the phone name is empty.
      CHECK(phone_name);
      primary_passkey_indices_.push_back(i);
    } else {
      secondary_passkey_indices_.push_back(i);
    }
  }
}

AuthenticatorMultiSourcePickerSheetModel::
    ~AuthenticatorMultiSourcePickerSheetModel() = default;

std::u16string AuthenticatorMultiSourcePickerSheetModel::GetStepTitle() const {
  return base::UTF8ToUTF16(std::string("Choose a passkey for ") +
                           dialog_model()->relying_party_id() +
                           " (UNTRANSLATED)");
}

std::u16string AuthenticatorMultiSourcePickerSheetModel::GetStepDescription()
    const {
  return u"";
}

// AuthenticatorPriorityMechanismSheetModel --------------------------------

AuthenticatorPriorityMechanismSheetModel::
    AuthenticatorPriorityMechanismSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {
  vector_illustrations_.emplace(kPasskeyHeaderIcon, kPasskeyHeaderDarkIcon);
}
AuthenticatorPriorityMechanismSheetModel::
    ~AuthenticatorPriorityMechanismSheetModel() = default;

std::u16string AuthenticatorPriorityMechanismSheetModel::GetStepTitle() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_USE_PASSKEY_TITLE,
                                    GetRelyingPartyIdString(dialog_model()));
}

std::u16string AuthenticatorPriorityMechanismSheetModel::GetStepDescription()
    const {
  return u"";
}

bool AuthenticatorPriorityMechanismSheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorPriorityMechanismSheetModel::IsAcceptButtonEnabled() const {
  return true;
}

std::u16string AuthenticatorPriorityMechanismSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CONTINUE);
}

void AuthenticatorPriorityMechanismSheetModel::OnAccept() {
  dialog_model()
      ->mechanisms()[*dialog_model()->priority_mechanism_index()]
      .callback.Run();
}
