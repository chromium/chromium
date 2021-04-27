// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/pin.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace {

// Attempts to auto-select the most likely transport that will be used to
// service this request, or returns base::nullopt if unsure.
base::Optional<device::FidoTransportProtocol> SelectMostLikelyTransport(
    const device::FidoRequestHandlerBase::TransportAvailabilityInfo&
        transport_availability,
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

  // Auto advance to the platform authenticator if it has a matching credential
  // for the (possibly empty) allow list.
  if (base::Contains(candidate_transports,
                     device::FidoTransportProtocol::kInternal) &&
      *transport_availability
           .has_recognized_platform_authenticator_credential) {
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

  return base::nullopt;
}

}  // namespace

AuthenticatorRequestDialogModel::EphemeralState::EphemeralState() = default;
AuthenticatorRequestDialogModel::EphemeralState::~EphemeralState() = default;

void AuthenticatorRequestDialogModel::EphemeralState::Reset() {
  selected_authenticator_id_ = base::nullopt;
  saved_authenticators_.RemoveAllAuthenticators();
  users_.clear();
}

AuthenticatorRequestDialogModel::AuthenticatorRequestDialogModel(
    const std::string& relying_party_id)
    : relying_party_id_(relying_party_id) {}

AuthenticatorRequestDialogModel::~AuthenticatorRequestDialogModel() {
  for (auto& observer : observers_)
    observer.OnModelDestroyed(this);
}

void AuthenticatorRequestDialogModel::SetCurrentStep(Step step) {
  if (!started_) {
    // Dialog isn't showing yet. Remember to show this step when it appears.
    pending_step_ = step;
    return;
  }

  current_step_ = step;
  for (auto& observer : observers_)
    observer.OnStepTransition();
}

void AuthenticatorRequestDialogModel::HideDialog() {
  SetCurrentStep(Step::kNotStarted);
}

void AuthenticatorRequestDialogModel::StartFlow(
    TransportAvailabilityInfo transport_availability,
    bool use_location_bar_bubble) {
  DCHECK(!started_);
  DCHECK_EQ(current_step(), Step::kNotStarted);

  started_ = true;
  transport_availability_ = std::move(transport_availability);
  use_location_bar_bubble_ = use_location_bar_bubble;

  if (use_location_bar_bubble_) {
    // This is a conditional request so show a lightweight, non-modal dialog
    // instead.
    StartLocationBarBubbleRequest();
  } else {
    StartGuidedFlowForMostLikelyTransportOrShowTransportSelection();
  }
}

