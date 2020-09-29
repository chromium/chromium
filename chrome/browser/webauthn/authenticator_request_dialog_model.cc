// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"

namespace {

// Attempts to auto-select the most likely transport that will be used to
// service this request, or returns base::nullopt if unsure.
base::Optional<device::FidoTransportProtocol> SelectMostLikelyTransport(
    const device::FidoRequestHandlerBase::TransportAvailabilityInfo&
        transport_availability,
    base::Optional<device::FidoTransportProtocol> last_used_transport,
    bool cable_extension_provided,
    bool have_paired_phones) {
  const base::flat_set<AuthenticatorTransport>& candidate_transports(
      transport_availability.available_transports);

  // If there is only one transport available, select that instead of showing a
  // transport selection screen with only a single item.
  if (candidate_transports.size() == 1) {
    return *candidate_transports.begin();
  }

  // The remaining decisions apply to GetAssertion requests only. For
  // MakeCredential, the user needs to choose from transport selection.
  if (transport_availability.request_type !=
      device::FidoRequestHandlerBase::RequestType::kGetAssertion) {
    return base::nullopt;
  }

  // Auto advance to Touch ID if the authenticator has a matching credential
  // for the (possibly empty) allow list.
  if (base::Contains(candidate_transports,
                     device::FidoTransportProtocol::kInternal) &&
      transport_availability.has_recognized_mac_touch_id_credential) {
    return device::FidoTransportProtocol::kInternal;
  }

  // If the RP supplied the caBLE extension then respect that and always select
  // caBLE for GetAssertion operations.
  if (cable_extension_provided &&
      base::Contains(
          candidate_transports,
          AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy)) {
    return AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy;
  }

  // The remaining decisions are based on the most recently used successful
  // transport.
  if (!last_used_transport ||
      !base::Contains(candidate_transports, *last_used_transport)) {
    return base::nullopt;
  }

  // Auto-advancing to Touch ID based on credential availability has been
  // handled above. Hence, at this point it does not have a matching credential
  // and should not be advanced to, because it would fail immediately.
  if (*last_used_transport == device::FidoTransportProtocol::kInternal) {
    return base::nullopt;
  }

  // Auto-advancing to caBLE based on a caBLEv1 request extension has been
  // handled above. For caBLEv2, only auto-advance if the user has previously
  // paired a caBLEv2 authenticator.
  if (*last_used_transport ==
          device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy &&
      !have_paired_phones) {
    return base::nullopt;
  }

  return *last_used_transport;
}

}  // namespace

AuthenticatorRequestDialogModel::EphemeralState::EphemeralState() = default;
AuthenticatorRequestDialogModel::EphemeralState::~EphemeralState() = default;

void AuthenticatorRequestDialogModel::EphemeralState::Reset() {
  selected_authenticator_id_ = base::nullopt;
  saved_authenticators_.RemoveAllAuthenticators();
  has_attempted_pin_entry_ = false;
  responses_.clear();
}

AuthenticatorRequestDialogModel::AuthenticatorRequestDialogModel(
    const std::string& relying_party_id)
    : relying_party_id_(relying_party_id) {}

AuthenticatorRequestDialogModel::~AuthenticatorRequestDialogModel() {
  for (auto& observer : observers_)
    observer.OnModelDestroyed();
}

void AuthenticatorRequestDialogModel::SetCurrentStep(Step step) {
  current_step_ = step;
  for (auto& observer : observers_)
    observer.OnStepTransition();
}

void AuthenticatorRequestDialogModel::HideDialog() {
  SetCurrentStep(Step::kNotStarted);
}

void AuthenticatorRequestDialogModel::StartFlow(
    TransportAvailabilityInfo transport_availability,
    base::Optional<device::FidoTransportProtocol> last_used_transport) {
  DCHECK_EQ(current_step(), Step::kNotStarted);

  transport_availability_ = std::move(transport_availability);
  last_used_transport_ = last_used_transport;

  StartGuidedFlowForMostLikelyTransportOrShowTransportSelection();
}

