// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/sheet_models.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/webauthn/user_actions.h"
#include "chrome/browser/ui/webauthn/webauthn_ui_helpers.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_types.h"
#include "device/fido/pin.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "crypto/scoped_lacontext.h"
#include "device/fido/mac/util.h"
#endif

namespace {

using CredentialMech = AuthenticatorRequestDialogModel::Mechanism::Credential;
using EnclaveMech = AuthenticatorRequestDialogModel::Mechanism::Enclave;
using ICloudKeychainMech =
    AuthenticatorRequestDialogModel::Mechanism::ICloudKeychain;
using Step = AuthenticatorRequestDialogModel::Step;

constexpr int kGpmArbitraryPinMinLength = 4;

bool IsLocalPasskeyOrEnclaveAuthenticator(
    const AuthenticatorRequestDialogModel::Mechanism& mech) {
  return (absl::holds_alternative<CredentialMech>(mech.type) &&
          absl::get<CredentialMech>(mech.type).value().source !=
              device::AuthenticatorType::kPhone) ||
         absl::holds_alternative<EnclaveMech>(mech.type);
}

// Possibly returns a resident key warning if the model indicates that it's
// needed.
std::u16string PossibleResidentKeyWarning(
    AuthenticatorRequestDialogModel* dialog_model) {
  switch (dialog_model->resident_key_requirement) {
    case device::ResidentKeyRequirement::kDiscouraged:
      return std::u16string();
    case device::ResidentKeyRequirement::kPreferred:
      return l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_RESIDENT_KEY_PREFERRED_PRIVACY);
    case device::ResidentKeyRequirement::kRequired:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_RESIDENT_KEY_PRIVACY);
  }
  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

// Return a warning about attestation if attestation was requested, otherwise
// return an empty string.
std::u16string PossibleAttestationWarning(
    AuthenticatorRequestDialogModel* dialog_model) {
  if (!dialog_model->attestation_conveyance_preference) {
    return std::u16string();
  }

  switch (*dialog_model->attestation_conveyance_preference) {
    case device::AttestationConveyancePreference::kNone:
      return std::u16string();
    case device::AttestationConveyancePreference::kIndirect:
    case device::AttestationConveyancePreference::kDirect:
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_ATTESTATION_WARNING,
          AuthenticatorSheetModelBase::GetRelyingPartyIdString(dialog_model));
    case device::AttestationConveyancePreference::
        kEnterpriseIfRPListedOnAuthenticator:
    case device::AttestationConveyancePreference::kEnterpriseApprovedByBrowser:
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_ENTERPRISE_ATTESTATION_WARNING,
          AuthenticatorSheetModelBase::GetRelyingPartyIdString(dialog_model));
  }
}

}  // namespace

// AuthenticatorSheetModelBase ------------------------------------------------

AuthenticatorSheetModelBase::AuthenticatorSheetModelBase(
    AuthenticatorRequestDialogModel* dialog_model)
    : dialog_model_(dialog_model) {
  DCHECK(dialog_model);
  dialog_model_->observers.AddObserver(this);
}

AuthenticatorSheetModelBase::AuthenticatorSheetModelBase(
    AuthenticatorRequestDialogModel* dialog_model,
    OtherMechanismButtonVisibility other_mechanism_button_visibility)
    : AuthenticatorSheetModelBase(dialog_model) {
  other_mechanism_button_visibility_ = other_mechanism_button_visibility;
}

AuthenticatorSheetModelBase::~AuthenticatorSheetModelBase() {
  if (dialog_model_) {
    dialog_model_->observers.RemoveObserver(this);
    dialog_model_ = nullptr;
  }
}

// static
std::u16string AuthenticatorSheetModelBase::GetRelyingPartyIdString(
    const AuthenticatorRequestDialogModel* dialog_model) {
  // The preferred width of medium snap point modal dialog view is 448 dp, but
  // we leave some room for padding between the text and the modal views.
  static constexpr int kDialogWidth = 300;
  return webauthn_ui_helpers::RpIdToElidedHost(dialog_model->relying_party_id,
                                               kDialogWidth);
}

bool AuthenticatorSheetModelBase::IsActivityIndicatorVisible() const {
  return dialog_model_->ui_disabled_;
}

bool AuthenticatorSheetModelBase::IsCancelButtonVisible() const {
  return true;
}

bool AuthenticatorSheetModelBase::IsOtherMechanismButtonVisible() const {
  return other_mechanism_button_visibility_ ==
             OtherMechanismButtonVisibility::kVisible &&
         dialog_model_ && dialog_model_->mechanisms.size() > 1;
}

std::u16string AuthenticatorSheetModelBase::GetOtherMechanismButtonLabel()
    const {
  switch (dialog_model()->request_type) {
    case device::FidoRequestType::kMakeCredential:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_SAVE_ANOTHER_WAY);
    case device::FidoRequestType::kGetAssertion:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_USE_A_DIFFERENT_PASSKEY);
  }
}

std::u16string AuthenticatorSheetModelBase::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool AuthenticatorSheetModelBase::IsAcceptButtonVisible() const {
  return false;
}

bool AuthenticatorSheetModelBase::IsAcceptButtonEnabled() const {
  return !dialog_model_->ui_disabled_;
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
  NOTREACHED_IN_MIGRATION();
}

