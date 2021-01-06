// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/sheet_models.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/webauthn/other_transports_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_utils.h"
#include "url/gurl.h"

namespace {

base::string16 GetRelyingPartyIdString(
    AuthenticatorRequestDialogModel* dialog_model) {
  static constexpr char kRpIdUrlPrefix[] = "https://";
  // The preferred width of medium snap point modal dialog view is 448 dp, but
  // we leave some room for padding between the text and the modal views.
  static constexpr int kDialogWidth = 300;
  const auto& rp_id = dialog_model->relying_party_id();
  DCHECK(!rp_id.empty());
  GURL rp_id_url(kRpIdUrlPrefix + rp_id);
  return url_formatter::ElideHost(rp_id_url, gfx::FontList(), kDialogWidth);
}

// Possibly returns a resident key warning if the model indicates that it's
// needed.
base::string16 PossibleResidentKeyWarning(
    AuthenticatorRequestDialogModel* dialog_model) {
  if (dialog_model->might_create_resident_credential()) {
    return l10n_util::GetStringUTF16(IDS_WEBAUTHN_RESIDENT_KEY_PRIVACY);
  }
  return base::string16();
}

}  // namespace

// AuthenticatorSheetModelBase ------------------------------------------------

AuthenticatorSheetModelBase::AuthenticatorSheetModelBase(
    AuthenticatorRequestDialogModel* dialog_model)
    : dialog_model_(dialog_model) {
  DCHECK(dialog_model);
  dialog_model_->AddObserver(this);
}

AuthenticatorSheetModelBase::~AuthenticatorSheetModelBase() {
  if (dialog_model_) {
    dialog_model_->RemoveObserver(this);
    dialog_model_ = nullptr;
  }
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

base::string16 AuthenticatorSheetModelBase::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool AuthenticatorSheetModelBase::IsAcceptButtonVisible() const {
  return false;
}

bool AuthenticatorSheetModelBase::IsAcceptButtonEnabled() const {
  return false;
}

base::string16 AuthenticatorSheetModelBase::GetAcceptButtonLabel() const {
  return base::string16();
}

void AuthenticatorSheetModelBase::OnBack() {
  if (dialog_model())
    dialog_model()->StartOver();
}

void AuthenticatorSheetModelBase::OnAccept() {
  NOTREACHED();
}

void AuthenticatorSheetModelBase::OnCancel() {
  if (dialog_model())
    dialog_model()->Cancel();
}

void AuthenticatorSheetModelBase::OnModelDestroyed() {
  dialog_model_ = nullptr;
}

// AuthenticatorTransportSelectorSheetModel -----------------------------------

bool AuthenticatorTransportSelectorSheetModel::IsBackButtonVisible() const {
  return false;
}

const gfx::VectorIcon&
AuthenticatorTransportSelectorSheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnWelcomeDarkIcon
                                                 : kWebauthnWelcomeIcon;
}

base::string16 AuthenticatorTransportSelectorSheetModel::GetStepTitle() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_TRANSPORT_SELECTION_TITLE,
                                    GetRelyingPartyIdString(dialog_model()));
}

base::string16 AuthenticatorTransportSelectorSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_TRANSPORT_SELECTION_DESCRIPTION);
}

void AuthenticatorTransportSelectorSheetModel::OnTransportSelected(
    AuthenticatorTransport transport) {
  dialog_model()->StartGuidedFlowForTransport(transport);
}

void AuthenticatorTransportSelectorSheetModel::StartWinNativeApi() {
  dialog_model()->StartWinNativeApi();
}

// AuthenticatorInsertAndActivateUsbSheetModel ----------------------

AuthenticatorInsertAndActivateUsbSheetModel::
    AuthenticatorInsertAndActivateUsbSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model),
      other_transports_menu_model_(std::make_unique<OtherTransportsMenuModel>(
          dialog_model,
          AuthenticatorTransport::kUsbHumanInterfaceDevice)) {}

AuthenticatorInsertAndActivateUsbSheetModel::
    ~AuthenticatorInsertAndActivateUsbSheetModel() = default;

bool AuthenticatorInsertAndActivateUsbSheetModel::IsActivityIndicatorVisible()
    const {
  return true;
}

const gfx::VectorIcon&
AuthenticatorInsertAndActivateUsbSheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnUsbDarkIcon
                                                 : kWebauthnUsbIcon;
}