void AuthenticatorRequestDialogModel::StartOver() {
  ephemeral_state_.Reset();

  for (auto& observer : observers_)
    observer.OnStartOver();
  SetCurrentStep(Step::kTransportSelection);
}

void AuthenticatorRequestDialogModel::
    StartGuidedFlowForMostLikelyTransportOrShowTransportSelection() {
  DCHECK(current_step() == Step::kNotStarted);

  // If no authenticator other than the one for the native Windows API is
  // available, or if the sole authenticator is caBLE, but there's no caBLE
  // extension nor paired phone, then don't show Chrome UI but proceed straight
  // to the native Windows UI.
  if (transport_availability_.has_win_native_api_authenticator &&
      !win_native_api_already_tried_) {
    const auto& transports = transport_availability_.available_transports;
    if (transports.empty() ||
        (transports.size() == 1 &&
         base::Contains(
             transports,
             AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy) &&
         !cable_extension_provided_ && !have_paired_phones_)) {
      StartWinNativeApi();
      return;
    }
  }

  auto most_likely_transport =
      SelectMostLikelyTransport(transport_availability_, last_used_transport_,
                                cable_extension_provided_, have_paired_phones_);
  if (most_likely_transport) {
    StartGuidedFlowForTransport(*most_likely_transport);
  } else if (!transport_availability_.available_transports.empty()) {
    SetCurrentStep(Step::kTransportSelection);
  } else {
    SetCurrentStep(Step::kErrorNoAvailableTransports);
  }
}