void AuthenticatorSheetModelBase::OnCancel() {
  if (dialog_model()) {
    webauthn::user_actions::RecordCancelClick();
    dialog_model()->CancelAuthenticatorRequest();
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
  lottie_illustrations_.emplace(IDR_WEBAUTHN_PASSKEY_LIGHT,
                                IDR_WEBAUTHN_PASSKEY_DARK);
  webauthn::user_actions::RecordMultipleOptionsShown(dialog_model->mechanisms,
                                                     /*is_create=*/true);
}

std::u16string AuthenticatorMechanismSelectorSheetModel::GetStepTitle() const {
  CHECK_EQ(dialog_model()->request_type,
           device::FidoRequestType::kMakeCredential);
  return l10n_util::GetStringFUTF16(
      IDS_WEBAUTHN_CREATE_PASSKEY_CHOOSE_DEVICE_TITLE,
      GetRelyingPartyIdString(dialog_model()));
}

std::u16string AuthenticatorMechanismSelectorSheetModel::GetStepDescription()
    const {
  return u"";
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

std::vector<std::u16string>
AuthenticatorInsertAndActivateUsbSheetModel::GetAdditionalDescriptions() const {
  return {PossibleAttestationWarning(dialog_model()),
          PossibleResidentKeyWarning(dialog_model())};
}

// AuthenticatorTimeoutErrorModel ---------------------------------------------

AuthenticatorTimeoutErrorModel::AuthenticatorTimeoutErrorModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  vector_illustrations_.emplace(kPasskeyErrorIcon, kPasskeyErrorDarkIcon);
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

std::u16string AuthenticatorNotRegisteredErrorModel::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

bool AuthenticatorNotRegisteredErrorModel::IsAcceptButtonVisible() const {
  return dialog_model()->offer_try_again_in_ui;
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

std::u16string AuthenticatorAlreadyRegisteredErrorModel::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

bool AuthenticatorAlreadyRegisteredErrorModel::IsAcceptButtonVisible() const {
  return dialog_model()->offer_try_again_in_ui;
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

bool AuthenticatorInternalUnrecognizedErrorSheetModel::IsAcceptButtonVisible()
    const {
  return dialog_model()->offer_try_again_in_ui;
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
  return dialog_model()->ble_adapter_is_powered;
}

std::u16string AuthenticatorBlePowerOnManualSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLUETOOTH_POWER_ON_MANUAL_NEXT);
}

void AuthenticatorBlePowerOnManualSheetModel::OnBluetoothPoweredStateChanged() {
  dialog_model()->OnSheetModelChanged();
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
  dialog_model()->OnSheetModelChanged();
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

// AuthenticatorTouchIdSheetModel
// ------------------------------------

AuthenticatorTouchIdSheetModel::AuthenticatorTouchIdSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {
  webauthn::user_actions::RecordGpmTouchIdDialogShown(
      dialog_model->request_type == device::FidoRequestType::kMakeCredential);
}

std::u16string AuthenticatorTouchIdSheetModel::GetStepTitle() const {
  const std::u16string rp_id = GetRelyingPartyIdString(dialog_model());
  switch (dialog_model()->request_type) {
    case device::FidoRequestType::kMakeCredential:
      return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_GPM_CREATE_PASSKEY_TITLE,
                                        rp_id);
    case device::FidoRequestType::kGetAssertion:
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_CHOOSE_PASSKEY_FOR_RP_TITLE, rp_id);
  }
}

std::u16string AuthenticatorTouchIdSheetModel::GetStepDescription() const {
  switch (dialog_model()->request_type) {
    case device::FidoRequestType::kMakeCredential:
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_GPM_CREATE_PASSKEY_DESC,
          base::UTF8ToUTF16(dialog_model()->GetGpmAccountEmail()));

    case device::FidoRequestType::kGetAssertion:
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_TOUCH_ID_ASSERTION_DESC,
          GetRelyingPartyIdString(dialog_model()));
  }
}

bool AuthenticatorTouchIdSheetModel::IsAcceptButtonVisible() const {
  return !device::fido::mac::DeviceHasBiometricsAvailable();
}

bool AuthenticatorTouchIdSheetModel::IsCancelButtonVisible() const {
  return true;
}

std::u16string AuthenticatorTouchIdSheetModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_TOUCH_ID_ENTER_PASSWORD);
}

void AuthenticatorTouchIdSheetModel::OnAccept() {
  if (touch_id_completed_) {
    return;
  }
  webauthn::user_actions::RecordAcceptClick();
  touch_id_completed_ = true;
  dialog_model()->OnTouchIDComplete(false);
}

void AuthenticatorTouchIdSheetModel::OnTouchIDSensorTapped(
    std::optional<crypto::ScopedLAContext> lacontext) {
  // Ignore Touch ID ceremony status after the user has completed the ceremony.
  if (touch_id_completed_) {
    return;
  }
  if (!lacontext) {
    // Authentication failed. Update the button status and rebuild the sheet,
    // which will restart the Touch ID request if the sensor is not softlocked
    // or display a padlock icon if it is.
    dialog_model()->OnSheetModelChanged();
    return;
  }
  touch_id_completed_ = true;
  dialog_model()->lacontext = std::move(lacontext);
  dialog_model()->OnTouchIDComplete(true);
}

