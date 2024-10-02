// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/ui/webauthn/authenticator_request_window.h"
#include "chrome/browser/webauthn/authenticator_transport.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "components/device_event_log/device_event_log.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/fido_types.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// StepUiType enumerates the different types of UI that can be displayed.
enum class StepUIType {
  NONE,
  // A Chromium captive dialog.
  DIALOG,
  // A top-level window.
  WINDOW,
};

StepUIType step_ui_type(AuthenticatorRequestDialogModel::Step step) {
  switch (step) {
    case AuthenticatorRequestDialogModel::Step::kClosed:
    case AuthenticatorRequestDialogModel::Step::kNotStarted:
    case AuthenticatorRequestDialogModel::Step::kConditionalMediation:
      return StepUIType::NONE;

    case AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain:
    case AuthenticatorRequestDialogModel::Step::kGPMReauthForPinReset:
      return StepUIType::WINDOW;

    default:
      return StepUIType::DIALOG;
  }
}

std::optional<content::GlobalRenderFrameHostId> FrameHostIdFromMaybeNull(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == nullptr) {
    return std::nullopt;
  }
  return render_frame_host->GetGlobalId();
}

content::WebContents* GetWebContentsFromFrameHostId(
    std::optional<content::GlobalRenderFrameHostId> frame_host_id) {
  if (!frame_host_id) {
    return nullptr;
  }
  return content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(*frame_host_id));
}

}  // namespace

#define AUTHENTICATOR_REQUEST_EVENT_0(name) \
  void AuthenticatorRequestDialogModel::Observer::name() {}
#define AUTHENTICATOR_REQUEST_EVENT_1(name, arg1type) \
  void AuthenticatorRequestDialogModel::Observer::name(arg1type) {}
AUTHENTICATOR_EVENTS
#undef AUTHENTICATOR_REQUEST_EVENT_0
#undef AUTHENTICATOR_REQUEST_EVENT_1

AuthenticatorRequestDialogModel::AuthenticatorRequestDialogModel(
    content::RenderFrameHost* render_frame_host)
    : frame_host_id(FrameHostIdFromMaybeNull(render_frame_host)) {}

AuthenticatorRequestDialogModel::~AuthenticatorRequestDialogModel() {
  for (auto& observer : observers) {
    observer.OnModelDestroyed(this);
  }
}

void AuthenticatorRequestDialogModel::AddObserver(
    AuthenticatorRequestDialogModel::Observer* observer) {
  observers.AddObserver(observer);
}

void AuthenticatorRequestDialogModel::RemoveObserver(
    AuthenticatorRequestDialogModel::Observer* observer) {
  observers.RemoveObserver(observer);
}

void AuthenticatorRequestDialogModel::SetStep(Step step) {
  FIDO_LOG(EVENT) << "UI step: " << step;

  const StepUIType previous_ui_type = step_ui_type(step_);
  step_ = step;
  ui_disabled_ = false;

  const StepUIType ui_type = step_ui_type(step_);
  auto* web_contents = GetWebContentsFromFrameHostId(frame_host_id);
  if (previous_ui_type != ui_type && web_contents) {
    // The UI observes `OnStepTransition` and updates automatically.
    switch (ui_type) {
      case StepUIType::NONE:
        // Any UI will close itself.
        break;

      case StepUIType::DIALOG:
        ShowAuthenticatorRequestDialog(web_contents, this);
        break;

      case StepUIType::WINDOW:
        ShowAuthenticatorRequestWindow(web_contents, this);
        break;
    }
  }

  for (auto& observer : observers) {
    observer.OnStepTransition();
  }
}

void AuthenticatorRequestDialogModel::DisableUiOrShowLoadingDialog() {
  // If the current step is showing a dialog, disable it. Else, show the GPM
  // Connecting dialog. The native Touch ID control cannot be effectively
  // disabled so that sheet is an exception.
  if (should_dialog_be_closed() || step() == Step::kGPMTouchID) {
    SetStep(Step::kGPMConnecting);
  } else {
    ui_disabled_ = true;
    OnSheetModelChanged();
  }
}