base::string16 AuthenticatorInsertAndActivateUsbSheetModel::GetStepTitle()
    const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_GENERIC_TITLE,
                                    GetRelyingPartyIdString(dialog_model()));
}

base::string16 AuthenticatorInsertAndActivateUsbSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_USB_ACTIVATE_DESCRIPTION);
}

base::string16
AuthenticatorInsertAndActivateUsbSheetModel::GetAdditionalDescription() const {
  return PossibleResidentKeyWarning(dialog_model());
}

ui::MenuModel*
AuthenticatorInsertAndActivateUsbSheetModel::GetOtherTransportsMenuModel() {
  return other_transports_menu_model_.get();
}

// AuthenticatorTimeoutErrorModel ---------------------------------------------

bool AuthenticatorTimeoutErrorModel::IsBackButtonVisible() const {
  return false;
}

base::string16 AuthenticatorTimeoutErrorModel::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

const gfx::VectorIcon& AuthenticatorTimeoutErrorModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnErrorDarkIcon
                                                 : kWebauthnErrorIcon;
}

base::string16 AuthenticatorTimeoutErrorModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_GENERIC_TITLE);
}

base::string16 AuthenticatorTimeoutErrorModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_TIMEOUT_DESCRIPTION);
}

// AuthenticatorNoAvailableTransportsErrorModel -------------------------------

bool AuthenticatorNoAvailableTransportsErrorModel::IsBackButtonVisible() const {
  return false;
}

base::string16
AuthenticatorNoAvailableTransportsErrorModel::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

const gfx::VectorIcon&
AuthenticatorNoAvailableTransportsErrorModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnErrorDarkIcon
                                                 : kWebauthnErrorIcon;
}

base::string16 AuthenticatorNoAvailableTransportsErrorModel::GetStepTitle()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_NO_TRANSPORTS_TITLE);
}

base::string16
AuthenticatorNoAvailableTransportsErrorModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_ERROR_NO_TRANSPORTS_DESCRIPTION);
}

// AuthenticatorNotRegisteredErrorModel ---------------------------------------

bool AuthenticatorNotRegisteredErrorModel::IsBackButtonVisible() const {
  return false;
}

base::string16 AuthenticatorNotRegisteredErrorModel::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

bool AuthenticatorNotRegisteredErrorModel::IsAcceptButtonVisible() const {
  return dialog_model()->offer_try_again_in_ui();
}

bool AuthenticatorNotRegisteredErrorModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorNotRegisteredErrorModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_RETRY);
}

const gfx::VectorIcon&
AuthenticatorNotRegisteredErrorModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnErrorDarkIcon
                                                 : kWebauthnErrorIcon;
}

base::string16 AuthenticatorNotRegisteredErrorModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_WRONG_KEY_TITLE);
}

base::string16 AuthenticatorNotRegisteredErrorModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_ERROR_WRONG_KEY_SIGN_DESCRIPTION);
}

void AuthenticatorNotRegisteredErrorModel::OnAccept() {
  dialog_model()->StartOver();
}

// AuthenticatorAlreadyRegisteredErrorModel -----------------------------------

bool AuthenticatorAlreadyRegisteredErrorModel::IsBackButtonVisible() const {
  return false;
}

base::string16 AuthenticatorAlreadyRegisteredErrorModel::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

bool AuthenticatorAlreadyRegisteredErrorModel::IsAcceptButtonVisible() const {
  return dialog_model()->offer_try_again_in_ui();
}

bool AuthenticatorAlreadyRegisteredErrorModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorAlreadyRegisteredErrorModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_RETRY);
}

const gfx::VectorIcon&
AuthenticatorAlreadyRegisteredErrorModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnErrorDarkIcon
                                                 : kWebauthnErrorIcon;
}

base::string16 AuthenticatorAlreadyRegisteredErrorModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_WRONG_KEY_TITLE);
}

base::string16 AuthenticatorAlreadyRegisteredErrorModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_ERROR_WRONG_KEY_REGISTER_DESCRIPTION);
}

void AuthenticatorAlreadyRegisteredErrorModel::OnAccept() {
  dialog_model()->StartOver();
}

// AuthenticatorInternalUnrecognizedErrorSheetModel
// -----------------------------------
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

base::string16
AuthenticatorInternalUnrecognizedErrorSheetModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_RETRY);
}