void AuthenticatorRequestDialogModel::StartOver() {
  ephemeral_state_.Reset();

  for (auto& observer : observers_)
    observer.OnStartOver();

  if (use_location_bar_bubble_) {
    StartLocationBarBubbleRequest();
    return;
  }
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

  auto most_likely_transport = SelectMostLikelyTransport(
      transport_availability_, cable_extension_provided_, have_paired_phones_);
  if (pending_step_) {
    SetCurrentStep(*pending_step_);
    pending_step_.reset();
  } else if (most_likely_transport) {
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
         current_step() == Step::kAndroidAccessory ||
         current_step() == Step::kNotStarted);
  switch (transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      SetCurrentStep(Step::kUsbInsertAndActivate);
      break;
    case AuthenticatorTransport::kNearFieldCommunication:
      SetCurrentStep(Step::kTransportSelection);
      break;
    case AuthenticatorTransport::kInternal:
      StartPlatformAuthenticatorFlow();
      break;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      EnsureBleAdapterIsPoweredAndContinueWithCable();
      break;
    case AuthenticatorTransport::kAndroidAccessory:
      SetCurrentStep(Step::kAndroidAccessory);
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

  if (resident_key_requirement() !=
          device::ResidentKeyRequirement::kDiscouraged &&
      !transport_availability_.win_native_ui_shows_resident_credential_notice) {
    SetCurrentStep(Step::kResidentCredentialConfirmation);
  } else {
    HideDialogAndDispatchToNativeWindowsApi();
  }
}

void AuthenticatorRequestDialogModel::StartPhonePairing() {
  DCHECK(cable_qr_string_);
  SetCurrentStep(Step::kCableV2QRCode);
}

void AuthenticatorRequestDialogModel::
    EnsureBleAdapterIsPoweredAndContinueWithCable() {
  DCHECK(current_step() == Step::kTransportSelection ||
         current_step() == Step::kUsbInsertAndActivate ||
         current_step() == Step::kCableActivate ||
         current_step() == Step::kAndroidAccessory ||
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

void AuthenticatorRequestDialogModel::StartPlatformAuthenticatorFlow() {
  // Never try the platform authenticator if the request is known in advance to
  // fail. Proceed to a special error screen instead.
  if (transport_availability_.request_type ==
      device::FidoRequestHandlerBase::RequestType::kGetAssertion) {
    DCHECK(transport_availability_
               .has_recognized_platform_authenticator_credential);
    if (!*transport_availability_
              .has_recognized_platform_authenticator_credential) {
      SetCurrentStep(Step::kErrorInternalUnrecognized);
      return;
    }
  }

  if (transport_availability_.request_type ==
          device::FidoRequestHandlerBase::RequestType::kMakeCredential &&
      transport_availability_.is_off_the_record_context) {
    SetCurrentStep(Step::kPlatformAuthenticatorOffTheRecordInterstitial);
    return;
  }

  HideDialogAndDispatchToPlatformAuthenticator();
}

void AuthenticatorRequestDialogModel::
    HideDialogAndDispatchToPlatformAuthenticator() {
  HideDialog();

  auto& authenticators =
      ephemeral_state_.saved_authenticators_.authenticator_list();
  auto platform_authenticator_it =
      std::find_if(authenticators.begin(), authenticators.end(),
                   [](const auto& authenticator) {
                     return authenticator.transport ==
                            device::FidoTransportProtocol::kInternal;
                   });

  if (platform_authenticator_it == authenticators.end()) {
    return;
  }

  DispatchRequestAsync(&*platform_authenticator_it);
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
  if (current_step_ == Step::kLocationBarBubble) {
    Cancel();
    return;
  }
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
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogModel::OnNoCommonAlgorithms() {
  SetCurrentStep(Step::kMissingCapability);
}

void AuthenticatorRequestDialogModel::OnAuthenticatorStorageFull() {
  SetCurrentStep(Step::kStorageFull);
}

void AuthenticatorRequestDialogModel::OnUserConsentDenied() {
  if (use_location_bar_bubble_) {
    // Do not show a page-modal retry error sheet if the user cancelled out of
    // their platform authenticator while displaying the location bar bubble UI.
    Cancel();
    return;
  }
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
#if defined(OS_WIN)
    DCHECK(authenticator.IsWinNativeApiAuthenticator());
#endif  // defined(OS_WIN)
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

void AuthenticatorRequestDialogModel::StartLocationBarBubbleRequest() {
  ephemeral_state_.users_ = {};
  for (const auto& user :
       transport_availability_.recognized_platform_authenticator_credentials) {
    ephemeral_state_.users_.push_back(user);
  }
  SetCurrentStep(Step::kLocationBarBubble);
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
  if (preselected_account_) {
    for (auto& response : responses) {
      if (response.user_entity == preselected_account_) {
        std::move(callback).Run(std::move(response));
        return;
      }
    }
    // The user selected an account that was not part of the responses. This
    // shouldn't really happen, cancel the request.
    Cancel();
    return;
  }
  ephemeral_state_.responses_ = std::move(responses);
  ephemeral_state_.users_ = {};
  for (const auto& response : ephemeral_state_.responses_) {
    ephemeral_state_.users_.push_back(*response.user_entity);
  }
  selection_callback_ = std::move(callback);
  SetCurrentStep(Step::kSelectAccount);
}

void AuthenticatorRequestDialogModel::OnAccountSelected(size_t index) {
  if (ephemeral_state_.responses_.empty()) {
    // An account has been pre-selected from the conditional UI prompt.
    preselected_account_ = std::move(ephemeral_state_.users_.at(index));
    ephemeral_state_.users_.clear();
    HideDialogAndDispatchToPlatformAuthenticator();
    return;
  }

  if (!selection_callback_) {
    // It's possible that the user could activate the dialog more than once
    // before the Webauthn request is completed and its torn down.
    return;
  }

  device::AuthenticatorGetAssertionResponse response =
      std::move(ephemeral_state_.responses_.at(index));
  ephemeral_state_.users_.clear();
  ephemeral_state_.responses_.clear();
  std::move(selection_callback_).Run(std::move(response));
}

void AuthenticatorRequestDialogModel::SetSelectedAuthenticatorForTesting(
    AuthenticatorReference test_authenticator) {
  ephemeral_state_.selected_authenticator_id_ =
      test_authenticator.authenticator_id;
  ephemeral_state_.saved_authenticators_.AddAuthenticator(
      std::move(test_authenticator));
}

bool AuthenticatorRequestDialogModel::cable_should_suggest_usb() const {
  return base::Contains(transport_availability_.available_transports,
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
    bool is_enterprise_attestation,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(current_step_ != Step::kClosed);
  attestation_callback_ = std::move(callback);
  SetCurrentStep(is_enterprise_attestation
                     ? Step::kEnterpriseAttestationPermissionRequest
                     : Step::kAttestationPermissionRequest);
}

void AuthenticatorRequestDialogModel::set_cable_transport_info(
    bool cable_extension_provided,
    bool have_paired_phones,
    const base::Optional<std::string>& cable_qr_string) {
  cable_extension_provided_ = cable_extension_provided;
  have_paired_phones_ = have_paired_phones;
  cable_qr_string_ = cable_qr_string;
}

base::WeakPtr<AuthenticatorRequestDialogModel>
AuthenticatorRequestDialogModel::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
