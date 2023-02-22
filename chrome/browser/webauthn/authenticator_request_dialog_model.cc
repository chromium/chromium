// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <iterator>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate.h"
#include "chrome/browser/password_manager/chrome_webauthn_credentials_delegate_factory.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/webauthn/webauthn_metrics_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/pin.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

// BleEvent enumerates user-visible BLE events.
enum class BleEvent {
  kAlreadyPowered = 0,    // BLE was already powered.
  kNeedsPowerAuto = 1,    // BLE was not powered and so we asked the user.
  kNeedsPowerManual = 2,  // BLE was not powered and so we asked the user, but
                          // they have to do it manually.
  kNewlyPowered = 3,      // BLE wasn't powered, but the user turned it on.

  kMaxValue = kNewlyPowered,
};

constexpr int GetMessageIdForTransportDescription(
    AuthenticatorTransport transport) {
  switch (transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      return IDS_WEBAUTHN_TRANSPORT_USB;
    case AuthenticatorTransport::kInternal:
      return IDS_WEBAUTHN_TRANSPORT_INTERNAL;
    case AuthenticatorTransport::kHybrid:
      return IDS_WEBAUTHN_TRANSPORT_CABLE;
    case AuthenticatorTransport::kAndroidAccessory:
      return IDS_WEBAUTHN_TRANSPORT_AOA;
    case AuthenticatorTransport::kBluetoothLowEnergy:
    case AuthenticatorTransport::kNearFieldCommunication:
      NOTREACHED();
      return 0;
  }
}

std::u16string GetTransportDescription(AuthenticatorTransport transport) {
  const int msg_id = GetMessageIdForTransportDescription(transport);
  if (!msg_id) {
    return std::u16string();
  }
  return l10n_util::GetStringUTF16(msg_id);
}

constexpr int GetMessageIdForTransportShortDescription(
    AuthenticatorTransport transport) {
  switch (transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      return IDS_WEBAUTHN_TRANSPORT_POPUP_USB;
    case AuthenticatorTransport::kInternal:
      return IDS_WEBAUTHN_TRANSPORT_POPUP_INTERNAL;
    case AuthenticatorTransport::kHybrid:
      return IDS_WEBAUTHN_TRANSPORT_POPUP_CABLE;
    case AuthenticatorTransport::kAndroidAccessory:
      return IDS_WEBAUTHN_TRANSPORT_POPUP_AOA;
    case AuthenticatorTransport::kBluetoothLowEnergy:
    case AuthenticatorTransport::kNearFieldCommunication:
      NOTREACHED();
      return 0;
  }
}

std::u16string GetTransportShortDescription(AuthenticatorTransport transport) {
  const int msg_id = GetMessageIdForTransportShortDescription(transport);
  if (!msg_id) {
    return std::u16string();
  }
  return l10n_util::GetStringUTF16(msg_id);
}

constexpr const gfx::VectorIcon& GetTransportIcon(
    AuthenticatorTransport transport) {
  switch (transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      return vector_icons::kUsbIcon;
    case AuthenticatorTransport::kInternal:
      return kLaptopIcon;
    case AuthenticatorTransport::kHybrid:
      return kSmartphoneIcon;
    case AuthenticatorTransport::kAndroidAccessory:
      return kUsbCableIcon;
    case AuthenticatorTransport::kBluetoothLowEnergy:
    case AuthenticatorTransport::kNearFieldCommunication:
      NOTREACHED();
      return gfx::kNoneIcon;
  }
}

// Whether to show Step::kCreatePasskey, which prompts the user before platform
// authenticator dispatch during MakeCredential. This is currently only shown on
// MacOS, because that is the only desktop platform authenticator without a
// "native" WebAuthn UI.
constexpr bool kShowCreatePlatformPasskeyStep = BUILDFLAG(IS_MAC);

}  // namespace

AuthenticatorRequestDialogModel::EphemeralState::EphemeralState() = default;
AuthenticatorRequestDialogModel::EphemeralState::EphemeralState(
    EphemeralState&&) = default;
AuthenticatorRequestDialogModel::EphemeralState&
AuthenticatorRequestDialogModel::EphemeralState::operator=(EphemeralState&&) =
    default;
AuthenticatorRequestDialogModel::EphemeralState::~EphemeralState() = default;

AuthenticatorRequestDialogModel::Mechanism::Mechanism(
    AuthenticatorRequestDialogModel::Mechanism::Type in_type,
    std::u16string in_name,
    std::u16string in_short_name,
    const gfx::VectorIcon& in_icon,
    base::RepeatingClosure in_callback,
    bool is_priority)
    : type(std::move(in_type)),
      name(std::move(in_name)),
      short_name(std::move(in_short_name)),
      icon(in_icon),
      callback(std::move(in_callback)),
      priority(is_priority) {}
AuthenticatorRequestDialogModel::Mechanism::~Mechanism() = default;
AuthenticatorRequestDialogModel::Mechanism::Mechanism(Mechanism&&) = default;

AuthenticatorRequestDialogModel::PairedPhone::PairedPhone(const PairedPhone&) =
    default;
AuthenticatorRequestDialogModel::PairedPhone::PairedPhone(
    const std::string& name,
    size_t contact_id,
    const std::array<uint8_t, device::kP256X962Length> public_key_x962) {
  this->name = name;
  this->contact_id = contact_id;
  this->public_key_x962 = public_key_x962;
}
AuthenticatorRequestDialogModel::PairedPhone::~PairedPhone() = default;
AuthenticatorRequestDialogModel::PairedPhone&
AuthenticatorRequestDialogModel::PairedPhone::operator=(const PairedPhone&) =
    default;

void AuthenticatorRequestDialogModel::ResetEphemeralState() {
  ephemeral_state_ = {};
}