bool AuthenticatorRequestDialogModel::should_dialog_be_closed() const {
  return step_ui_type(step_) != StepUIType::DIALOG;
}

std::optional<AccountInfo>
AuthenticatorRequestDialogModel::GetGpmAccountInfo() {
  Profile* profile = GetProfile();
  if (!profile) {
    return std::nullopt;
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return std::nullopt;
  }
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  CHECK(!core_account_info.IsEmpty());
  return identity_manager->FindExtendedAccountInfo(core_account_info);
}

std::string AuthenticatorRequestDialogModel::GetGpmAccountEmail() {
  std::optional<AccountInfo> account_info = GetGpmAccountInfo();
  if (!account_info) {
    return "";
  }
  return account_info->email;
}

Profile* AuthenticatorRequestDialogModel::GetProfile() {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(*frame_host_id);
  if (!rfh) {
    return nullptr;
  }
  return Profile::FromBrowserContext(rfh->GetBrowserContext());
}

#define AUTHENTICATOR_REQUEST_EVENT_0(name)        \
  void AuthenticatorRequestDialogModel::name() {   \
    const int start_generation = this->generation; \
    for (auto& observer : observers) {             \
      if (start_generation != this->generation) {  \
        break;                                     \
      }                                            \
      observer.name();                             \
    }                                              \
  }
#define AUTHENTICATOR_REQUEST_EVENT_1(name, arg1type)         \
  void AuthenticatorRequestDialogModel::name(arg1type arg1) { \
    const int start_generation = this->generation;            \
    for (auto& observer : observers) {                        \
      if (start_generation != this->generation) {             \
        break;                                                \
      }                                                       \
      observer.name(arg1);                                    \
    }                                                         \
  }
AUTHENTICATOR_EVENTS
#undef AUTHENTICATOR_REQUEST_EVENT_0
#undef AUTHENTICATOR_REQUEST_EVENT_1