#endif  // IS_MAC

// AuthenticatorOffTheRecordInterstitialSheetModel
// -----------------------------------------

AuthenticatorOffTheRecordInterstitialSheetModel::
    AuthenticatorOffTheRecordInterstitialSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  // TODO(crbug.com/40237082): Add more specific illustration once available.
  // The "error" graphic is a large question mark, so it looks visually very
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
                                  OtherMechanismButtonVisibility::kHidden) {
  vector_illustrations_.emplace(kPasskeyPhoneIcon, kPasskeyPhoneDarkIcon);
}

AuthenticatorPaaskSheetModel::~AuthenticatorPaaskSheetModel() = default;

bool AuthenticatorPaaskSheetModel::IsActivityIndicatorVisible() const {
  return true;
}

std::u16string AuthenticatorPaaskSheetModel::GetStepTitle() const {
  switch (*dialog_model()->cable_ui_type) {
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
  switch (*dialog_model()->cable_ui_type) {
    case AuthenticatorRequestDialogModel::CableUIType::CABLE_V1:
    case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_SERVER_LINK:
      // caBLEv1 and v2 server-link don't include device names.
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLE_ACTIVATE_DESCRIPTION);
    case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_2ND_FACTOR: {
      DCHECK(dialog_model()->selected_phone_name);
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_CABLE_ACTIVATE_DEVICE_NAME_DESCRIPTION,
          base::UTF8ToUTF16(dialog_model()->selected_phone_name.value_or("")));
    }
  }
}

bool AuthenticatorPaaskSheetModel::IsManageDevicesButtonVisible() const {
  return true;
}

void AuthenticatorPaaskSheetModel::OnManageDevices() {
  dialog_model()->OnManageDevicesClicked();
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
          IDS_WEBAUTHN_PIN_ENTRY_ERROR_TOO_SHORT, dialog_model->min_pin_length);
      break;
    case device::pin::PINEntryError::kWrongPIN:
      std::optional<int> attempts = dialog_model->pin_attempts;
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

std::u16string AuthenticatorClientPinEntrySheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_NEXT);
}

void AuthenticatorClientPinEntrySheetModel::OnAccept() {
  if ((mode_ == Mode::kPinChange || mode_ == Mode::kPinSetup) &&
      pin_code_ != pin_confirmation_) {
    error_ = l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_ERROR_MISMATCH);
    dialog_model()->OnSheetModelChanged();
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

std::vector<std::u16string>
AuthenticatorClientPinTapAgainSheetModel::GetAdditionalDescriptions() const {
  return {PossibleAttestationWarning(dialog_model()),
          PossibleResidentKeyWarning(dialog_model())};
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

bool AuthenticatorBioEnrollmentSheetModel::IsAcceptButtonVisible() const {
  return dialog_model()->bio_samples_remaining &&
         dialog_model()->bio_samples_remaining <= 0;
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
  int attempts = *dialog_model()->uv_attempts;
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

std::u16string AuthenticatorGenericErrorSheetModel::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

bool AuthenticatorGenericErrorSheetModel::IsAcceptButtonVisible() const {
  return dialog_model()->offer_try_again_in_ui;
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

// TODO(crbug.com/40237082): Add more specific illustration once available. The
// "error" graphic is a large question mark, so it looks visually very similar.
AuthenticatorResidentCredentialConfirmationSheetView::
    AuthenticatorResidentCredentialConfirmationSheetView(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  vector_illustrations_.emplace(kPasskeyErrorIcon, kPasskeyErrorDarkIcon);
}

AuthenticatorResidentCredentialConfirmationSheetView::
    ~AuthenticatorResidentCredentialConfirmationSheetView() = default;

bool AuthenticatorResidentCredentialConfirmationSheetView::
    IsAcceptButtonVisible() const {
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
  lottie_illustrations_.emplace(IDR_WEBAUTHN_PASSKEY_LIGHT,
                                IDR_WEBAUTHN_PASSKEY_DARK);
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
  DCHECK_EQ(dialog_model()->creds.size(), 1u);
  return dialog_model()->creds.at(0);
}

void AuthenticatorSelectAccountSheetModel::SetCurrentSelection(int selected) {
  DCHECK_EQ(selection_type_, kMultipleAccounts);
  DCHECK_LE(0, selected);
  DCHECK_LT(static_cast<size_t>(selected), dialog_model()->creds.size());
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

std::u16string AuthenticatorSelectAccountSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CONTINUE);
}

// AuthenticatorQRSheetModel --------------------------------------------------

// No illustration since there already is the QR code.
AuthenticatorQRSheetModel::AuthenticatorQRSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {}

AuthenticatorQRSheetModel::~AuthenticatorQRSheetModel() = default;

std::u16string AuthenticatorQRSheetModel::GetStepTitle() const {
  switch (dialog_model()->request_type) {
    case device::FidoRequestType::kMakeCredential:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CREATE_PASSKEY_QR_TITLE);
    case device::FidoRequestType::kGetAssertion:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_USE_PASSKEY_QR_TITLE);
  }
}

std::u16string AuthenticatorQRSheetModel::GetStepDescription() const {
  switch (dialog_model()->request_type) {
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

std::vector<std::u16string> AuthenticatorQRSheetModel::GetSecurityKeyLabels()
    const {
  if (!dialog_model()->show_security_key_on_qr_sheet) {
    return {};
  }

  switch (dialog_model()->request_type) {
    case device::FidoRequestType::kMakeCredential: {
      std::u16string body_text = l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_QR_CREATE_PASSKEY_ON_SECURITY_KEY_LABEL,
          GetRelyingPartyIdString(dialog_model()));
      std::u16string attestation_warning =
          PossibleAttestationWarning(dialog_model());
      if (attestation_warning.empty()) {
        return {body_text};
      } else {
        return {body_text, attestation_warning};
      }
    }
    case device::FidoRequestType::kGetAssertion:
      return {l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_QR_USE_PASSKEY_ON_SECURITY_KEY_LABEL,
          GetRelyingPartyIdString(dialog_model()))};
  }
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
  lottie_illustrations_.emplace(IDR_WEBAUTHN_PASSKEY_LIGHT,
                                IDR_WEBAUTHN_PASSKEY_DARK);
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
      dialog_model()->is_off_the_record
          ? IDS_WEBAUTHN_CREATE_PASSKEY_EXTRA_WIN_INCOGNITO
          : IDS_WEBAUTHN_CREATE_PASSKEY_EXTRA_WIN);
#else
  return l10n_util::GetStringUTF16(
      dialog_model()->is_off_the_record
          ? IDS_WEBAUTHN_CREATE_PASSKEY_EXTRA_INCOGNITO
          : IDS_WEBAUTHN_CREATE_PASSKEY_EXTRA);
#endif
}