AuthenticatorRequestDialogModel::AuthenticatorRequestDialogModel(
    content::RenderFrameHost* frame_host) {
  if (frame_host) {
    frame_host_id_ = frame_host->GetGlobalId();
  }
}

AuthenticatorRequestDialogModel::~AuthenticatorRequestDialogModel() {
  for (auto& observer : observers_) {
    observer.OnModelDestroyed(this);
  }
}

void AuthenticatorRequestDialogModel::HideDialog() {
  SetCurrentStep(Step::kNotStarted);
}

void AuthenticatorRequestDialogModel::StartFlow(
    TransportAvailabilityInfo transport_availability,
    bool use_conditional_mediation,
    bool prefer_native_api) {
  DCHECK(!started_);
  DCHECK_EQ(current_step(), Step::kNotStarted);

  started_ = true;
  transport_availability_ = std::move(transport_availability);
  use_conditional_mediation_ = use_conditional_mediation;

  PopulateMechanisms(prefer_native_api);

  if (use_conditional_mediation_) {
    // This is a conditional mediation request.
    StartConditionalMediationRequest();
  } else {
    StartGuidedFlowForMostLikelyTransportOrShowMechanismSelection();
  }
}

void AuthenticatorRequestDialogModel::StartOver() {
  ResetEphemeralState();

  for (auto& observer : observers_) {
    observer.OnStartOver();
  }

  current_mechanism_.reset();
  current_step_ = Step::kNotStarted;
  SetCurrentStep(Step::kMechanismSelection);
}

void AuthenticatorRequestDialogModel::TransitionToModalWebAuthnRequest() {
  DCHECK_EQ(current_step(), Step::kConditionalMediation);

  // Dispatch requests to any plugged in authenticators.
  for (auto& authenticator :
       ephemeral_state_.saved_authenticators_.authenticator_list()) {
    if (authenticator.transport != device::FidoTransportProtocol::kInternal) {
      DispatchRequestAsync(&authenticator);
    }
  }
  StartGuidedFlowForMostLikelyTransportOrShowMechanismSelection();
}

void AuthenticatorRequestDialogModel::
    StartGuidedFlowForMostLikelyTransportOrShowMechanismSelection() {
  const auto priority_mechanism_it =
      base::ranges::find_if(mechanisms_, &Mechanism::priority);

  if (pending_step_) {
    SetCurrentStep(*pending_step_);
    pending_step_.reset();
  } else if (mechanisms_.empty()) {
    if (base::FeatureList::IsEnabled(device::kWebAuthnNoPasskeysError) &&
        transport_availability_.transport_list_did_include_internal) {
      SetCurrentStep(Step::kErrorNoPasskeys);
    } else {
      SetCurrentStep(Step::kErrorNoAvailableTransports);
    }
  } else if (mechanisms_.size() == 1) {
    mechanisms_[0].callback.Run();
  } else if (priority_mechanism_it != mechanisms_.end()) {
    priority_mechanism_it->callback.Run();
  } else {
    SetCurrentStep(Step::kMechanismSelection);
  }
}

void AuthenticatorRequestDialogModel::
    HideDialogAndDispatchToNativeWindowsApi() {
  if (!transport_availability()->has_win_native_api_authenticator ||
      transport_availability()->win_native_api_authenticator_id.empty()) {
    NOTREACHED();
    SetCurrentStep(Step::kClosed);
    return;
  }

  // The Windows-native UI already handles retrying so we do not offer a second
  // level of retry in that case.
  offer_try_again_in_ui_ = false;

  // There is no AuthenticatorReference for the Windows authenticator, hence
  // directly call DispatchRequestAsyncInternal here.
  DispatchRequestAsyncInternal(
      transport_availability()->win_native_api_authenticator_id);

  HideDialog();
}

void AuthenticatorRequestDialogModel::OnPhoneContactFailed(
    const std::string& name) {
  ContactNextPhoneByName(name);
}

void AuthenticatorRequestDialogModel::StartPhonePairing() {
  DCHECK(cable_qr_string_);
  SetCurrentStep(Step::kCableV2QRCode);
}

void AuthenticatorRequestDialogModel::
    EnsureBleAdapterIsPoweredAndContinueWithStep(Step step) {
  DCHECK(current_step() == Step::kMechanismSelection ||
         current_step() == Step::kUsbInsertAndActivate ||
         current_step() == Step::kCableActivate ||
         current_step() == Step::kAndroidAccessory ||
         current_step() == Step::kOffTheRecordInterstitial ||
         current_step() == Step::kPreSelectAccount ||
         current_step() == Step::kSelectAccount ||
         current_step() == Step::kMechanismSelection ||
         current_step() == Step::kConditionalMediation ||
         current_step() == Step::kNotStarted)
      << "Invalid step " << static_cast<int>(current_step());

#if BUILDFLAG(IS_MAC)
  if (transport_availability()->ble_access_denied) {
    // |step| is not saved because macOS asks the user to restart Chrome
    // after permission has been granted. So the user will end up retrying
    // the whole WebAuthn request in the new process.
    SetCurrentStep(Step::kBlePermissionMac);
    return;
  }
#endif

  if (ble_adapter_is_powered()) {
    base::UmaHistogramEnumeration("WebAuthentication.BLEUserEvents",
                                  BleEvent::kAlreadyPowered);
    SetCurrentStep(step);
    return;
  }

  after_ble_adapter_powered_ =
      base::BindOnce(&AuthenticatorRequestDialogModel::SetCurrentStep,
                     weak_factory_.GetWeakPtr(), step);

  BleEvent event;
  if (transport_availability()->can_power_on_ble_adapter) {
    event = BleEvent::kNeedsPowerAuto;
    SetCurrentStep(Step::kBlePowerOnAutomatic);
  } else {
    event = BleEvent::kNeedsPowerManual;
    SetCurrentStep(Step::kBlePowerOnManual);
  }

  base::UmaHistogramEnumeration("WebAuthentication.BLEUserEvents", event);
}