void AuthenticatorRequestDialogModel::StartGuidedFlowForTransport(
    AuthenticatorTransport transport) {
  DCHECK(current_step() == Step::kTransportSelection ||
         current_step() == Step::kUsbInsertAndActivate ||
         current_step() == Step::kCableActivate ||
         current_step() == Step::kNotStarted);
  switch (transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      SetCurrentStep(Step::kUsbInsertAndActivate);
      break;
    case AuthenticatorTransport::kNearFieldCommunication:
      SetCurrentStep(Step::kTransportSelection);
      break;
    case AuthenticatorTransport::kInternal:
      StartTouchIdFlow();
      break;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      EnsureBleAdapterIsPoweredAndContinueWithCable();
      break;
    default:
      break;
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

void AuthenticatorRequestDialogModel::StartWinNativeApi() {
  DCHECK(transport_availability_.has_win_native_api_authenticator);

  if (might_create_resident_credential_ &&
      !transport_availability_.win_native_ui_shows_resident_credential_notice) {
    SetCurrentStep(Step::kResidentCredentialConfirmation);
  } else {
    HideDialogAndDispatchToNativeWindowsApi();
  }
}

void AuthenticatorRequestDialogModel::StartPhonePairing() {
  DCHECK(qr_generator_key_);
  SetCurrentStep(Step::kCableV2QRCode);
}

void AuthenticatorRequestDialogModel::
    EnsureBleAdapterIsPoweredAndContinueWithCable() {
  DCHECK(current_step() == Step::kTransportSelection ||
         current_step() == Step::kUsbInsertAndActivate ||
         current_step() == Step::kCableActivate ||
         current_step() == Step::kNotStarted);
  Step cable_step;
  if (cable_extension_provided_) {
    // caBLEv1.
    cable_step = Step::kCableActivate;
  } else {
    // caBLEv2. Display QR code if the user never paired a phone before, or
    // show instructions how to use the previously paired phone otherwise. The
    // user can still decide to pair a new phone on that screen.
    cable_step =
        have_paired_phones_ ? Step::kCableV2Activate : Step::kCableV2QRCode;
  }
  if (ble_adapter_is_powered()) {
    SetCurrentStep(cable_step);
    return;
  }

  next_step_once_ble_powered_ = cable_step;
  SetCurrentStep(transport_availability()->can_power_on_ble_adapter
                     ? Step::kBlePowerOnAutomatic
                     : Step::kBlePowerOnManual);
}

void AuthenticatorRequestDialogModel::ContinueWithFlowAfterBleAdapterPowered() {
  DCHECK(current_step() == Step::kBlePowerOnManual ||
         current_step() == Step::kBlePowerOnAutomatic);
  DCHECK(ble_adapter_is_powered());
  DCHECK(next_step_once_ble_powered_.has_value());

  SetCurrentStep(*next_step_once_ble_powered_);
}

void AuthenticatorRequestDialogModel::PowerOnBleAdapter() {
  DCHECK_EQ(current_step(), Step::kBlePowerOnAutomatic);
  if (!bluetooth_adapter_power_on_callback_)
    return;

  bluetooth_adapter_power_on_callback_.Run();
}

void AuthenticatorRequestDialogModel::TryUsbDevice() {
  DCHECK_EQ(current_step(), Step::kUsbInsertAndActivate);
}

void AuthenticatorRequestDialogModel::StartTouchIdFlow() {
  // Never try Touch ID if the request is known in advance to fail. Proceed to
  // a special error screen instead.
  if (transport_availability_.request_type ==
          device::FidoRequestHandlerBase::RequestType::kGetAssertion &&
      !transport_availability_.has_recognized_mac_touch_id_credential) {
    SetCurrentStep(Step::kErrorInternalUnrecognized);
    return;
  }

  if (transport_availability_.request_type ==
          device::FidoRequestHandlerBase::RequestType::kMakeCredential &&
      incognito_mode_) {
    SetCurrentStep(Step::kTouchIdIncognitoSpeedBump);
    return;
  }

  HideDialogAndTryTouchId();
}

void AuthenticatorRequestDialogModel::HideDialogAndTryTouchId() {
  HideDialog();

  auto& authenticators =
      ephemeral_state_.saved_authenticators_.authenticator_list();
  auto touch_id_authenticator_it =
      std::find_if(authenticators.begin(), authenticators.end(),
                   [](const auto& authenticator) {
                     return authenticator.transport ==
                            device::FidoTransportProtocol::kInternal;
                   });

  if (touch_id_authenticator_it == authenticators.end()) {
    return;
  }

  DispatchRequestAsync(&*touch_id_authenticator_it);
}

void AuthenticatorRequestDialogModel::Cancel() {
  if (is_request_complete()) {
    SetCurrentStep(Step::kClosed);
  }

  for (auto& observer : observers_)
    observer.OnCancelRequest();
}

void AuthenticatorRequestDialogModel::OnSheetModelDidChange() {
  for (auto& observer : observers_)
    observer.OnSheetModelChanged();
}

void AuthenticatorRequestDialogModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AuthenticatorRequestDialogModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AuthenticatorRequestDialogModel::OnRequestComplete() {
  SetCurrentStep(Step::kClosed);
}

void AuthenticatorRequestDialogModel::OnRequestTimeout() {
  // The request may time out while the UI shows a different error.
  if (!is_request_complete())
    SetCurrentStep(Step::kTimedOut);
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

void AuthenticatorRequestDialogModel::OnNoCommonAlgorithms() {
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogModel::OnAuthenticatorStorageFull() {
  SetCurrentStep(Step::kStorageFull);
}

void AuthenticatorRequestDialogModel::OnUserConsentDenied() {
  SetCurrentStep(Step::kErrorInternalUnrecognized);
}

bool AuthenticatorRequestDialogModel::OnWinUserCancelled() {
  // If caBLE v2 isn't enabled then this event isn't handled and will cause the
  // request to fail with a NotAllowedError.
  if (!base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport)) {
    return false;
  }

  // Otherwise, if the user cancels out of the Windows-native UI, we show the
  // transport selection dialog which allows them to pair a phone.
  win_native_api_already_tried_ = true;

  StartOver();
  return true;
}

void AuthenticatorRequestDialogModel::OnBluetoothPoweredStateChanged(
    bool powered) {
  transport_availability_.is_ble_powered = powered;

  for (auto& observer : observers_)
    observer.OnBluetoothPoweredStateChanged();

  // For the manual flow, the user has to click the "next" button explicitly.
  if (current_step() == Step::kBlePowerOnAutomatic)
    ContinueWithFlowAfterBleAdapterPowered();
}

void AuthenticatorRequestDialogModel::SetRequestCallback(
    RequestCallback request_callback) {
  request_callback_ = request_callback;
}

void AuthenticatorRequestDialogModel::SetBluetoothAdapterPowerOnCallback(
    base::RepeatingClosure bluetooth_adapter_power_on_callback) {
  bluetooth_adapter_power_on_callback_ = bluetooth_adapter_power_on_callback;
}

void AuthenticatorRequestDialogModel::SetPINCallback(
    base::OnceCallback<void(std::string)> pin_callback) {
  pin_callback_ = std::move(pin_callback);
}

void AuthenticatorRequestDialogModel::OnHavePIN(const std::string& pin) {
  if (!pin_callback_) {
    // Protect against the view submitting a PIN more than once without
    // receiving a matching response first. |SetPINCallback| is called again if
    // the user needs to be prompted for a retry.
    return;
  }
  std::move(pin_callback_).Run(pin);
  ephemeral_state_.has_attempted_pin_entry_ = true;
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
#if defined(OS_WIN)
    DCHECK(authenticator.IsWinNativeApiAuthenticator());
#endif  // defined(OS_WIN)
    return;
  }

  AuthenticatorReference authenticator_reference(
      authenticator.GetId(), authenticator.GetDisplayName(),
      *authenticator.AuthenticatorTransport());

  ephemeral_state_.saved_authenticators_.AddAuthenticator(
      std::move(authenticator_reference));
}

void AuthenticatorRequestDialogModel::RemoveAuthenticator(
    base::StringPiece authenticator_id) {
  ephemeral_state_.saved_authenticators_.RemoveAuthenticator(authenticator_id);
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
  if (!request_callback_)
    return;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(request_callback_, authenticator_id));
}

// SelectAccount is called to trigger an account selection dialog.
void AuthenticatorRequestDialogModel::SelectAccount(
    std::vector<device::AuthenticatorGetAssertionResponse> responses,
    base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
        callback) {
  ephemeral_state_.responses_ = std::move(responses);
  selection_callback_ = std::move(callback);
  SetCurrentStep(Step::kSelectAccount);
}

void AuthenticatorRequestDialogModel::OnAccountSelected(size_t index) {
  if (!selection_callback_) {
    // It's possible that the user could activate the dialog more than once
    // before the Webauthn request is completed and its torn down.
    return;
  }

  auto selected = std::move(ephemeral_state_.responses_[index]);
  ephemeral_state_.responses_.clear();
  std::move(selection_callback_).Run(std::move(selected));
}

void AuthenticatorRequestDialogModel::SetSelectedAuthenticatorForTesting(
    AuthenticatorReference test_authenticator) {
  ephemeral_state_.selected_authenticator_id_ =
      test_authenticator.authenticator_id;
  ephemeral_state_.saved_authenticators_.AddAuthenticator(
      std::move(test_authenticator));
}

void AuthenticatorRequestDialogModel::CollectPIN(
    base::Optional<int> attempts,
    base::OnceCallback<void(std::string)> provide_pin_cb) {
  pin_callback_ = std::move(provide_pin_cb);
  if (attempts) {
    pin_attempts_ = attempts;
    SetCurrentStep(Step::kClientPinEntry);
  } else {
    SetCurrentStep(Step::kClientPinSetup);
  }
}

void AuthenticatorRequestDialogModel::StartInlineBioEnrollment(
    base::OnceClosure next_callback) {
  max_bio_samples_ = base::nullopt;
  bio_samples_remaining_ = base::nullopt;
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
    base::OnceCallback<void(bool)> callback) {
  DCHECK(current_step_ != Step::kClosed);
  attestation_callback_ = std::move(callback);
  SetCurrentStep(Step::kAttestationPermissionRequest);
}

void AuthenticatorRequestDialogModel::set_cable_transport_info(
    bool cable_extension_provided,
    bool have_paired_phones,
    const base::Optional<std::array<uint8_t, device::cablev2::kQRKeySize>>&
        qr_generator_key) {
  cable_extension_provided_ = cable_extension_provided;
  have_paired_phones_ = have_paired_phones;
  qr_generator_key_ = qr_generator_key;
}

base::WeakPtr<AuthenticatorRequestDialogModel>
AuthenticatorRequestDialogModel::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
