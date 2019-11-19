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
#include "device/fido/fido_authenticator.h"

namespace {

// CableV1Event enumerates several things that can occur during a caBLE v1
// transaction. Do not change assigned values since they are used in histograms,
// only append new values. Keep synced with enums.xml.
enum class CableV1Event : int {
  kFlowStart = 0,
  kBLEHardwareFound = 1,
  kBLEAlreadyPowered = 2,
  kBLEAutoPower = 3,
  kBLEManualPower = 4,
  kTransmitting = 5,
  kSuccess = 6,
  kTimeout = 7,
  kMaxValue = kTimeout,
};

void RecordCableV1Event(CableV1Event event) {
  UMA_HISTOGRAM_ENUMERATION("WebAuthentication.CableV1Event", event);
}

bool ShouldShowBlePairingUI(
    bool previously_paired_with_bluetooth_authenticator,
    bool pair_with_new_device_for_bluetooth_low_energy) {
  if (pair_with_new_device_for_bluetooth_low_energy)
    return true;

  return !previously_paired_with_bluetooth_authenticator;
}

// Attempts to auto-select the most likely transport that will be used to
// service this request, or returns base::nullopt if unsure.
base::Optional<device::FidoTransportProtocol> SelectMostLikelyTransport(
    const device::FidoRequestHandlerBase::TransportAvailabilityInfo&
        transport_availability,
    base::Optional<device::FidoTransportProtocol> last_used_transport,
    bool cable_extension_provided,
    bool have_paired_phones) {
  base::flat_set<AuthenticatorTransport> candidate_transports(
      transport_availability.available_transports);

  // For GetAssertion requests, auto advance to Touch ID if the authenticator
  // has a matching credential for the (possibly empty) allow list.
  if (transport_availability.request_type ==
          device::FidoRequestHandlerBase::RequestType::kGetAssertion &&
      base::Contains(candidate_transports,
                     device::FidoTransportProtocol::kInternal) &&
      transport_availability.has_recognized_mac_touch_id_credential) {
    return device::FidoTransportProtocol::kInternal;
  }

  // If the RP supplied the caBLE extension then respect that and always
  // select caBLE for GetAssertion operations.
  if (transport_availability.request_type ==
          device::FidoRequestHandlerBase::RequestType::kGetAssertion &&
      cable_extension_provided &&
      base::Contains(
          candidate_transports,
          AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy)) {
    return AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy;
  }

  // Otherwise, for GetAssertion calls, if the |last_used_transport| is
  // available, use that. Unless the preference is Touch ID, because Touch ID
  // at this point is guaranteed to not have the credential and would go
  // straight to its special error screen.
  if (transport_availability.request_type ==
          device::FidoRequestHandlerBase::RequestType::kGetAssertion &&
      last_used_transport &&
      base::Contains(candidate_transports, *last_used_transport) &&
      *last_used_transport != device::FidoTransportProtocol::kInternal &&
      (have_paired_phones ||
       *last_used_transport !=
           device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy)) {
    return *last_used_transport;
  }

  // Finally, if there is only one transport available we can use, select that,
  // instead of showing a transport selection screen with only a single item.
  if (candidate_transports.size() == 1) {
    return *candidate_transports.begin();
  }

  return base::nullopt;
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
    base::Optional<device::FidoTransportProtocol> last_used_transport,
    const base::ListValue* previously_paired_bluetooth_device_list) {
  DCHECK_EQ(current_step(), Step::kNotStarted);

  if (cable_extension_provided_) {
    RecordCableV1Event(CableV1Event::kFlowStart);
  }

  transport_availability_ = std::move(transport_availability);
  last_used_transport_ = last_used_transport;
  for (const auto transport : transport_availability_.available_transports) {
    if (transport == AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy) {
      if (cable_extension_provided_) {
        RecordCableV1Event(CableV1Event::kBLEHardwareFound);
      }
      if (!cable_extension_provided_ && !have_paired_phones_) {
        continue;
      }
    }
    available_transports_.emplace_back(transport);
  }

  previously_paired_with_bluetooth_authenticator_ =
      previously_paired_bluetooth_device_list &&
      !previously_paired_bluetooth_device_list->GetList().empty();

  StartGuidedFlowForMostLikelyTransportOrShowTransportSelection();
}

void AuthenticatorRequestDialogModel::StartOver() {
  if (!request_may_start_over_) {
    NOTREACHED();
    return;
  }
  ephemeral_state_.Reset();

  for (auto& observer : observers_)
    observer.OnStartOver();
  SetCurrentStep(Step::kTransportSelection);
}

void AuthenticatorRequestDialogModel::
    StartGuidedFlowForMostLikelyTransportOrShowTransportSelection() {
  DCHECK(current_step() == Step::kNotStarted);

  // If no authenticator other than the one for the native Windows API is
  // available, don't show Chrome UI but proceed straight to the native
  // Windows UI.
  if (transport_availability_.has_win_native_api_authenticator &&
      transport_availability_.available_transports.empty()) {
    if (might_create_resident_credential_ &&
        !transport_availability_
             .win_native_ui_shows_resident_credential_notice) {
      SetCurrentStep(Step::kResidentCredentialConfirmation);
    } else {
      HideDialogAndDispatchToNativeWindowsApi();
    }
    return;
  }

  auto most_likely_transport =
      SelectMostLikelyTransport(transport_availability_, last_used_transport_,
                                cable_extension_provided_, have_paired_phones_);
  if (most_likely_transport) {
    StartGuidedFlowForTransport(*most_likely_transport);
  } else if (!transport_availability_.available_transports.empty()) {
    DCHECK_GE(transport_availability_.available_transports.size(), 2u);
    SetCurrentStep(Step::kTransportSelection);
  } else {
    SetCurrentStep(Step::kErrorNoAvailableTransports);
  }
}

void AuthenticatorRequestDialogModel::StartGuidedFlowForTransport(
    AuthenticatorTransport transport,
    bool pair_with_new_device_for_bluetooth_low_energy) {
  DCHECK(current_step() == Step::kTransportSelection ||
         current_step() == Step::kUsbInsertAndActivate ||
         current_step() == Step::kBleActivate ||
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
    case AuthenticatorTransport::kBluetoothLowEnergy: {
      Step next_step = ShouldShowBlePairingUI(
                           previously_paired_with_bluetooth_authenticator_,
                           pair_with_new_device_for_bluetooth_low_energy)
                           ? Step::kBlePairingBegin
                           : Step::kBleActivate;
      EnsureBleAdapterIsPoweredBeforeContinuingWithStep(next_step);
      break;
    }
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      EnsureBleAdapterIsPoweredBeforeContinuingWithStep(Step::kCableActivate);
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

  // The StartOver() logic does not work in combination with the Windows API.
  // Therefore do not show a retry button on any error sheet shown after the
  // Windows API call returns.
  request_may_start_over_ = false;

  // There is no AuthenticatorReference for the Windows authenticator, hence
  // directly call DispatchRequestAsyncInternal here.
  DispatchRequestAsyncInternal(
      transport_availability()->win_native_api_authenticator_id);

  HideDialog();
}

void AuthenticatorRequestDialogModel::StartPhonePairing() {
  DCHECK(qr_generator_key_);
  EnsureBleAdapterIsPoweredBeforeContinuingWithStep(Step::kQRCode);
}

void AuthenticatorRequestDialogModel::
    EnsureBleAdapterIsPoweredBeforeContinuingWithStep(Step next_step) {
  DCHECK(current_step() == Step::kTransportSelection ||
         current_step() == Step::kUsbInsertAndActivate ||
         current_step() == Step::kBleActivate ||
         current_step() == Step::kCableActivate ||
         current_step() == Step::kNotStarted);
  if (ble_adapter_is_powered()) {
    if (cable_extension_provided_) {
      did_cable_broadcast_ = true;
      RecordCableV1Event(CableV1Event::kBLEAlreadyPowered);
    }
    SetCurrentStep(next_step);
  } else {
    next_step_once_ble_powered_ = next_step;
    if (transport_availability()->can_power_on_ble_adapter) {
      if (cable_extension_provided_) {
        RecordCableV1Event(CableV1Event::kBLEAutoPower);
      }
      SetCurrentStep(Step::kBlePowerOnAutomatic);
    } else {
      if (cable_extension_provided_) {
        RecordCableV1Event(CableV1Event::kBLEManualPower);
      }
      SetCurrentStep(Step::kBlePowerOnManual);
    }
  }
}

void AuthenticatorRequestDialogModel::ContinueWithFlowAfterBleAdapterPowered() {
  DCHECK(current_step() == Step::kBlePowerOnManual ||
         current_step() == Step::kBlePowerOnAutomatic);
  DCHECK(ble_adapter_is_powered());
  DCHECK(next_step_once_ble_powered_.has_value());

  if (cable_extension_provided_) {
    did_cable_broadcast_ = true;
    RecordCableV1Event(CableV1Event::kTransmitting);
  }
  SetCurrentStep(*next_step_once_ble_powered_);
}

void AuthenticatorRequestDialogModel::PowerOnBleAdapter() {
  DCHECK_EQ(current_step(), Step::kBlePowerOnAutomatic);
  if (!bluetooth_adapter_power_on_callback_)
    return;

  bluetooth_adapter_power_on_callback_.Run();
}

void AuthenticatorRequestDialogModel::StartBleDiscovery() {
  DCHECK_EQ(current_step(), Step::kBlePairingBegin);
}

void AuthenticatorRequestDialogModel::InitiatePairingDevice(
    base::StringPiece authenticator_id) {
  DCHECK_EQ(current_step(), Step::kBleDeviceSelection);
  auto* selected_authenticator =
      ephemeral_state_.saved_authenticators_.GetAuthenticator(authenticator_id);
  DCHECK(selected_authenticator);
  ephemeral_state_.selected_authenticator_id_ = authenticator_id.as_string();

// For MacOS, Bluetooth pin pairing is done via system native UI, which is
// triggered by a write attempt to GATT characteristic. Thus, simply resume
// with WebAuthn request for MacOS.
#if defined(OS_MACOSX)
  SetCurrentStep(Step::kBleVerifying);
  DispatchRequestAsync(selected_authenticator);
#else
  if (selected_authenticator->requires_ble_pairing_pin) {
    SetCurrentStep(Step::kBlePinEntry);
    return;
  }
  ble_pairing_callback_.Run(
      *selected_authenticator_id(), base::nullopt,
      base::BindOnce(&AuthenticatorRequestDialogModel::OnPairingSuccess,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&AuthenticatorRequestDialogModel::OnPairingFailure,
                     weak_factory_.GetWeakPtr()));
  SetCurrentStep(Step::kBleVerifying);
#endif
}

void AuthenticatorRequestDialogModel::FinishPairingWithPin(
    const base::string16& pin) {
  DCHECK_EQ(current_step(), Step::kBlePinEntry);
  DCHECK(ephemeral_state_.selected_authenticator_id_);
  const auto* selected_authenticator =
      ephemeral_state_.saved_authenticators_.GetAuthenticator(
          *ephemeral_state_.selected_authenticator_id_);
  if (!selected_authenticator) {
    // TODO(hongjunchoi): Implement an error screen for error encountered when
    // pairing.
    SetCurrentStep(Step::kBleDeviceSelection);
    return;
  }

  DCHECK_EQ(device::FidoTransportProtocol::kBluetoothLowEnergy,
            selected_authenticator->transport);
  ble_pairing_callback_.Run(
      *ephemeral_state_.selected_authenticator_id_, base::UTF16ToUTF8(pin),
      base::BindOnce(&AuthenticatorRequestDialogModel::OnPairingSuccess,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&AuthenticatorRequestDialogModel::OnPairingFailure,
                     weak_factory_.GetWeakPtr()));
  SetCurrentStep(Step::kBleVerifying);
}

void AuthenticatorRequestDialogModel::OnPairingSuccess() {
  DCHECK_EQ(current_step(), Step::kBleVerifying);
  DCHECK(ephemeral_state_.selected_authenticator_id_);
  auto* authenticator = ephemeral_state_.saved_authenticators_.GetAuthenticator(
      *ephemeral_state_.selected_authenticator_id_);
  if (!authenticator)
    return;

  authenticator->is_paired = true;
  DCHECK(ble_device_paired_callback_);
  ble_device_paired_callback_.Run(*ephemeral_state_.selected_authenticator_id_);

  DispatchRequestAsync(authenticator);
}

void AuthenticatorRequestDialogModel::OnPairingFailure() {
  DCHECK_EQ(current_step(), Step::kBleVerifying);
  ephemeral_state_.selected_authenticator_id_.reset();
  SetCurrentStep(Step::kBleDeviceSelection);
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
  if (did_cable_broadcast_) {
    RecordCableV1Event(CableV1Event::kTimeout);
  }

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

void AuthenticatorRequestDialogModel::OnAuthenticatorStorageFull() {
  SetCurrentStep(Step::kStorageFull);
}

void AuthenticatorRequestDialogModel::OnUserConsentDenied() {
  SetCurrentStep(Step::kErrorInternalUnrecognized);
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

void AuthenticatorRequestDialogModel::SetBlePairingCallback(
    BlePairingCallback ble_pairing_callback) {
  ble_pairing_callback_ = ble_pairing_callback;
}

void AuthenticatorRequestDialogModel::SetBluetoothAdapterPowerOnCallback(
    base::RepeatingClosure bluetooth_adapter_power_on_callback) {
  bluetooth_adapter_power_on_callback_ = bluetooth_adapter_power_on_callback;
}

void AuthenticatorRequestDialogModel::UpdateAuthenticatorReferenceId(
    base::StringPiece old_authenticator_id,
    std::string new_authenticator_id) {
  // Bluetooth authenticator address may be changed during pairing process after
  // the user chose device to pair during device selection UI. Thus, change
  // |ephemeral_state_.selected_authenticator_id_| as well.
  if (ephemeral_state_.selected_authenticator_id_ &&
      *ephemeral_state_.selected_authenticator_id_ == old_authenticator_id)
    ephemeral_state_.selected_authenticator_id_ = new_authenticator_id;

  ephemeral_state_.saved_authenticators_.ChangeAuthenticatorId(
      old_authenticator_id, std::move(new_authenticator_id));
}

void AuthenticatorRequestDialogModel::SetBleDevicePairedCallback(
    BleDevicePairedCallback ble_device_paired_callback) {
  ble_device_paired_callback_ = std::move(ble_device_paired_callback);
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

  bool is_ble = *authenticator.AuthenticatorTransport() ==
                AuthenticatorTransport::kBluetoothLowEnergy;
  AuthenticatorReference authenticator_reference(
      authenticator.GetId(), authenticator.GetDisplayName(),
      *authenticator.AuthenticatorTransport(),
      is_ble && authenticator.IsInPairingMode(),
      is_ble && authenticator.IsPaired(),
      is_ble && authenticator.RequiresBlePairingPin());

  if (authenticator_reference.is_paired &&
      authenticator_reference.transport ==
          AuthenticatorTransport::kBluetoothLowEnergy) {
    DispatchRequestAsync(&authenticator_reference);
  }
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

void AuthenticatorRequestDialogModel::UpdateAuthenticatorReferencePairingMode(
    base::StringPiece authenticator_id,
    bool is_in_pairing_mode,
    base::string16 display_name) {
  ephemeral_state_.saved_authenticators_.ChangeAuthenticatorPairingMode(
      authenticator_id, is_in_pairing_mode, display_name);
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

void AuthenticatorRequestDialogModel::OnSuccess(
    AuthenticatorTransport transport) {
  if (transport == AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy &&
      cable_extension_provided_) {
    RecordCableV1Event(CableV1Event::kSuccess);
  }
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

void AuthenticatorRequestDialogModel::RequestAttestationPermission(
    base::OnceCallback<void(bool)> callback) {
  DCHECK(current_step_ != Step::kClosed);
  attestation_callback_ = std::move(callback);
  SetCurrentStep(Step::kAttestationPermissionRequest);
}

void AuthenticatorRequestDialogModel::set_cable_transport_info(
    bool cable_extension_provided,
    bool have_paired_phones,
    base::Optional<device::QRGeneratorKey> qr_generator_key) {
  cable_extension_provided_ = cable_extension_provided;
  have_paired_phones_ = have_paired_phones;
  qr_generator_key_ = std::move(qr_generator_key);
}