bool AuthenticatorCreatePasskeySheetModel::IsAcceptButtonVisible() const {
  return true;
}

std::u16string AuthenticatorCreatePasskeySheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CONTINUE);
}

void AuthenticatorCreatePasskeySheetModel::OnAccept() {
  dialog_model()->OnCreatePasskeyAccepted();
}

// AuthenticatorGPMErrorSheetModel -------------------------------------------

AuthenticatorGPMErrorSheetModel::AuthenticatorGPMErrorSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kHidden) {
  vector_illustrations_.emplace(kPasskeyErrorIcon, kPasskeyErrorDarkIcon);
  webauthn::user_actions::RecordGpmFailureShown();
}

AuthenticatorGPMErrorSheetModel::~AuthenticatorGPMErrorSheetModel() = default;

std::u16string AuthenticatorGPMErrorSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_ERROR_TITLE);
}

std::u16string AuthenticatorGPMErrorSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_ERROR_DESC);
}

// AuthenticatorGPMConnectingSheetModel --------------------------------------

AuthenticatorGPMConnectingSheetModel::AuthenticatorGPMConnectingSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kHidden) {
  lottie_illustrations_.emplace(IDR_WEBAUTHN_HYBRID_CONNECTING_LIGHT,
                                IDR_WEBAUTHN_HYBRID_CONNECTING_DARK);
}

AuthenticatorGPMConnectingSheetModel::~AuthenticatorGPMConnectingSheetModel() =
    default;

std::u16string AuthenticatorGPMConnectingSheetModel::GetStepTitle() const {
  return u"";
}

std::u16string AuthenticatorGPMConnectingSheetModel::GetStepDescription()
    const {
  return u"";
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
      base::UTF8ToUTF16(dialog_model()->paired_phone_names.at(0)),
      GetRelyingPartyIdString(dialog_model()));
}

std::u16string AuthenticatorPhoneConfirmationSheet::GetStepDescription() const {
  return u"";
}

