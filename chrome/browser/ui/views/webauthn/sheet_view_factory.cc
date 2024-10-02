// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/sheet_view_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_model.h"
#include "chrome/browser/ui/views/webauthn/authenticator_bio_enrollment_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_client_pin_entry_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_create_user_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_gpm_arbitrary_pin_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_gpm_pin_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_multi_source_picker_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_priority_mechanism_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_qr_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_select_account_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/hover_list_view.h"
#include "chrome/browser/ui/views/webauthn/passkey_detail_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/browser/ui/webauthn/transport_hover_list_model.h"
#include "chrome/browser/ui/webauthn/user_actions.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "device/fido/features.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/webauthn/authenticator_touch_id_view.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

// Number of digits for the GPM Pin.
constexpr int kPinDigitCount = 6;

// A placeholder sheet to show in place of unimplemented sheets.
class PlaceholderSheetModel : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  std::u16string GetStepTitle() const override { return std::u16string(); }
  std::u16string GetStepDescription() const override {
    return std::u16string();
  }
};

// Represents a sheet in the Web Authentication request dialog that allows the
// user to pick the mechanism (i.e. USB / Windows API / phone / etc) to use.
class AuthenticatorMechanismSelectorSheetView
    : public AuthenticatorRequestSheetView {
 public:
  explicit AuthenticatorMechanismSelectorSheetView(
      std::unique_ptr<AuthenticatorMechanismSelectorSheetModel> model)
      : AuthenticatorRequestSheetView(std::move(model)) {}

  AuthenticatorMechanismSelectorSheetView(
      const AuthenticatorMechanismSelectorSheetView&) = delete;
  AuthenticatorMechanismSelectorSheetView& operator=(
      const AuthenticatorMechanismSelectorSheetView&) = delete;

 private:
  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>,
            AuthenticatorRequestSheetView::AutoFocus>
  BuildStepSpecificContent() override {
    auto* model = static_cast<AuthenticatorMechanismSelectorSheetModel*>(
        AuthenticatorRequestSheetView::model());
    return std::make_pair(
        std::make_unique<HoverListView>(
            std::make_unique<TransportHoverListModel>(model->dialog_model())),
        AutoFocus::kYes);
  }
};

class AuthenticatorCreatePasskeySheetView
    : public AuthenticatorRequestSheetView {
 public:
  explicit AuthenticatorCreatePasskeySheetView(
      std::unique_ptr<AuthenticatorCreatePasskeySheetModel> model)
      : AuthenticatorRequestSheetView(std::move(model)) {}

  AuthenticatorCreatePasskeySheetView(
      const AuthenticatorCreatePasskeySheetView&) = delete;
  AuthenticatorCreatePasskeySheetView& operator=(
      const AuthenticatorCreatePasskeySheetView&) = delete;

 private:
  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>,
            AuthenticatorRequestSheetView::AutoFocus>
  BuildStepSpecificContent() override {
    auto container = std::make_unique<views::BoxLayoutView>();
    container->SetOrientation(views::BoxLayout::Orientation::kVertical);
    container->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);
    container->SetBetweenChildSpacing(12);
    container->AddChildView(std::make_unique<PasskeyDetailView>(
        static_cast<AuthenticatorCreatePasskeySheetModel*>(model())
            ->dialog_model()
            ->user_entity));
    auto* label = container->AddChildView(std::make_unique<views::Label>(
        static_cast<AuthenticatorCreatePasskeySheetModel*>(model())
            ->passkey_storage_description(),
        views::style::CONTEXT_DIALOG_BODY_TEXT));
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

    return std::make_pair(std::move(container), AutoFocus::kNo);
  }
};

}  // namespace