const gfx::VectorIcon&
AuthenticatorInternalUnrecognizedErrorSheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnErrorDarkIcon
                                                 : kWebauthnErrorIcon;
}

base::string16 AuthenticatorInternalUnrecognizedErrorSheetModel::GetStepTitle()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_ERROR_INTERNAL_UNRECOGNIZED_TITLE);
}

base::string16
AuthenticatorInternalUnrecognizedErrorSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_ERROR_INTERNAL_UNRECOGNIZED_DESCRIPTION);
}

void AuthenticatorInternalUnrecognizedErrorSheetModel::OnAccept() {
  dialog_model()->StartOver();
}

// AuthenticatorBlePowerOnManualSheetModel ------------------------------------

const gfx::VectorIcon&
AuthenticatorBlePowerOnManualSheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark
             ? kWebauthnErrorBluetoothDarkIcon
             : kWebauthnErrorBluetoothIcon;
}

base::string16 AuthenticatorBlePowerOnManualSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_BLUETOOTH_POWER_ON_MANUAL_TITLE);
}

base::string16 AuthenticatorBlePowerOnManualSheetModel::GetStepDescription()
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

base::string16 AuthenticatorBlePowerOnManualSheetModel::GetAcceptButtonLabel()
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

bool AuthenticatorBlePowerOnAutomaticSheetModel::IsActivityIndicatorVisible()
    const {
  return busy_powering_on_ble_;
}

const gfx::VectorIcon&
AuthenticatorBlePowerOnAutomaticSheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark
             ? kWebauthnErrorBluetoothDarkIcon
             : kWebauthnErrorBluetoothIcon;
}

base::string16 AuthenticatorBlePowerOnAutomaticSheetModel::GetStepTitle()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLUETOOTH_POWER_ON_AUTO_TITLE);
}

base::string16 AuthenticatorBlePowerOnAutomaticSheetModel::GetStepDescription()
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

base::string16
AuthenticatorBlePowerOnAutomaticSheetModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLUETOOTH_POWER_ON_AUTO_NEXT);
}

void AuthenticatorBlePowerOnAutomaticSheetModel::OnAccept() {
  busy_powering_on_ble_ = true;
  dialog_model()->OnSheetModelDidChange();
  dialog_model()->PowerOnBleAdapter();
}

// AuthenticatorOffTheRecordInterstitialSheetModel
// -----------------------------------------

AuthenticatorOffTheRecordInterstitialSheetModel::
    AuthenticatorOffTheRecordInterstitialSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model),
      other_transports_menu_model_(std::make_unique<OtherTransportsMenuModel>(
          dialog_model,
          AuthenticatorTransport::kInternal)) {}

AuthenticatorOffTheRecordInterstitialSheetModel::
    ~AuthenticatorOffTheRecordInterstitialSheetModel() = default;

const gfx::VectorIcon&
AuthenticatorOffTheRecordInterstitialSheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnPermissionDarkIcon
                                                 : kWebauthnPermissionIcon;
}

base::string16 AuthenticatorOffTheRecordInterstitialSheetModel::GetStepTitle()
    const {
#if defined(OS_MAC)
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_TOUCH_ID_INCOGNITO_BUMP_TITLE,
                                    GetRelyingPartyIdString(dialog_model()));
#else
  return base::string16();
#endif  // defined(OS_MAC)
}

base::string16
AuthenticatorOffTheRecordInterstitialSheetModel::GetStepDescription() const {
#if defined(OS_MAC)
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_TOUCH_ID_INCOGNITO_BUMP_DESCRIPTION);
#else
  return base::string16();
#endif  // defined(OS_MAC)
}

ui::MenuModel*
AuthenticatorOffTheRecordInterstitialSheetModel::GetOtherTransportsMenuModel() {
  return other_transports_menu_model_.get();
}

bool AuthenticatorOffTheRecordInterstitialSheetModel::IsAcceptButtonVisible()
    const {
  return true;
}

bool AuthenticatorOffTheRecordInterstitialSheetModel::IsAcceptButtonEnabled()
    const {
  return true;
}

base::string16
AuthenticatorOffTheRecordInterstitialSheetModel::GetAcceptButtonLabel() const {
#if defined(OS_MAC)
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_TOUCH_ID_INCOGNITO_BUMP_CONTINUE);
#else
  return base::string16();
#endif  // defined(OS_MAC)
}