bool AuthenticatorPhoneConfirmationSheet::IsAcceptButtonVisible() const {
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
  lottie_illustrations_.emplace(IDR_WEBAUTHN_PASSKEY_LIGHT,
                                IDR_WEBAUTHN_PASSKEY_DARK);

  webauthn::user_actions::RecordMultipleOptionsShown(dialog_model->mechanisms,
                                                     /*is_create=*/false);
  if (base::ranges::any_of(dialog_model->mechanisms,
                           &IsLocalPasskeyOrEnclaveAuthenticator)) {
    primary_passkeys_label_ =
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_THIS_DEVICE_LABEL);
    for (size_t i = 0; i < dialog_model->mechanisms.size(); ++i) {
      const AuthenticatorRequestDialogModel::Mechanism& mech =
          dialog_model->mechanisms[i];
      if (IsLocalPasskeyOrEnclaveAuthenticator(mech) ||
          // iCloud Keychain appears in the primary list if present. This
          // happens when Chrome does not have permission to enumerate
          // credentials from iCloud Keychain. Thus this generic option is the
          // only way for the user to trigger it.
          absl::holds_alternative<ICloudKeychainMech>(mech.type)) {
        primary_passkey_indices_.push_back(i);
      } else {
        secondary_passkey_indices_.push_back(i);
      }
    }
    return;
  }

  const std::optional<std::string>& phone_name =
      dialog_model->priority_phone_name;
  if (phone_name) {
    primary_passkeys_label_ = l10n_util::GetStringFUTF16(
        IDS_WEBAUTHN_FROM_PHONE_LABEL, base::UTF8ToUTF16(*phone_name));
  }
  for (size_t i = 0; i < dialog_model->mechanisms.size(); ++i) {
    const AuthenticatorRequestDialogModel::Mechanism& mech =
        dialog_model->mechanisms[i];
    if (absl::holds_alternative<CredentialMech>(mech.type) &&
        absl::get<CredentialMech>(mech.type).value().source ==
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

bool AuthenticatorMultiSourcePickerSheetModel::IsManageDevicesButtonVisible()
    const {
  using Mechanism = AuthenticatorRequestDialogModel::Mechanism;
  // If any phones or passkeys from a phone are shown then also show a button
  // that goes to the settings page to manage them.
  return base::ranges::any_of(
      dialog_model()->mechanisms, [](const Mechanism& mech) {
        return absl::holds_alternative<Mechanism::Phone>(mech.type) ||
               (absl::holds_alternative<Mechanism::Credential>(mech.type) &&
                absl::get<Mechanism::Credential>(mech.type).value().source ==
                    device::AuthenticatorType::kPhone);
      });
}

void AuthenticatorMultiSourcePickerSheetModel::OnManageDevices() {
  if (dialog_model()) {
    dialog_model()->OnManageDevicesClicked();
  }
}

std::u16string AuthenticatorMultiSourcePickerSheetModel::GetStepTitle() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_CHOOSE_PASSKEY_FOR_RP_TITLE,
                                    GetRelyingPartyIdString(dialog_model()));
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
  lottie_illustrations_.emplace(IDR_WEBAUTHN_PASSKEY_LIGHT,
                                IDR_WEBAUTHN_PASSKEY_DARK);

  webauthn::user_actions::RecordPriorityOptionShown(
      dialog_model->mechanisms[*dialog_model->priority_mechanism_index]);
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

std::u16string AuthenticatorPriorityMechanismSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CONTINUE);
}

void AuthenticatorPriorityMechanismSheetModel::OnAccept() {
  dialog_model()->OnUserConfirmedPriorityMechanism();
}

// AuthenticatorGpmPinSheetModelBase -------------------------------------------

AuthenticatorGpmPinSheetModelBase::AuthenticatorGpmPinSheetModelBase(
    AuthenticatorRequestDialogModel* dialog_model,
    Mode mode)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kHidden),
      mode_(mode) {}

AuthenticatorGpmPinSheetModelBase::~AuthenticatorGpmPinSheetModelBase() =
    default;

std::u16string AuthenticatorGpmPinSheetModelBase::GetGpmAccountEmail() const {
  std::optional<AccountInfo> account_info = dialog_model()->GetGpmAccountInfo();
  if (!account_info) {
    return std::u16string();
  }
  return base::UTF8ToUTF16(account_info->email);
}

std::u16string AuthenticatorGpmPinSheetModelBase::GetGpmAccountName() const {
  std::optional<AccountInfo> account_info = dialog_model()->GetGpmAccountInfo();
  if (!account_info) {
    return std::u16string();
  }
  return base::UTF8ToUTF16(account_info->full_name);
}

gfx::Image AuthenticatorGpmPinSheetModelBase::GetGpmAccountImage() const {
  std::optional<AccountInfo> account_info = dialog_model()->GetGpmAccountInfo();
  if (!account_info) {
    return gfx::Image();
  }
  gfx::Image account_image = account_info->account_image;
  if (account_image.IsEmpty()) {
    account_image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }
  constexpr int kAvatarIconSize = 32;
  return profiles::GetSizedAvatarIcon(account_image,
                                      /*width=*/kAvatarIconSize,
                                      /*height=*/kAvatarIconSize,
                                      profiles::SHAPE_CIRCLE);
}

std::u16string AuthenticatorGpmPinSheetModelBase::GetAccessibleDescription()
    const {
  std::u16string error = GetError();
  return error.empty() ? GetHint() : error;
}

bool AuthenticatorGpmPinSheetModelBase::ui_disabled() const {
  return dialog_model()->ui_disabled_;
}

std::u16string AuthenticatorGpmPinSheetModelBase::GetStepTitle() const {
  switch (mode_) {
    case Mode::kPinCreate:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_CREATE_PIN_TITLE);
    case Mode::kPinEntry:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_ENTER_PIN_TITLE);
  }
}

std::u16string AuthenticatorGpmPinSheetModelBase::GetStepDescription() const {
  switch (mode_) {
    case Mode::kPinCreate:
      return l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_CREATE_PIN_DESC);
    case Mode::kPinEntry:
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_GPM_ENTER_PIN_DESC,
          GetRelyingPartyIdString(dialog_model()));
  }
}

std::u16string AuthenticatorGpmPinSheetModelBase::GetError() const {
  std::optional<int> remaining_attempts =
      dialog_model()->gpm_pin_remaining_attempts_;
  return remaining_attempts && mode_ == Mode::kPinEntry
             ? l10n_util::GetPluralStringFUTF16(
                   IDS_WEBAUTHN_GPM_WRONG_PIN_ERROR, *remaining_attempts)
             : std::u16string();
}

bool AuthenticatorGpmPinSheetModelBase::IsForgotGPMPinButtonVisible() const {
  return mode_ == Mode::kPinEntry;
}