std::ostream& operator<<(std::ostream& os,
                         const AuthenticatorRequestDialogModel::Step& step) {
  using Step = AuthenticatorRequestDialogModel::Step;
  constexpr auto kStepNames = base::MakeFixedFlatMap<Step, std::string_view>({
      {Step::kNotStarted, "kNotStarted"},
      {Step::kConditionalMediation, "kConditionalMediation"},
      {Step::kMechanismSelection, "kMechanismSelection"},
      {Step::kErrorNoAvailableTransports, "kErrorNoAvailableTransports"},
      {Step::kErrorNoPasskeys, "kErrorNoPasskeys"},
      {Step::kErrorInternalUnrecognized, "kErrorInternalUnrecognized"},
      {Step::kErrorWindowsHelloNotEnabled, "kErrorWindowsHelloNotEnabled"},
      {Step::kTimedOut, "kTimedOut"},
      {Step::kKeyNotRegistered, "kKeyNotRegistered"},
      {Step::kKeyAlreadyRegistered, "kKeyAlreadyRegistered"},
      {Step::kMissingCapability, "kMissingCapability"},
      {Step::kStorageFull, "kStorageFull"},
      {Step::kClosed, "kClosed"},
      {Step::kUsbInsertAndActivate, "kUsbInsertAndActivate"},
      {Step::kBlePowerOnAutomatic, "kBlePowerOnAutomatic"},
      {Step::kBlePowerOnManual, "kBlePowerOnManual"},
      {Step::kBlePermissionMac, "kBlePermissionMac"},
      {Step::kOffTheRecordInterstitial, "kOffTheRecordInterstitial"},
      {Step::kPhoneConfirmationSheet, "kPhoneConfirmationSheet"},
      {Step::kCableActivate, "kCableActivate"},
      {Step::kCableV2QRCode, "kCableV2QRCode"},
      {Step::kCableV2Connecting, "kCableV2Connecting"},
      {Step::kCableV2Connected, "kCableV2Connected"},
      {Step::kCableV2Error, "kCableV2Error"},
      {Step::kClientPinChange, "kClientPinChange"},
      {Step::kClientPinEntry, "kClientPinEntry"},
      {Step::kClientPinSetup, "kClientPinSetup"},
      {Step::kClientPinTapAgain, "kClientPinTapAgain"},
      {Step::kClientPinErrorSoftBlock, "kClientPinErrorSoftBlock"},
      {Step::kClientPinErrorHardBlock, "kClientPinErrorHardBlock"},
      {Step::kClientPinErrorAuthenticatorRemoved,
       "kClientPinErrorAuthenticatorRemoved"},
      {Step::kInlineBioEnrollment, "kInlineBioEnrollment"},
      {Step::kRetryInternalUserVerification, "kRetryInternalUserVerification"},
      {Step::kResidentCredentialConfirmation,
       "kResidentCredentialConfirmation"},
      {Step::kSelectAccount, "kSelectAccount"},
      {Step::kSelectSingleAccount, "kSelectSingleAccount"},
      {Step::kPreSelectAccount, "kPreSelectAccount"},
      {Step::kPreSelectSingleAccount, "kPreSelectSingleAccount"},
      {Step::kSelectPriorityMechanism, "kSelectPriorityMechanism"},
      {Step::kGPMChangePin, "kGPMChangePin"},
      {Step::kGPMCreatePin, "kGPMCreatePin"},
      {Step::kGPMEnterPin, "kGPMEnterPin"},
      {Step::kGPMChangeArbitraryPin, "kGPMChangeArbitraryPin"},
      {Step::kGPMCreateArbitraryPin, "kGPMCreateArbitraryPin"},
      {Step::kGPMEnterArbitraryPin, "kGPMEnterArbitraryPin"},
      {Step::kGPMTouchID, "kGPMTouchID"},
      {Step::kGPMCreatePasskey, "kGPMCreatePasskey"},
      {Step::kGPMConfirmOffTheRecordCreate, "kGPMConfirmOffTheRecordCreate"},
      {Step::kCreatePasskey, "kCreatePasskey"},
      {Step::kGPMError, "kGPMError"},
      {Step::kGPMConnecting, "kGPMConnecting"},
      {Step::kRecoverSecurityDomain, "kRecoverSecurityDomain"},
      {Step::kTrustThisComputerAssertion, "kTrustThisComputerAssertion"},
      {Step::kTrustThisComputerCreation, "kTrustThisComputerCreation"},
      {Step::kGPMReauthForPinReset, "kGPMReauthForPinReset"},
      {Step::kGPMLockedPin, "kGPMLockedPin"},
  });
  static_assert(Step::kMaxValue == Step::kGPMLockedPin &&
                    kStepNames.size() - 1 == static_cast<int>(Step::kMaxValue),
                "implement operator<< overload when adding new Step values");
  return os << kStepNames.at(step);
}

AuthenticatorRequestDialogModel::Mechanism::Mechanism(
    AuthenticatorRequestDialogModel::Mechanism::Type in_type,
    std::u16string in_name,
    std::u16string in_short_name,
    const gfx::VectorIcon& in_icon,
    base::RepeatingClosure in_callback)
    : type(std::move(in_type)),
      name(std::move(in_name)),
      short_name(std::move(in_short_name)),
      icon(in_icon),
      callback(std::move(in_callback)) {}
AuthenticatorRequestDialogModel::Mechanism::~Mechanism() = default;
AuthenticatorRequestDialogModel::Mechanism::Mechanism(Mechanism&&) = default;

AuthenticatorRequestDialogModel::Mechanism::CredentialInfo::CredentialInfo(
    device::AuthenticatorType source_in,
    std::vector<uint8_t> user_id_in)
    : source(source_in), user_id(std::move(user_id_in)) {}
AuthenticatorRequestDialogModel::Mechanism::CredentialInfo::CredentialInfo(
    const CredentialInfo&) = default;
AuthenticatorRequestDialogModel::Mechanism::CredentialInfo::~CredentialInfo() =
    default;
bool AuthenticatorRequestDialogModel::Mechanism::CredentialInfo::operator==(
    const CredentialInfo&) const = default;