void AuthenticatorOffTheRecordInterstitialSheetModel::OnAccept() {
  dialog_model()->HideDialogAndDispatchToPlatformAuthenticator();
}

// AuthenticatorPaaskSheetModel -----------------------------------------

AuthenticatorPaaskSheetModel::AuthenticatorPaaskSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model),
      other_transports_menu_model_(std::make_unique<OtherTransportsMenuModel>(
          dialog_model,
          AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy)) {}

AuthenticatorPaaskSheetModel::~AuthenticatorPaaskSheetModel() = default;

bool AuthenticatorPaaskSheetModel::IsBackButtonVisible() const {
#if defined(OS_WIN)
  return !base::FeatureList::IsEnabled(device::kWebAuthUseNativeWinApi);
#else
  return true;
#endif
}

bool AuthenticatorPaaskSheetModel::IsActivityIndicatorVisible() const {
  return true;
}

const gfx::VectorIcon& AuthenticatorPaaskSheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnPhoneDarkIcon
                                                 : kWebauthnPhoneIcon;
}

base::string16 AuthenticatorPaaskSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLE_ACTIVATE_TITLE);
}

base::string16 AuthenticatorPaaskSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLE_ACTIVATE_DESCRIPTION);
}

ui::MenuModel* AuthenticatorPaaskSheetModel::GetOtherTransportsMenuModel() {
  return other_transports_menu_model_.get();
}

// AuthenticatorPaaskV2SheetModel  -----------------------------------------

AuthenticatorPaaskV2SheetModel::AuthenticatorPaaskV2SheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model),
      other_transports_menu_model_(std::make_unique<OtherTransportsMenuModel>(
          dialog_model,
          AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy)) {}

AuthenticatorPaaskV2SheetModel::~AuthenticatorPaaskV2SheetModel() = default;

bool AuthenticatorPaaskV2SheetModel::IsBackButtonVisible() const {
#if defined(OS_WIN)
  return !base::FeatureList::IsEnabled(device::kWebAuthUseNativeWinApi);
#else
  return true;
#endif
}

bool AuthenticatorPaaskV2SheetModel::IsActivityIndicatorVisible() const {
  return true;
}

const gfx::VectorIcon& AuthenticatorPaaskV2SheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnPhoneDarkIcon
                                                 : kWebauthnPhoneIcon;
}

bool AuthenticatorPaaskV2SheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorPaaskV2SheetModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorPaaskV2SheetModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLE_QR_TITLE);
}

void AuthenticatorPaaskV2SheetModel::OnAccept() {
  return dialog_model()->StartPhonePairing();
}

base::string16 AuthenticatorPaaskV2SheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLE_V2_ACTIVATE_TITLE);
}

base::string16 AuthenticatorPaaskV2SheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_CABLE_V2_ACTIVATE_DESCRIPTION_SHORT);
}

ui::MenuModel* AuthenticatorPaaskV2SheetModel::GetOtherTransportsMenuModel() {
  return other_transports_menu_model_.get();
}

// AuthenticatorClientPinEntrySheetModel
// -----------------------------------------

AuthenticatorClientPinEntrySheetModel::AuthenticatorClientPinEntrySheetModel(
    AuthenticatorRequestDialogModel* dialog_model,
    Mode mode,
    device::pin::PINEntryError error)
    : AuthenticatorSheetModelBase(dialog_model), mode_(mode) {
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
      base::Optional<int> attempts = dialog_model->pin_attempts();
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
    base::string16 pin_code) {
  pin_code_ = std::move(pin_code);
}

void AuthenticatorClientPinEntrySheetModel::SetPinConfirmation(
    base::string16 pin_confirmation) {
  DCHECK(mode_ == Mode::kPinSetup || mode_ == Mode::kPinChange);
  pin_confirmation_ = std::move(pin_confirmation);
}

const gfx::VectorIcon&
AuthenticatorClientPinEntrySheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnUsbDarkIcon
                                                 : kWebauthnUsbIcon;
}

base::string16 AuthenticatorClientPinEntrySheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_TITLE);
}

base::string16 AuthenticatorClientPinEntrySheetModel::GetStepDescription()
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

base::string16 AuthenticatorClientPinEntrySheetModel::GetError() const {
  return error_;
}

bool AuthenticatorClientPinEntrySheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorClientPinEntrySheetModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorClientPinEntrySheetModel::GetAcceptButtonLabel()
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
    : AuthenticatorSheetModelBase(dialog_model) {}