bool AuthenticatorGpmPinSheetModelBase::IsGPMPinOptionsButtonVisible() const {
  return mode_ == Mode::kPinCreate;
}

void AuthenticatorGpmPinSheetModelBase::OnAccept() {
  webauthn::user_actions::RecordAcceptClick();
  dialog_model()->OnGPMPinEntered(pin_);
}

void AuthenticatorGpmPinSheetModelBase::OnForgotGPMPin() const {
  webauthn::user_actions::RecordGpmForgotPinClick();
  dialog_model()->OnForgotGPMPinPressed();
}

void AuthenticatorGpmPinSheetModelBase::OnGPMPinOptionChosen(
    bool is_arbitrary) const {
  if ((dialog_model()->step() == Step::kGPMChangeArbitraryPin ||
       dialog_model()->step() == Step::kGPMCreateArbitraryPin ||
       dialog_model()->step() == Step::kGPMEnterArbitraryPin) &&
      is_arbitrary) {
    // The sheet already facilitates entering arbitrary pin.
    return;
  }
  if ((dialog_model()->step() == Step::kGPMChangePin ||
       dialog_model()->step() == Step::kGPMCreatePin ||
       dialog_model()->step() == Step::kGPMEnterPin) &&
      !is_arbitrary) {
    // The sheet already facilitates entering six digit pin.
    return;
  }
  webauthn::user_actions::RecordGpmPinOptionChangeClick();
  dialog_model()->OnGPMPinOptionChanged(is_arbitrary);
}

// AuthenticatorGpmPinSheetModel -----------------------------------------------

AuthenticatorGpmPinSheetModel::AuthenticatorGpmPinSheetModel(
    AuthenticatorRequestDialogModel* dialog_model,
    int pin_digits_count,
    Mode mode)
    : AuthenticatorGpmPinSheetModelBase(dialog_model, mode),
      pin_digits_count_(pin_digits_count) {
  webauthn::user_actions::RecordGpmPinSheetShown(
      /*is_credential_creation=*/dialog_model->request_type ==
          device::FidoRequestType::kMakeCredential,
      /*is_pin_creation=*/mode == Mode::kPinCreate,
      /*is_arbitrary=*/false);
}

AuthenticatorGpmPinSheetModel::~AuthenticatorGpmPinSheetModel() = default;

void AuthenticatorGpmPinSheetModel::PinCharTyped(bool is_digit) {
  if (mode_ != Mode::kPinCreate || show_digit_hint_ != is_digit) {
    return;
  }

  show_digit_hint_ = !is_digit;
  dialog_model()->OnSheetModelChanged();
}

int AuthenticatorGpmPinSheetModel::pin_digits_count() const {
  return pin_digits_count_;
}

void AuthenticatorGpmPinSheetModel::SetPin(std::u16string pin) {
  bool full_pin_typed_before = FullPinTyped();
  pin_ = std::move(pin);
  bool full_pin_typed = FullPinTyped();

  // When entering an existing PIN, the dialog completes as soon as all the
  // digits have been typed. When creating a new PIN, the user has to hit enter
  // to confirm.
  if (mode_ == Mode::kPinEntry && full_pin_typed) {
    dialog_model()->OnGPMPinEntered(pin_);
  } else if (mode_ == Mode::kPinCreate &&
             full_pin_typed_before != full_pin_typed) {
    dialog_model()->OnButtonsStateChanged();
  }
}

std::u16string AuthenticatorGpmPinSheetModel::GetAccessibleName() const {
  std::u16string pin_digits_typed_str = base::NumberToString16(
      std::min(static_cast<int>(pin_.length()) + 1, pin_digits_count_));
  return l10n_util::GetStringFUTF16(
      IDS_WEBAUTHN_GPM_SIX_DIGIT_PIN_ACCESSIBILITY_LABEL, pin_digits_typed_str);
}

bool AuthenticatorGpmPinSheetModel::FullPinTyped() const {
  return static_cast<int>(pin_.length()) == pin_digits_count_;
}

bool AuthenticatorGpmPinSheetModel::IsAcceptButtonVisible() const {
  return mode_ == Mode::kPinCreate;
}

bool AuthenticatorGpmPinSheetModel::IsAcceptButtonEnabled() const {
  return mode_ == Mode::kPinCreate && FullPinTyped() && !ui_disabled();
}

std::u16string AuthenticatorGpmPinSheetModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CONFIRM);
}

std::u16string AuthenticatorGpmPinSheetModel::GetHint() const {
  return mode_ == Mode::kPinCreate && show_digit_hint_
             ? l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_PIN_DIGIT_HINT)
             : std::u16string();
}

// AuthenticatorGpmArbitraryPinSheetModel --------------------------------------

AuthenticatorGpmArbitraryPinSheetModel::AuthenticatorGpmArbitraryPinSheetModel(
    AuthenticatorRequestDialogModel* dialog_model,
    Mode mode)
    : AuthenticatorGpmPinSheetModelBase(dialog_model, mode) {
  webauthn::user_actions::RecordGpmPinSheetShown(
      /*is_credential_creation=*/dialog_model->request_type ==
          device::FidoRequestType::kMakeCredential,
      /*is_pin_creation=*/mode == Mode::kPinCreate,
      /*is_arbitrary=*/true);
}