void AuthenticatorRequestDialogModel::ContinueWithFlowAfterBleAdapterPowered() {
  DCHECK(current_step() == Step::kBlePowerOnManual ||
         current_step() == Step::kBlePowerOnAutomatic);
  DCHECK(ble_adapter_is_powered());
  base::UmaHistogramEnumeration("WebAuthentication.BLEUserEvents",
                                BleEvent::kNewlyPowered);

  std::move(after_ble_adapter_powered_).Run();
}

void AuthenticatorRequestDialogModel::PowerOnBleAdapter() {
  DCHECK_EQ(current_step(), Step::kBlePowerOnAutomatic);
  if (!bluetooth_adapter_power_on_callback_) {
    return;
  }

  bluetooth_adapter_power_on_callback_.Run();
}

#if BUILDFLAG(IS_MAC)
void AuthenticatorRequestDialogModel::OpenBlePreferences() {
  DCHECK_EQ(current_step(), Step::kBlePermissionMac);
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_Bluetooth);
}
#endif  // IS_MAC

void AuthenticatorRequestDialogModel::TryUsbDevice() {
  DCHECK_EQ(current_step(), Step::kUsbInsertAndActivate);
}

void AuthenticatorRequestDialogModel::StartPlatformAuthenticatorFlow() {
  // Never try the platform authenticator if the request is known in advance to
  // fail. Proceed to a special error screen instead.
  if (transport_availability_.request_type ==
      device::FidoRequestType::kGetAssertion) {
    DCHECK_NE(transport_availability_.has_platform_authenticator_credential,
              device::FidoRequestHandlerBase::RecognizedCredential::kUnknown);
    if (transport_availability_.has_platform_authenticator_credential ==
        device::FidoRequestHandlerBase::RecognizedCredential::
            kNoRecognizedCredential) {
      SetCurrentStep(Step::kErrorInternalUnrecognized);
      return;
    }

    // For empty allow list requests, let the user select one of the silently
    // enumerated credentials before dispatching to the platform authenticator.
    if (transport_availability_.has_empty_allow_list &&
        !transport_availability_.recognized_platform_authenticator_credentials
             .empty()) {
      ephemeral_state_.creds_ =
          transport_availability_.recognized_platform_authenticator_credentials;
      SetCurrentStep(ephemeral_state_.creds_.size() == 1
                         ? Step::kPreSelectSingleAccount
                         : Step::kPreSelectAccount);
      return;
    }
  }

  if (transport_availability_.request_type ==
      device::FidoRequestType::kMakeCredential) {
    if (kShowCreatePlatformPasskeyStep) {
      SetCurrentStep(Step::kCreatePasskey);
      return;
    }

    if (transport_availability_.is_off_the_record_context) {
      // Step::kCreatePasskey incorporates an incognito warning if
      // applicable, so the OTR interstitial step only needs to show in the
      // "old" UI.
      after_off_the_record_interstitial_ =
          base::BindOnce(&AuthenticatorRequestDialogModel::
                             HideDialogAndDispatchToPlatformAuthenticator,
                         weak_factory_.GetWeakPtr());
      SetCurrentStep(Step::kOffTheRecordInterstitial);
      return;
    }
  }

  HideDialogAndDispatchToPlatformAuthenticator();
}

void AuthenticatorRequestDialogModel::OnOffTheRecordInterstitialAccepted() {
  std::move(after_off_the_record_interstitial_).Run();
}

void AuthenticatorRequestDialogModel::ShowCableUsbFallback() {
  DCHECK_EQ(current_step(), Step::kCableActivate);
  SetCurrentStep(Step::kAndroidAccessory);
}

void AuthenticatorRequestDialogModel::ShowCable() {
  DCHECK_EQ(current_step(), Step::kAndroidAccessory);
  SetCurrentStep(Step::kCableActivate);
}

void AuthenticatorRequestDialogModel::Cancel() {
  if (use_conditional_mediation_) {
    // Conditional UI requests are never cancelled, they restart silently.
    ResetEphemeralState();
    for (auto& observer : observers_) {
      observer.OnStartOver();
    }
    StartConditionalMediationRequest();
    return;
  }

  if (is_request_complete()) {
    SetCurrentStep(Step::kClosed);
  }

  for (auto& observer : observers_) {
    observer.OnCancelRequest();
  }
}

void AuthenticatorRequestDialogModel::ManageDevices() {
  for (auto& observer : observers_) {
    observer.OnManageDevicesClicked();
  }
}

void AuthenticatorRequestDialogModel::OnSheetModelDidChange() {
  for (auto& observer : observers_) {
    observer.OnSheetModelChanged();
  }
}

void AuthenticatorRequestDialogModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AuthenticatorRequestDialogModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AuthenticatorRequestDialogModel::OnRequestComplete() {
  if (use_conditional_mediation_) {
    auto* render_frame_host = content::RenderFrameHost::FromID(frame_host_id_);
    auto* web_contents = GetWebContents();
    if (web_contents && render_frame_host) {
      ChromeWebAuthnCredentialsDelegateFactory::GetFactory(web_contents)
          ->GetDelegateForFrame(render_frame_host)
          ->NotifyWebAuthnRequestAborted();
    }
  }
  SetCurrentStep(Step::kClosed);
}

void AuthenticatorRequestDialogModel::OnRequestTimeout() {
  // The request may time out while the UI shows a different error.
  if (!is_request_complete()) {
    SetCurrentStep(Step::kTimedOut);
  }
}