std::unique_ptr<AuthenticatorRequestSheetView> CreateSheetViewForCurrentStepOf(
    AuthenticatorRequestDialogModel* dialog_model) {
  using Step = AuthenticatorRequestDialogModel::Step;

  std::unique_ptr<AuthenticatorRequestSheetView> sheet_view;
  switch (dialog_model->step()) {
    case Step::kMechanismSelection:
      if (dialog_model->request_type ==
          device::FidoRequestType::kGetAssertion) {
        sheet_view = std::make_unique<AuthenticatorMultiSourcePickerSheetView>(
            std::make_unique<AuthenticatorMultiSourcePickerSheetModel>(
                dialog_model));
      } else {
        sheet_view = std::make_unique<AuthenticatorMechanismSelectorSheetView>(
            std::make_unique<AuthenticatorMechanismSelectorSheetModel>(
                dialog_model));
      }
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
    case Step::kErrorNoPasskeys:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorNoPasskeysErrorModel>(dialog_model));
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
    case Step::kErrorWindowsHelloNotEnabled:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          AuthenticatorGenericErrorSheetModel::ForWindowsHelloNotEnabled(
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
    case Step::kBlePermissionMac:
#if BUILDFLAG(IS_MAC)
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorBlePermissionMacSheetModel>(
              dialog_model));
      break;
#else
      NOTREACHED();
#endif
    case Step::kOffTheRecordInterstitial:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorOffTheRecordInterstitialSheetModel>(
              dialog_model));
      break;
    case Step::kPhoneConfirmationSheet:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorPhoneConfirmationSheet>(dialog_model));
      break;
    case Step::kCableActivate:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorPaaskSheetModel>(dialog_model));
      break;
    case Step::kCableV2QRCode:
      sheet_view = std::make_unique<AuthenticatorQRSheetView>(
          std::make_unique<AuthenticatorQRSheetModel>(dialog_model));
      break;
    case Step::kCableV2Connecting:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorConnectingSheetModel>(dialog_model));
      break;
    case Step::kCableV2Connected:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorConnectedSheetModel>(dialog_model));
      break;
    case Step::kCableV2Error:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorCableErrorSheetModel>(dialog_model));
      break;
    case Step::kClientPinChange:
      sheet_view = std::make_unique<AuthenticatorClientPinEntrySheetView>(
          std::make_unique<AuthenticatorClientPinEntrySheetModel>(
              dialog_model,
              AuthenticatorClientPinEntrySheetModel::Mode::kPinChange,
              dialog_model->pin_error));
      break;
    case Step::kClientPinEntry:
      sheet_view = std::make_unique<AuthenticatorClientPinEntrySheetView>(
          std::make_unique<AuthenticatorClientPinEntrySheetModel>(
              dialog_model,
              AuthenticatorClientPinEntrySheetModel::Mode::kPinEntry,
              dialog_model->pin_error));
      break;
    case Step::kClientPinSetup:
      sheet_view = std::make_unique<AuthenticatorClientPinEntrySheetView>(
          std::make_unique<AuthenticatorClientPinEntrySheetModel>(
              dialog_model,
              AuthenticatorClientPinEntrySheetModel::Mode::kPinSetup,
              dialog_model->pin_error));
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
    case Step::kInlineBioEnrollment:
      sheet_view = std::make_unique<AuthenticatorBioEnrollmentSheetView>(
          std::make_unique<AuthenticatorBioEnrollmentSheetModel>(dialog_model));
      break;
    case Step::kRetryInternalUserVerification:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorRetryUvSheetModel>(dialog_model));
      break;
    case Step::kResidentCredentialConfirmation:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<
              AuthenticatorResidentCredentialConfirmationSheetView>(
              dialog_model));
      break;
    case Step::kSelectAccount:
      sheet_view = std::make_unique<AuthenticatorSelectAccountSheetView>(
          std::make_unique<AuthenticatorSelectAccountSheetModel>(
              dialog_model,
              AuthenticatorSelectAccountSheetModel::kPostUserVerification,
              AuthenticatorSelectAccountSheetModel::kMultipleAccounts));
      break;
    case Step::kSelectSingleAccount:
      sheet_view = std::make_unique<AuthenticatorSelectAccountSheetView>(
          std::make_unique<AuthenticatorSelectAccountSheetModel>(
              dialog_model,
              AuthenticatorSelectAccountSheetModel::kPostUserVerification,
              AuthenticatorSelectAccountSheetModel::kSingleAccount));
      break;
    case Step::kPreSelectAccount:
      sheet_view = std::make_unique<AuthenticatorSelectAccountSheetView>(
          std::make_unique<AuthenticatorSelectAccountSheetModel>(
              dialog_model,
              AuthenticatorSelectAccountSheetModel::kPreUserVerification,
              AuthenticatorSelectAccountSheetModel::kMultipleAccounts));
      break;
    case Step::kPreSelectSingleAccount:
      sheet_view = std::make_unique<AuthenticatorSelectAccountSheetView>(
          std::make_unique<AuthenticatorSelectAccountSheetModel>(
              dialog_model,
              AuthenticatorSelectAccountSheetModel::kPreUserVerification,
              AuthenticatorSelectAccountSheetModel::kSingleAccount));
      break;
    case Step::kSelectPriorityMechanism:
      sheet_view = std::make_unique<AuthenticatorPriorityMechanismSheetView>(
          std::make_unique<AuthenticatorPriorityMechanismSheetModel>(
              dialog_model));
      break;
    case Step::kCreatePasskey:
      sheet_view = std::make_unique<AuthenticatorCreatePasskeySheetView>(
          std::make_unique<AuthenticatorCreatePasskeySheetModel>(dialog_model));
      break;
    case Step::kGPMError:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorGPMErrorSheetModel>(dialog_model));
      break;
    case Step::kGPMConnecting:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorGPMConnectingSheetModel>(dialog_model));
      break;
    case Step::kGPMChangePin:
    case Step::kGPMCreatePin:
      sheet_view = std::make_unique<AuthenticatorGpmPinSheetView>(
          std::make_unique<AuthenticatorGpmPinSheetModel>(
              dialog_model, kPinDigitCount,
              AuthenticatorGpmPinSheetModelBase::Mode::kPinCreate));
      break;
    case Step::kGPMEnterPin:
      sheet_view = std::make_unique<AuthenticatorGpmPinSheetView>(
          std::make_unique<AuthenticatorGpmPinSheetModel>(
              dialog_model, kPinDigitCount,
              AuthenticatorGpmPinSheetModelBase::Mode::kPinEntry));
      break;
    case Step::kGPMChangeArbitraryPin:
    case Step::kGPMCreateArbitraryPin:
      sheet_view = std::make_unique<AuthenticatorGPMArbitraryPinSheetView>(
          std::make_unique<AuthenticatorGpmArbitraryPinSheetModel>(
              dialog_model,
              AuthenticatorGpmPinSheetModelBase::Mode::kPinCreate));
      break;
    case Step::kGPMEnterArbitraryPin:
      sheet_view = std::make_unique<AuthenticatorGPMArbitraryPinSheetView>(
          std::make_unique<AuthenticatorGpmArbitraryPinSheetModel>(
              dialog_model,
              AuthenticatorGpmPinSheetModelBase::Mode::kPinEntry));
      break;
    case Step::kTrustThisComputerAssertion:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorTrustThisComputerAssertionSheetModel>(
              dialog_model));
      break;
    case Step::kTrustThisComputerCreation:
      sheet_view = std::make_unique<AuthenticatorCreateUserSheetView>(
          std::make_unique<AuthenticatorTrustThisComputerCreationSheetModel>(
              dialog_model));
      break;
    case Step::kGPMCreatePasskey:
      sheet_view = std::make_unique<AuthenticatorCreateUserSheetView>(
          std::make_unique<AuthenticatorCreateGpmPasskeySheetModel>(
              dialog_model));
      break;
    case Step::kGPMConfirmOffTheRecordCreate:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorGpmIncognitoCreateSheetModel>(
              dialog_model));
      break;
    case Step::kGPMTouchID:
#if BUILDFLAG(IS_MAC)
      if (__builtin_available(macOS 12.0, *)) {
        sheet_view = std::make_unique<AuthenticatorTouchIdView>(
            std::make_unique<AuthenticatorTouchIdSheetModel>(dialog_model));
      } else {
        NOTREACHED() << "MacOS version does not support LAAuthenticationView";
      }
#else
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<PlaceholderSheetModel>(dialog_model));
#endif
      break;
    case Step::kGPMLockedPin:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<AuthenticatorGPMLockedPinSheetModel>(dialog_model));
      break;
    case Step::kNotStarted:
    case Step::kConditionalMediation:
    case Step::kClosed:
    case Step::kRecoverSecurityDomain:
    case Step::kGPMReauthForPinReset:
      sheet_view = std::make_unique<AuthenticatorRequestSheetView>(
          std::make_unique<PlaceholderSheetModel>(dialog_model));
      break;
  }

  CHECK(sheet_view);
  return sheet_view;
}

std::unique_ptr<AuthenticatorRequestSheetView>
CreateSheetViewForAutofillWebAuthn(
    std::unique_ptr<autofill::WebauthnDialogModel> model) {
  return std::make_unique<AuthenticatorRequestSheetView>(std::move(model));
}