AuthenticatorGpmArbitraryPinSheetModel::
    ~AuthenticatorGpmArbitraryPinSheetModel() = default;

void AuthenticatorGpmArbitraryPinSheetModel::SetPin(std::u16string pin) {
  bool accept_button_enabled = IsAcceptButtonEnabled();
  pin_ = std::move(pin);
  if (accept_button_enabled != IsAcceptButtonEnabled()) {
    dialog_model()->OnButtonsStateChanged();
  }
}

std::u16string AuthenticatorGpmArbitraryPinSheetModel::GetAccessibleName()
    const {
  switch (mode_) {
    case Mode::kPinCreate:
      return l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_GPM_CREATE_ALPHANUMERIC_PIN_ACCESSIBILITY);
    case Mode::kPinEntry:
      return l10n_util::GetStringFUTF16(
          IDS_WEBAUTHN_GPM_ENTER_ALPHANUMERIC_PIN_ACCESSIBILITY_WITH_WEBSITE,
          GetRelyingPartyIdString(dialog_model()));
  }
}

bool AuthenticatorGpmArbitraryPinSheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorGpmArbitraryPinSheetModel::IsAcceptButtonEnabled() const {
  return pin_.length() >= kGpmArbitraryPinMinLength && !ui_disabled();
}

std::u16string AuthenticatorGpmArbitraryPinSheetModel::GetAcceptButtonLabel()
    const {
  return mode_ == Mode::kPinEntry
             ? l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_NEXT)
             : l10n_util::GetStringUTF16(IDS_CONFIRM);
}

std::u16string AuthenticatorGpmArbitraryPinSheetModel::GetHint() const {
  return mode_ == Mode::kPinCreate
             ? l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_PIN_LENGTH_HINT)
             : std::u16string();
}

// AuthenticatorTrustThisComputerAssertionSheetModel -------------------------

AuthenticatorTrustThisComputerAssertionSheetModel::
    AuthenticatorTrustThisComputerAssertionSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kHidden) {
  lottie_illustrations_.emplace(IDR_WEBAUTHN_LAPTOP_LIGHT,
                                IDR_WEBAUTHN_LAPTOP_DARK);

  webauthn::user_actions::RecordTrustDialogShown(/*is_create=*/false);
}

AuthenticatorTrustThisComputerAssertionSheetModel::
    ~AuthenticatorTrustThisComputerAssertionSheetModel() = default;

std::u16string AuthenticatorTrustThisComputerAssertionSheetModel::GetStepTitle()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_GPM_TRUST_THIS_COMPUTER_ASSERTION_TITLE);
}

std::u16string
AuthenticatorTrustThisComputerAssertionSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_GPM_TRUST_THIS_COMPUTER_ASSERTION_DESC);
}

bool AuthenticatorTrustThisComputerAssertionSheetModel::IsCancelButtonVisible()
    const {
  return true;
}

std::u16string
AuthenticatorTrustThisComputerAssertionSheetModel::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool AuthenticatorTrustThisComputerAssertionSheetModel::IsAcceptButtonVisible()
    const {
  return true;
}

std::u16string
AuthenticatorTrustThisComputerAssertionSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_NEXT);
}

bool AuthenticatorTrustThisComputerAssertionSheetModel::
    IsOtherMechanismButtonVisible() const {
  return true;
}

std::u16string AuthenticatorTrustThisComputerAssertionSheetModel::
    GetOtherMechanismButtonLabel() const {
  const std::optional<std::string>& phone_name =
      dialog_model()->priority_phone_name;
  if (phone_name) {
    return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_USE_PHONE_WITH_NAME,
                                      base::UTF8ToUTF16(*phone_name));
  }
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_USE_A_DIFFERENT_DEVICE);
}

void AuthenticatorTrustThisComputerAssertionSheetModel::OnBack() {
  dialog_model()->ContactPriorityPhone();
}

void AuthenticatorTrustThisComputerAssertionSheetModel::OnAccept() {
  webauthn::user_actions::RecordAcceptClick();
  dialog_model()->OnTrustThisComputer();
}

// AuthenticatorCreateGpmPasskeySheetModel -------------------------------------

AuthenticatorCreateGpmPasskeySheetModel::
    AuthenticatorCreateGpmPasskeySheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {
  lottie_illustrations_.emplace(IDR_WEBAUTHN_GPM_PASSKEY_LIGHT,
                                IDR_WEBAUTHN_GPM_PASSKEY_DARK);

  webauthn::user_actions::RecordCreateGpmDialogShown();
}

AuthenticatorCreateGpmPasskeySheetModel::
    ~AuthenticatorCreateGpmPasskeySheetModel() = default;

std::u16string AuthenticatorCreateGpmPasskeySheetModel::GetStepTitle() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_GPM_CREATE_PASSKEY_TITLE,
                                    GetRelyingPartyIdString(dialog_model()));
}

std::u16string AuthenticatorCreateGpmPasskeySheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringFUTF16(
      IDS_WEBAUTHN_GPM_CREATE_PASSKEY_DESC,
      base::UTF8ToUTF16(dialog_model()->GetGpmAccountEmail()));
}

bool AuthenticatorCreateGpmPasskeySheetModel::IsCancelButtonVisible() const {
  return true;
}