void AuthenticatorRequestDialogModel::OnActivatedKeyNotRegistered() {
  DCHECK(!is_request_complete());
  SetCurrentStep(Step::kKeyNotRegistered);
}

void AuthenticatorRequestDialogModel::OnActivatedKeyAlreadyRegistered() {
  DCHECK(!is_request_complete());
  SetCurrentStep(Step::kKeyAlreadyRegistered);
}

void AuthenticatorRequestDialogModel::OnSoftPINBlock() {
  SetCurrentStep(Step::kClientPinErrorSoftBlock);
}

void AuthenticatorRequestDialogModel::OnHardPINBlock() {
  SetCurrentStep(Step::kClientPinErrorHardBlock);
}

void AuthenticatorRequestDialogModel::OnAuthenticatorRemovedDuringPINEntry() {
  SetCurrentStep(Step::kClientPinErrorAuthenticatorRemoved);
}

void AuthenticatorRequestDialogModel::OnAuthenticatorMissingResidentKeys() {
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogModel::OnAuthenticatorMissingUserVerification() {
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogModel::OnAuthenticatorMissingLargeBlob() {
  // TODO(nsatragno): on Windows we should have a more accurate message if large
  // blob is missing.
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogModel::OnNoCommonAlgorithms() {
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogModel::OnAuthenticatorStorageFull() {
  SetCurrentStep(Step::kStorageFull);
}

void AuthenticatorRequestDialogModel::OnUserConsentDenied() {
  if (use_conditional_mediation_) {
    // Do not show a page-modal retry error sheet if the user cancelled out of
    // their platform authenticator during a conditional UI request.
    // Instead, retry silently.
    Cancel();
    return;
  }
  SetCurrentStep(Step::kErrorInternalUnrecognized);
}

bool AuthenticatorRequestDialogModel::OnWinUserCancelled() {
#if BUILDFLAG(IS_WIN)
  if (use_conditional_mediation_) {
    // Do not show a page-modal retry error sheet if the user cancelled out of
    // their platform authenticator during a conditional UI request.
    // Instead, retry silently.
    Cancel();
    return true;
  }

  // If the native Windows API was triggered immediately (i.e. before any Chrome
  // dialog) then start the request over (once) if the user cancels the Windows
  // UI and there are other options in Chrome's UI.
  if (!have_restarted_due_to_windows_cancel_) {
    bool have_other_option =
        base::ranges::any_of(mechanisms_, [](const Mechanism& m) -> bool {
          return absl::holds_alternative<Mechanism::Phone>(m.type) ||
                 absl::holds_alternative<Mechanism::AddPhone>(m.type);
        });
    bool windows_was_priority =
        base::ranges::any_of(mechanisms_, [](const Mechanism& m) -> bool {
          return m.priority &&
                 absl::holds_alternative<Mechanism::WindowsAPI>(m.type);
        });
    if (have_other_option && windows_was_priority) {
      have_restarted_due_to_windows_cancel_ = true;
      StartOver();
      return true;
    }
  }
#endif

  return false;
}

void AuthenticatorRequestDialogModel::OnBluetoothPoweredStateChanged(
    bool powered) {
  transport_availability_.is_ble_powered = powered;

  for (auto& observer : observers_) {
    observer.OnBluetoothPoweredStateChanged();
  }

  // For the manual flow, the user has to click the "next" button explicitly.
  if (current_step() == Step::kBlePowerOnAutomatic) {
    ContinueWithFlowAfterBleAdapterPowered();
  }
}

void AuthenticatorRequestDialogModel::SetRequestCallback(
    RequestCallback request_callback) {
  request_callback_ = request_callback;
}

void AuthenticatorRequestDialogModel::SetAccountPreselectedCallback(
    content::AuthenticatorRequestClientDelegate::AccountPreselectedCallback
        callback) {
  account_preselected_callback_ = callback;
}

void AuthenticatorRequestDialogModel::SetBluetoothAdapterPowerOnCallback(
    base::RepeatingClosure bluetooth_adapter_power_on_callback) {
  bluetooth_adapter_power_on_callback_ = bluetooth_adapter_power_on_callback;
}

void AuthenticatorRequestDialogModel::OnHavePIN(std::u16string pin) {
  if (!pin_callback_) {
    // Protect against the view submitting a PIN more than once without
    // receiving a matching response first. |CollectPIN| is called again if
    // the user needs to be prompted for a retry.
    return;
  }
  std::move(pin_callback_).Run(pin);
}

void AuthenticatorRequestDialogModel::OnRetryUserVerification(int attempts) {
  uv_attempts_ = attempts;
  SetCurrentStep(Step::kRetryInternalUserVerification);
}

void AuthenticatorRequestDialogModel::OnResidentCredentialConfirmed() {
  DCHECK_EQ(current_step(), Step::kResidentCredentialConfirmation);
  HideDialogAndDispatchToNativeWindowsApi();
}

void AuthenticatorRequestDialogModel::OnAttestationPermissionResponse(
    bool attestation_permission_granted) {
  if (!attestation_callback_) {
    return;
  }
  std::move(attestation_callback_).Run(attestation_permission_granted);
}

void AuthenticatorRequestDialogModel::AddAuthenticator(
    const device::FidoAuthenticator& authenticator) {
  if (!authenticator.AuthenticatorTransport()) {
#if BUILDFLAG(IS_WIN)
    DCHECK_EQ(authenticator.GetType(),
              device::FidoAuthenticator::Type::kWinNative);
#endif  // BUILDFLAG(IS_WIN)
    return;
  }

  AuthenticatorReference authenticator_reference(
      authenticator.GetId(), *authenticator.AuthenticatorTransport());

  ephemeral_state_.saved_authenticators_.AddAuthenticator(
      std::move(authenticator_reference));
}

void AuthenticatorRequestDialogModel::RemoveAuthenticator(
    base::StringPiece authenticator_id) {
  ephemeral_state_.saved_authenticators_.RemoveAuthenticator(authenticator_id);
}

// SelectAccount is called to trigger an account selection dialog.
void AuthenticatorRequestDialogModel::SelectAccount(
    std::vector<device::AuthenticatorGetAssertionResponse> responses,
    base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
        callback) {
  ephemeral_state_.responses_ = std::move(responses);
  ephemeral_state_.creds_ = {};
  for (const auto& response : ephemeral_state_.responses_) {
    ephemeral_state_.creds_.emplace_back(
        relying_party_id_, response.credential->id, *response.user_entity);
  }
  selection_callback_ = std::move(callback);
  SetCurrentStep(ephemeral_state_.creds_.size() == 1
                     ? Step::kSelectSingleAccount
                     : Step::kSelectAccount);
}

void AuthenticatorRequestDialogModel::OnAccountSelected(size_t index) {
  if (!selection_callback_) {
    // It's possible that the user could activate the dialog more than once
    // before the Webauthn request is completed and its torn down.
    return;
  }

  device::AuthenticatorGetAssertionResponse response =
      std::move(ephemeral_state_.responses_.at(index));
  ephemeral_state_.creds_.clear();
  ephemeral_state_.responses_.clear();
  std::move(selection_callback_).Run(std::move(response));
}

void AuthenticatorRequestDialogModel::OnAccountPreselected(
    const std::vector<uint8_t>& credential_id) {
  for (size_t i = 0; i < creds().size(); ++i) {
    if (creds().at(i).cred_id == credential_id) {
      OnAccountPreselectedIndex(i);
      return;
    }
  }
  NOTREACHED() << "OnAccountPreselected() called with unknown credential_id "
               << base::HexEncode(credential_id);
}

void AuthenticatorRequestDialogModel::OnAccountPreselectedIndex(size_t index) {
  // User selected one of the platform authenticator credentials enumerated in
  // Conditional or regular modal UI prior to collecting user verification.
  // Run `account_preselected_callback_` to narrow the request to the selected
  // credential and dispatch to the platform authenticator.
  const device::DiscoverableCredentialMetadata& cred = creds().at(index);
  DCHECK(account_preselected_callback_);
  account_preselected_callback_.Run(cred.cred_id);
  ephemeral_state_.creds_.clear();
  if (transport_availability()->has_win_native_api_authenticator) {
    HideDialogAndDispatchToNativeWindowsApi();
  } else {
    HideDialogAndDispatchToPlatformAuthenticator();
  }
}

void AuthenticatorRequestDialogModel::SetSelectedAuthenticatorForTesting(
    AuthenticatorReference test_authenticator) {
  ephemeral_state_.selected_authenticator_id_ =
      test_authenticator.authenticator_id;
  ephemeral_state_.saved_authenticators_.AddAuthenticator(
      std::move(test_authenticator));
}

base::span<const AuthenticatorRequestDialogModel::Mechanism>
AuthenticatorRequestDialogModel::mechanisms() const {
  return mechanisms_;
}

absl::optional<size_t> AuthenticatorRequestDialogModel::current_mechanism()
    const {
  return current_mechanism_;
}

void AuthenticatorRequestDialogModel::ContactPhoneForTesting(
    const std::string& name) {
  // Ensure BLE is powered so that `ContactPhone()` shows the "Check your phone"
  // screen right away.
  transport_availability_.is_ble_powered = true;
  ContactPhone(name, /*mechanism_index=*/0);
}

void AuthenticatorRequestDialogModel::StartTransportFlowForTesting(
    AuthenticatorTransport transport) {
  StartGuidedFlowForTransport(transport, /*mechanism_index=*/0);
}

void AuthenticatorRequestDialogModel::SetCurrentStepForTesting(Step step) {
  SetCurrentStep(step);
}

bool AuthenticatorRequestDialogModel::cable_should_suggest_usb() const {
  // Offer AoA only for linked caBLEv2 authenticators, not caBLEv1.
  return cable_ui_type_ != CableUIType::CABLE_V1 &&
         base::Contains(transport_availability_.available_transports,
                        AuthenticatorTransport::kAndroidAccessory);
}

void AuthenticatorRequestDialogModel::CollectPIN(
    device::pin::PINEntryReason reason,
    device::pin::PINEntryError error,
    uint32_t min_pin_length,
    int attempts,
    base::OnceCallback<void(std::u16string)> provide_pin_cb) {
  pin_callback_ = std::move(provide_pin_cb);
  min_pin_length_ = min_pin_length;
  pin_error_ = error;
  switch (reason) {
    case device::pin::PINEntryReason::kChallenge:
      pin_attempts_ = attempts;
      SetCurrentStep(Step::kClientPinEntry);
      return;
    case device::pin::PINEntryReason::kChange:
      SetCurrentStep(Step::kClientPinChange);
      return;
    case device::pin::PINEntryReason::kSet:
      SetCurrentStep(Step::kClientPinSetup);
      return;
  }
}

void AuthenticatorRequestDialogModel::FinishCollectToken() {
  SetCurrentStep(Step::kClientPinTapAgain);
}

void AuthenticatorRequestDialogModel::StartInlineBioEnrollment(
    base::OnceClosure next_callback) {
  max_bio_samples_ = absl::nullopt;
  bio_samples_remaining_ = absl::nullopt;
  bio_enrollment_callback_ = std::move(next_callback);
  SetCurrentStep(Step::kInlineBioEnrollment);
}

void AuthenticatorRequestDialogModel::OnSampleCollected(
    int bio_samples_remaining) {
  DCHECK(current_step_ == Step::kInlineBioEnrollment);

  bio_samples_remaining_ = bio_samples_remaining;
  if (!max_bio_samples_) {
    max_bio_samples_ = bio_samples_remaining + 1;
  }
  OnSheetModelDidChange();
}

void AuthenticatorRequestDialogModel::OnBioEnrollmentDone() {
  std::move(bio_enrollment_callback_).Run();
}

void AuthenticatorRequestDialogModel::RequestAttestationPermission(
    bool is_enterprise_attestation,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(current_step_ != Step::kClosed);
  attestation_callback_ = std::move(callback);
  SetCurrentStep(is_enterprise_attestation
                     ? Step::kEnterpriseAttestationPermissionRequest
                     : Step::kAttestationPermissionRequest);
}

void AuthenticatorRequestDialogModel::set_cable_transport_info(
    absl::optional<bool> extension_is_v2,
    std::vector<PairedPhone> paired_phones,
    base::RepeatingCallback<void(size_t)> contact_phone_callback,
    const absl::optional<std::string>& cable_qr_string) {
  DCHECK(paired_phones.empty() || contact_phone_callback);

  if (extension_is_v2.has_value()) {
    cable_extension_provided_ = true;
    if (*extension_is_v2) {
      cable_ui_type_ = CableUIType::CABLE_V2_SERVER_LINK;
    } else {
      cable_ui_type_ = CableUIType::CABLE_V1;
    }
  } else {
    cable_ui_type_ = CableUIType::CABLE_V2_2ND_FACTOR;
  }

  paired_phones_ = std::move(paired_phones);
  contact_phone_callback_ = std::move(contact_phone_callback);
  cable_qr_string_ = cable_qr_string;

  paired_phones_contacted_.assign(paired_phones_.size(), false);
}

std::vector<std::string> AuthenticatorRequestDialogModel::paired_phone_names()
    const {
  std::vector<std::string> names;
  std::transform(paired_phones_.begin(), paired_phones_.end(),
                 std::back_inserter(names),
                 [](const PairedPhone& phone) -> const std::string& {
                   return phone.name;
                 });
  names.erase(std::unique(names.begin(), names.end()), names.end());
  return names;
}

void AuthenticatorRequestDialogModel::ReplaceCredListForTesting(
    std::vector<device::DiscoverableCredentialMetadata> creds) {
  ephemeral_state_.creds_ = std::move(creds);
}

base::WeakPtr<AuthenticatorRequestDialogModel>
AuthenticatorRequestDialogModel::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

content::WebContents* AuthenticatorRequestDialogModel::GetWebContents() {
  return content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(frame_host_id_));
}

void AuthenticatorRequestDialogModel::SetCurrentStep(Step step) {
  if (!started_) {
    // Dialog isn't showing yet. Remember to show this step when it appears.
    pending_step_ = step;
    return;
  }

  current_step_ = step;
  if (should_dialog_be_closed()) {
    // The dialog will close itself.
    showing_dialog_ = false;
  } else {
    auto* web_contents = GetWebContents();
    if (!showing_dialog_ && web_contents) {
      ShowAuthenticatorRequestDialog(web_contents, this);
      showing_dialog_ = true;
    }
  }

  for (auto& observer : observers_) {
    observer.OnStepTransition();
  }
}

void AuthenticatorRequestDialogModel::StartGuidedFlowForTransport(
    AuthenticatorTransport transport,
    size_t mechanism_index) {
  current_mechanism_ = mechanism_index;

  DCHECK(current_step() == Step::kMechanismSelection ||
         current_step() == Step::kUsbInsertAndActivate ||
         current_step() == Step::kCableActivate ||
         current_step() == Step::kAndroidAccessory ||
         current_step() == Step::kConditionalMediation ||
         current_step() == Step::kCreatePasskey ||
         current_step() == Step::kPreSelectAccount ||
         current_step() == Step::kSelectAccount ||
         current_step() == Step::kNotStarted);
  switch (transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      SetCurrentStep(Step::kUsbInsertAndActivate);
      break;
    case AuthenticatorTransport::kInternal:
      StartPlatformAuthenticatorFlow();
      break;
    case AuthenticatorTransport::kHybrid:
      EnsureBleAdapterIsPoweredAndContinueWithStep(Step::kCableActivate);
      break;
    case AuthenticatorTransport::kAndroidAccessory:
      SetCurrentStep(Step::kAndroidAccessory);
      break;
    default:
      break;
  }
}

void AuthenticatorRequestDialogModel::StartGuidedFlowForAddPhone(
    size_t mechanism_index) {
  current_mechanism_ = mechanism_index;
  EnsureBleAdapterIsPoweredAndContinueWithStep(Step::kCableV2QRCode);
}

void AuthenticatorRequestDialogModel::StartWinNativeApi(
    size_t mechanism_index) {
  DCHECK(transport_availability_.has_win_native_api_authenticator);
  current_mechanism_ = mechanism_index;

  if (resident_key_requirement() !=
          device::ResidentKeyRequirement::kDiscouraged &&
      !transport_availability_.win_native_ui_shows_resident_credential_notice) {
    SetCurrentStep(Step::kResidentCredentialConfirmation);
  } else {
    HideDialogAndDispatchToNativeWindowsApi();
  }
}

void AuthenticatorRequestDialogModel::ContactPhone(const std::string& name,
                                                   size_t mechanism_index) {
  current_mechanism_ = mechanism_index;

  if (transport_availability_.request_type ==
          device::FidoRequestType::kMakeCredential &&
      transport_availability_.is_off_the_record_context) {
    after_off_the_record_interstitial_ =
        base::BindOnce(&AuthenticatorRequestDialogModel::
                           ContactPhoneAfterOffTheRecordInterstitial,
                       weak_factory_.GetWeakPtr(), name);
    SetCurrentStep(Step::kOffTheRecordInterstitial);
    return;
  }

  ContactPhoneAfterOffTheRecordInterstitial(name);
}

void AuthenticatorRequestDialogModel::ContactPhoneAfterOffTheRecordInterstitial(
    std::string name) {
  if (!ble_adapter_is_powered()) {
    after_ble_adapter_powered_ = base::BindOnce(
        &AuthenticatorRequestDialogModel::ContactPhoneAfterBleIsPowered,
        weak_factory_.GetWeakPtr(), std::move(name));

    BleEvent event;
    if (transport_availability()->can_power_on_ble_adapter) {
      event = BleEvent::kNeedsPowerAuto;
      SetCurrentStep(Step::kBlePowerOnAutomatic);
    } else {
      event = BleEvent::kNeedsPowerManual;
      SetCurrentStep(Step::kBlePowerOnManual);
    }
    base::UmaHistogramEnumeration("WebAuthentication.BLEUserEvents", event);
    return;
  }

  base::UmaHistogramEnumeration("WebAuthentication.BLEUserEvents",
                                BleEvent::kAlreadyPowered);
  ContactPhoneAfterBleIsPowered(std::move(name));
}

void AuthenticatorRequestDialogModel::ContactPhoneAfterBleIsPowered(
    std::string name) {
  ContactNextPhoneByName(name);
  SetCurrentStep(Step::kCableActivate);
}

void AuthenticatorRequestDialogModel::StartConditionalMediationRequest() {
  ephemeral_state_.creds_ =
      transport_availability_.recognized_platform_authenticator_credentials;

  auto* render_frame_host = content::RenderFrameHost::FromID(frame_host_id_);
  auto* web_contents = GetWebContents();
  if (web_contents && render_frame_host) {
    ReportConditionalUiPasskeyCount(ephemeral_state_.creds_.size());
    ChromeWebAuthnCredentialsDelegateFactory::GetFactory(web_contents)
        ->GetDelegateForFrame(render_frame_host)
        ->OnCredentialsReceived(ephemeral_state_.creds_);
  }

  SetCurrentStep(Step::kConditionalMediation);
}

void AuthenticatorRequestDialogModel::DispatchRequestAsync(
    AuthenticatorReference* authenticator) {
  // Dispatching to the same authenticator twice may result in unexpected
  // behavior.
  if (authenticator->dispatched) {
    return;
  }

  DispatchRequestAsyncInternal(authenticator->authenticator_id);
  authenticator->dispatched = true;
}

void AuthenticatorRequestDialogModel::DispatchRequestAsyncInternal(
    const std::string& authenticator_id) {
  if (!request_callback_) {
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(request_callback_, authenticator_id));
}

void AuthenticatorRequestDialogModel::ContactNextPhoneByName(
    const std::string& name) {
  bool found_name = false;
  ephemeral_state_.selected_phone_name_.reset();
  for (size_t i = 0; i != paired_phones_.size(); i++) {
    const PairedPhone& phone = paired_phones_[i];
    if (phone.name == name) {
      found_name = true;
      ephemeral_state_.selected_phone_name_ = name;
      if (!paired_phones_contacted_[i]) {
        paired_phones_contacted_[i] = true;
        contact_phone_callback_.Run(phone.contact_id);
        break;
      }
    } else if (found_name) {
      // |paired_phones_| is sorted by name so as soon as we see a mismatch
      // after a match, we're done.
      break;
    }
  }

  DCHECK(found_name);
}

void AuthenticatorRequestDialogModel::PopulateMechanisms(
    bool prefer_native_api) {
  const bool is_get_assertion = transport_availability_.request_type ==
                                device::FidoRequestType::kGetAssertion;
  const bool is_passkey_request =
      ((is_get_assertion &&
        (transport_availability_.has_empty_allow_list ||
         transport_availability_.is_only_hybrid_or_internal)) ||
       (!is_get_assertion && resident_key_requirement() !=
                                 device::ResidentKeyRequirement::kDiscouraged));
  // priority_transport contains the transport that should be activated
  // immediately, if this is a getAssertion.
  absl::optional<AuthenticatorTransport> priority_transport;

  std::vector<AuthenticatorTransport> transports_to_list_if_active;
  if (!use_conditional_mediation_ &&
      base::Contains(transport_availability_.available_transports,
                     AuthenticatorTransport::kInternal)) {
    // Conditional requests offer platform credentials through the autofill UI.
    transports_to_list_if_active.push_back(AuthenticatorTransport::kInternal);
    bool make_credential_prefer_internal =
        !is_get_assertion && kShowCreatePlatformPasskeyStep;
    if (base::FeatureList::IsEnabled(device::kWebAuthPasskeysUI)) {
      // Do not prefer the internal authenticator for passkeys requests if the
      // QR-code first flow is enabled.
      make_credential_prefer_internal =
          make_credential_prefer_internal && !is_passkey_request;
    }
    if (transport_availability_.has_platform_authenticator_credential ==
            device::FidoRequestHandlerBase::RecognizedCredential::
                kHasRecognizedCredential ||
        make_credential_prefer_internal) {
      priority_transport = AuthenticatorTransport::kInternal;
    }
  }
  transports_to_list_if_active.push_back(
      AuthenticatorTransport::kUsbHumanInterfaceDevice);

  const auto kCable = AuthenticatorTransport::kHybrid;
  bool include_add_phone_option = false;

  if (cable_ui_type_) {
    switch (*cable_ui_type_) {
      case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_2ND_FACTOR:
        if (base::Contains(transport_availability_.available_transports,
                           kCable)) {
          include_add_phone_option = true;
        }
        break;

      case AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_SERVER_LINK:
        transports_to_list_if_active.push_back(
            AuthenticatorTransport::kAndroidAccessory);
        [[fallthrough]];

      case AuthenticatorRequestDialogModel::CableUIType::CABLE_V1: {
        if (base::Contains(transport_availability_.available_transports,
                           kCable)) {
          transports_to_list_if_active.push_back(kCable);
          if (!priority_transport) {
            priority_transport = kCable;
          }

          // If this is a caBLEv1 or server-link request then offering to "Try
          // Again" is unfortunate because the server won't send another ping
          // to the phone. It is valid if trying to use USB devices but the
          // confusion of the caBLE case overrides that.
          offer_try_again_in_ui_ = false;
        }
        break;
      }
    }
  }

  // The Windows API option comes first so that it gets focus and people can
  // select it by simply hitting enter.
  if (win_native_api_enabled()) {
    const std::u16string desc = l10n_util::GetStringUTF16(
        IDS_WEBAUTHN_TRANSPORT_POPUP_DIFFERENT_AUTHENTICATOR_WIN);
    // Prefer going straight to Windows native UI for requests that are not
    // clearly passkeys related, or where a platform credential may satisfy the
    // request, except for:
    //  - conditional UI
    //  - "legacy" caBLE (caBLEv1 and server-link caBLEv2 on a.g.c)
    bool is_legacy_cable =
        cable_ui_type_ && cable_ui_type_ != CableUIType::CABLE_V2_2ND_FACTOR;
    bool win_api_should_be_priority =
        !use_conditional_mediation_ && !is_legacy_cable &&
        (!is_passkey_request ||
         transport_availability_.has_platform_authenticator_credential ==
             device::FidoRequestHandlerBase::RecognizedCredential::
                 kHasRecognizedCredential);
    mechanisms_.emplace_back(
        Mechanism::WindowsAPI(/*unused*/ true), desc, desc,
        GetTransportIcon(AuthenticatorTransport::kUsbHumanInterfaceDevice),
        base::BindRepeating(&AuthenticatorRequestDialogModel::StartWinNativeApi,
                            base::Unretained(this), mechanisms_.size()),
        !priority_transport.has_value() && win_api_should_be_priority);
  }

  bool specific_phones_listed = false;
  if (base::Contains(transport_availability_.available_transports, kCable)) {
    for (const auto& phone_name : paired_phone_names()) {
      const std::u16string name16 = base::UTF8ToUTF16(phone_name);
      static constexpr size_t kMaxLongNameChars = 50;
      static constexpr size_t kMaxShortNameChars = 30;
      std::u16string long_name, short_name;
      gfx::ElideString(name16, kMaxLongNameChars, &long_name);
      gfx::ElideString(name16, kMaxShortNameChars, &short_name);

      mechanisms_.emplace_back(
          Mechanism::Phone(phone_name), std::move(long_name),
          std::move(short_name), kSmartphoneIcon,
          base::BindRepeating(&AuthenticatorRequestDialogModel::ContactPhone,
                              base::Unretained(this), phone_name,
                              mechanisms_.size()),
          /*priority=*/false);
      specific_phones_listed = true;
    }
  }

  if (include_add_phone_option) {
    // If there's no other priority mechanism, no phones, no platform
    // credentials, and this is a passkey request, jump directly to showing a QR
    // code.
    bool is_priority = false;
    if (base::FeatureList::IsEnabled(device::kWebAuthPasskeysUI)) {
      // On Windows<=10, we cannot tell in advance if the platform authenticator
      // can fulfill a get assertion request. In that case, don't jump to the QR
      // code.
      bool platform_authenticator_could_fulfill_get_assertion =
          is_get_assertion && !use_conditional_mediation_ &&
          transport_availability_.has_platform_authenticator_credential !=
              device::FidoRequestHandlerBase::RecognizedCredential::
                  kNoRecognizedCredential;
      is_priority = !priority_transport.has_value() &&
                    !base::ranges::any_of(mechanisms_, &Mechanism::priority) &&
                    paired_phone_names().empty() && is_passkey_request &&
                    !platform_authenticator_could_fulfill_get_assertion;
    }
    const std::u16string label = l10n_util::GetStringUTF16(
        specific_phones_listed
            ? IDS_WEBAUTHN_PASSKEY_DIFFERENT_PHONE_OR_TABLET_LABEL
            : IDS_WEBAUTHN_PASSKEY_PHONE_OR_TABLET_LABEL);
    mechanisms_.emplace_back(
        Mechanism::AddPhone(), label, label, kQrcodeGeneratorIcon,
        base::BindRepeating(
            &AuthenticatorRequestDialogModel::StartGuidedFlowForAddPhone,
            base::Unretained(this), mechanisms_.size()),
        is_priority);
  }

  for (const auto transport : transports_to_list_if_active) {
    if (!base::Contains(transport_availability_.available_transports,
                        transport)) {
      continue;
    }

    mechanisms_.emplace_back(
        Mechanism::Transport(transport), GetTransportDescription(transport),
        GetTransportShortDescription(transport), GetTransportIcon(transport),
        base::BindRepeating(
            &AuthenticatorRequestDialogModel::StartGuidedFlowForTransport,
            base::Unretained(this), transport, mechanisms_.size()),
        priority_transport.has_value() && *priority_transport == transport);
  }

  // At most one mechanism has priority.
  DCHECK_LE(base::ranges::count_if(mechanisms_, &Mechanism::priority), 1);
}

void AuthenticatorRequestDialogModel::
    HideDialogAndDispatchToPlatformAuthenticator() {
  HideDialog();

  auto& authenticators =
      ephemeral_state_.saved_authenticators_.authenticator_list();
  auto platform_authenticator_it = base::ranges::find(
      authenticators, device::FidoTransportProtocol::kInternal,
      &AuthenticatorReference::transport);

  if (platform_authenticator_it == authenticators.end()) {
    return;
  }

  DispatchRequestAsync(&*platform_authenticator_it);
}