AuthenticatorClientPinTapAgainSheetModel::
    ~AuthenticatorClientPinTapAgainSheetModel() = default;

bool AuthenticatorClientPinTapAgainSheetModel::IsActivityIndicatorVisible()
    const {
  return true;
}

const gfx::VectorIcon&
AuthenticatorClientPinTapAgainSheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnUsbDarkIcon
                                                 : kWebauthnUsbIcon;
}

base::string16 AuthenticatorClientPinTapAgainSheetModel::GetStepTitle() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_GENERIC_TITLE,
                                    GetRelyingPartyIdString(dialog_model()));
}

base::string16 AuthenticatorClientPinTapAgainSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_TAP_AGAIN_DESCRIPTION);
}

base::string16
AuthenticatorClientPinTapAgainSheetModel::GetAdditionalDescription() const {
  return PossibleResidentKeyWarning(dialog_model());
}

// AuthenticatorBioEnrollmentSheetModel ----------------------------------

AuthenticatorBioEnrollmentSheetModel::AuthenticatorBioEnrollmentSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {}

AuthenticatorBioEnrollmentSheetModel::~AuthenticatorBioEnrollmentSheetModel() =
    default;

bool AuthenticatorBioEnrollmentSheetModel::IsActivityIndicatorVisible() const {
  return !IsAcceptButtonVisible();
}

const gfx::VectorIcon&
AuthenticatorBioEnrollmentSheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnFingerprintDarkIcon
                                                 : kWebauthnFingerprintIcon;
}

base::string16 AuthenticatorBioEnrollmentSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_SETTINGS_SECURITY_KEYS_BIO_ENROLLMENT_ADD_TITLE);
}

base::string16 AuthenticatorBioEnrollmentSheetModel::GetStepDescription()
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

base::string16 AuthenticatorBioEnrollmentSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_NEXT);
}

bool AuthenticatorBioEnrollmentSheetModel::IsCancelButtonVisible() const {
  return !IsAcceptButtonVisible();
}

base::string16 AuthenticatorBioEnrollmentSheetModel::GetCancelButtonLabel()
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
    : AuthenticatorSheetModelBase(dialog_model) {}

AuthenticatorRetryUvSheetModel::~AuthenticatorRetryUvSheetModel() = default;

bool AuthenticatorRetryUvSheetModel::IsActivityIndicatorVisible() const {
  return true;
}

const gfx::VectorIcon& AuthenticatorRetryUvSheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnFingerprintDarkIcon
                                                 : kWebauthnFingerprintIcon;
}

base::string16 AuthenticatorRetryUvSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_UV_RETRY_TITLE);
}

base::string16 AuthenticatorRetryUvSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_UV_RETRY_DESCRIPTION);
}

base::string16 AuthenticatorRetryUvSheetModel::GetError() const {
  int attempts = *dialog_model()->uv_attempts();
  if (attempts > 3) {
    return base::string16();
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

AuthenticatorGenericErrorSheetModel::AuthenticatorGenericErrorSheetModel(
    AuthenticatorRequestDialogModel* dialog_model,
    base::string16 title,
    base::string16 description)
    : AuthenticatorSheetModelBase(dialog_model),
      title_(std::move(title)),
      description_(std::move(description)) {}

bool AuthenticatorGenericErrorSheetModel::IsBackButtonVisible() const {
  return false;
}

base::string16 AuthenticatorGenericErrorSheetModel::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

bool AuthenticatorGenericErrorSheetModel::IsAcceptButtonVisible() const {
  return dialog_model()->offer_try_again_in_ui();
}

bool AuthenticatorGenericErrorSheetModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorGenericErrorSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_RETRY);
}

const gfx::VectorIcon& AuthenticatorGenericErrorSheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnErrorDarkIcon
                                                 : kWebauthnErrorIcon;
}

base::string16 AuthenticatorGenericErrorSheetModel::GetStepTitle() const {
  return title_;
}

base::string16 AuthenticatorGenericErrorSheetModel::GetStepDescription() const {
  return description_;
}

void AuthenticatorGenericErrorSheetModel::OnAccept() {
  dialog_model()->StartOver();
}

// AuthenticatorResidentCredentialConfirmationSheetView -----------------------