std::u16string AuthenticatorCreateGpmPasskeySheetModel::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool AuthenticatorCreateGpmPasskeySheetModel::IsAcceptButtonVisible() const {
  return true;
}

std::u16string AuthenticatorCreateGpmPasskeySheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CREATE);
}

void AuthenticatorCreateGpmPasskeySheetModel::OnAccept() {
  webauthn::user_actions::RecordAcceptClick();
  dialog_model()->OnGPMCreatePasskey();
}

// AuthenticatorGpmIncognitoCreateSheetModel ---------------------------------
AuthenticatorGpmIncognitoCreateSheetModel::
    AuthenticatorGpmIncognitoCreateSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kHidden) {
  // Incognito always has a dark color scheme and so the two illustrations are
  // the same.
  lottie_illustrations_.emplace(IDR_WEBAUTHN_GPM_INCOGNITO,
                                IDR_WEBAUTHN_GPM_INCOGNITO);
}

AuthenticatorGpmIncognitoCreateSheetModel::
    ~AuthenticatorGpmIncognitoCreateSheetModel() = default;

std::u16string AuthenticatorGpmIncognitoCreateSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_INCOGNITO_CREATE_TITLE);
}

std::u16string AuthenticatorGpmIncognitoCreateSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_INCOGNITO_CREATE_DESC);
}

bool AuthenticatorGpmIncognitoCreateSheetModel::IsCancelButtonVisible() const {
  return true;
}

std::u16string AuthenticatorGpmIncognitoCreateSheetModel::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool AuthenticatorGpmIncognitoCreateSheetModel::IsAcceptButtonVisible() const {
  return true;
}

std::u16string AuthenticatorGpmIncognitoCreateSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CONTINUE);
}
void AuthenticatorGpmIncognitoCreateSheetModel::OnAccept() {
  dialog_model()->OnGPMConfirmOffTheRecordCreate();
}

// AuthenticatorTrustThisComputerCreationSheetModel ---------------------

AuthenticatorTrustThisComputerCreationSheetModel::
    AuthenticatorTrustThisComputerCreationSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kVisible) {
  lottie_illustrations_.emplace(IDR_WEBAUTHN_LAPTOP_LIGHT,
                                IDR_WEBAUTHN_LAPTOP_DARK);

  webauthn::user_actions::RecordTrustDialogShown(/*is_create=*/true);
}

AuthenticatorTrustThisComputerCreationSheetModel::
    ~AuthenticatorTrustThisComputerCreationSheetModel() = default;

std::u16string AuthenticatorTrustThisComputerCreationSheetModel::GetStepTitle()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_GPM_TRUST_THIS_COMPUTER_CREATION_TITLE);
}

std::u16string
AuthenticatorTrustThisComputerCreationSheetModel::GetStepDescription() const {
  return l10n_util::GetStringFUTF16(
      IDS_WEBAUTHN_GPM_TRUST_THIS_COMPUTER_CREATION_DESC,
      base::UTF8ToUTF16(dialog_model()->GetGpmAccountEmail()));
}

bool AuthenticatorTrustThisComputerCreationSheetModel::IsCancelButtonVisible()
    const {
  return true;
}

std::u16string
AuthenticatorTrustThisComputerCreationSheetModel::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool AuthenticatorTrustThisComputerCreationSheetModel::IsAcceptButtonVisible()
    const {
  return true;
}

std::u16string
AuthenticatorTrustThisComputerCreationSheetModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_NEXT);
}

std::u16string
AuthenticatorTrustThisComputerCreationSheetModel::GetOtherMechanismButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_SAVE_ANOTHER_WAY);
}

void AuthenticatorTrustThisComputerCreationSheetModel::OnAccept() {
  webauthn::user_actions::RecordAcceptClick();
  dialog_model()->OnTrustThisComputer();
}

// AuthenticatorGPMLockedPinSheetModel ----------------------------------

AuthenticatorGPMLockedPinSheetModel::AuthenticatorGPMLockedPinSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model,
                                  OtherMechanismButtonVisibility::kHidden) {
  lottie_illustrations_.emplace(IDR_WEBAUTHN_GPM_PIN_LOCKED_LIGHT,
                                IDR_WEBAUTHN_GPM_PIN_LOCKED_DARK);
  webauthn::user_actions::RecordGpmLockedShown();
}

AuthenticatorGPMLockedPinSheetModel::~AuthenticatorGPMLockedPinSheetModel() =
    default;

std::u16string AuthenticatorGPMLockedPinSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_LOCKED_GPM_PIN_TITLE);
}

std::u16string AuthenticatorGPMLockedPinSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_LOCKED_GPM_PIN_DESCRIPTION);
}

bool AuthenticatorGPMLockedPinSheetModel::IsAcceptButtonEnabled() const {
  return true;
}

bool AuthenticatorGPMLockedPinSheetModel::IsAcceptButtonVisible() const {
  return true;
}

std::u16string AuthenticatorGPMLockedPinSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CHANGE_PIN);
}

void AuthenticatorGPMLockedPinSheetModel::OnAccept() {
  webauthn::user_actions::RecordAcceptClick();
  dialog_model()->OnForgotGPMPinPressed();
}