AuthenticatorResidentCredentialConfirmationSheetView::
    AuthenticatorResidentCredentialConfirmationSheetView(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {}

AuthenticatorResidentCredentialConfirmationSheetView::
    ~AuthenticatorResidentCredentialConfirmationSheetView() = default;

const gfx::VectorIcon&
AuthenticatorResidentCredentialConfirmationSheetView::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnPermissionDarkIcon
                                                 : kWebauthnPermissionIcon;
}

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

base::string16
AuthenticatorResidentCredentialConfirmationSheetView::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_WELCOME_SCREEN_NEXT);
}

base::string16
AuthenticatorResidentCredentialConfirmationSheetView::GetStepTitle() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_GENERIC_TITLE,
                                    GetRelyingPartyIdString(dialog_model()));
}

base::string16
AuthenticatorResidentCredentialConfirmationSheetView::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_RESIDENT_KEY_PRIVACY);
}

void AuthenticatorResidentCredentialConfirmationSheetView::OnAccept() {
  dialog_model()->OnResidentCredentialConfirmed();
}

// AuthenticatorSelectAccountSheetModel ---------------------------------------

AuthenticatorSelectAccountSheetModel::AuthenticatorSelectAccountSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {}

AuthenticatorSelectAccountSheetModel::~AuthenticatorSelectAccountSheetModel() =
    default;

void AuthenticatorSelectAccountSheetModel::SetCurrentSelection(int selected) {
  DCHECK_LE(0, selected);
  DCHECK_LT(static_cast<size_t>(selected), dialog_model()->responses().size());
  selected_ = selected;
}

void AuthenticatorSelectAccountSheetModel::OnAccept() {
  dialog_model()->OnAccountSelected(selected_);
}

const gfx::VectorIcon&
AuthenticatorSelectAccountSheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  // TODO: this is likely the wrong image.
  return color_scheme == ImageColorScheme::kDark ? kWebauthnWelcomeDarkIcon
                                                 : kWebauthnWelcomeIcon;
}

base::string16 AuthenticatorSelectAccountSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_SELECT_ACCOUNT);
}

base::string16 AuthenticatorSelectAccountSheetModel::GetStepDescription()
    const {
  return base::string16();
}

bool AuthenticatorSelectAccountSheetModel::IsAcceptButtonVisible() const {
  return false;
}

bool AuthenticatorSelectAccountSheetModel::IsAcceptButtonEnabled() const {
  return false;
}

base::string16 AuthenticatorSelectAccountSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_WELCOME_SCREEN_NEXT);
}

// AttestationPermissionRequestSheetModel -------------------------------------

AttestationPermissionRequestSheetModel::AttestationPermissionRequestSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {}

AttestationPermissionRequestSheetModel::
    ~AttestationPermissionRequestSheetModel() = default;

void AttestationPermissionRequestSheetModel::OnAccept() {
  dialog_model()->OnAttestationPermissionResponse(true);
}

void AttestationPermissionRequestSheetModel::OnCancel() {
  dialog_model()->OnAttestationPermissionResponse(false);
}

const gfx::VectorIcon&
AttestationPermissionRequestSheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnPermissionDarkIcon
                                                 : kWebauthnPermissionIcon;
}

base::string16 AttestationPermissionRequestSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_REQUEST_ATTESTATION_PERMISSION_TITLE);
}

base::string16 AttestationPermissionRequestSheetModel::GetStepDescription()
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

base::string16 AttestationPermissionRequestSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ALLOW_ATTESTATION);
}

bool AttestationPermissionRequestSheetModel::IsCancelButtonVisible() const {
  return true;
}

base::string16 AttestationPermissionRequestSheetModel::GetCancelButtonLabel()
    const {
  // TODO(martinkr): This should be its own string definition; but we had to
  // make a change post string freeze and therefore reused this.
  return l10n_util::GetStringUTF16(IDS_PERMISSION_DENY);
}

// AuthenticatorQRSheetModel --------------------------------------------------

AuthenticatorQRSheetModel::AuthenticatorQRSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {}

AuthenticatorQRSheetModel::~AuthenticatorQRSheetModel() = default;

const gfx::VectorIcon& AuthenticatorQRSheetModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  return color_scheme == ImageColorScheme::kDark ? kWebauthnPhoneDarkIcon
                                                 : kWebauthnPhoneIcon;
}

base::string16 AuthenticatorQRSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLE_QR_TITLE);
}

base::string16 AuthenticatorQRSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLE_QR_DESCRIPTION);
}
