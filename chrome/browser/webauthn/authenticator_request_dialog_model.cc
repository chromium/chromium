// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace {

// Attempts to auto-select the most likely transport that will be used to
// service this request, or returns base::nullopt if unsure.
base::Optional<device::FidoTransportProtocol> SelectMostLikelyTransport(
    const device::FidoRequestHandlerBase::TransportAvailabilityInfo&
        transport_availability,
    base::Optional<device::FidoTransportProtocol> last_used_transport) {
  base::flat_set<AuthenticatorTransport> candidate_transports(
      transport_availability.available_transports);

  // As an exception, we can tell in advance if using Touch Id will succeed. If
  // yes, always auto-select that transport over all other considerations for
  // GetAssertion operations; and de-select it if it will not work.
  if (transport_availability.request_type ==
          device::FidoRequestHandlerBase::RequestType::kGetAssertion &&
      base::ContainsKey(candidate_transports,
                        device::FidoTransportProtocol::kInternal)) {
    // For GetAssertion requests, auto advance to Touch ID if the keychain
    // contains one of the allowedCredentials.
    if (transport_availability.has_recognized_mac_touch_id_credential)
      return device::FidoTransportProtocol::kInternal;
  }

  // If caBLE is listed as one of the allowed transports, it indicates that the
  // RP has bothered to supply the |cable_extension|. Respect that and always
  // select caBLE in that case for GetAssertion operations.
  if (transport_availability.request_type ==
          device::FidoRequestHandlerBase::RequestType::kGetAssertion &&
      base::ContainsKey(
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
      base::ContainsKey(candidate_transports, *last_used_transport) &&
      *last_used_transport != device::FidoTransportProtocol::kInternal) {
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

AuthenticatorRequestDialogModel::AuthenticatorRequestDialogModel()
    : weak_factory_(this) {}

AuthenticatorRequestDialogModel::~AuthenticatorRequestDialogModel() {
  for (auto& observer : observers_)
    observer.OnModelDestroyed();
}

void AuthenticatorRequestDialogModel::SetCurrentStep(Step step) {
  current_step_ = step;
  for (auto& observer : observers_)
    observer.OnStepTransition();
}

void AuthenticatorRequestDialogModel::StartFlow(
    TransportAvailabilityInfo transport_availability,
    base::Optional<device::FidoTransportProtocol> last_used_transport) {
  DCHECK_EQ(current_step(), Step::kNotStarted);

  transport_availability_ = std::move(transport_availability);
  last_used_transport_ = last_used_transport;
  for (const auto transport : transport_availability_.available_transports) {
    available_transports_.emplace_back(transport);
  }

  StartGuidedFlowForMostLikelyTransportOrShowTransportSelection();
}

void AuthenticatorRequestDialogModel::
    StartGuidedFlowForMostLikelyTransportOrShowTransportSelection() {
  DCHECK(current_step() == Step::kWelcomeScreen ||
         current_step() == Step::kNotStarted);
  auto most_likely_transport =
      SelectMostLikelyTransport(transport_availability_, last_used_transport_);
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
    AuthenticatorTransport transport) {
  DCHECK(current_step() == Step::kTransportSelection ||
         current_step() == Step::kWelcomeScreen ||
         current_step() == Step::kUsbInsertAndActivate ||
         current_step() == Step::kTouchId ||
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
    case AuthenticatorTransport::kBluetoothLowEnergy:
      EnsureBleAdapterIsPoweredBeforeContinuingWithStep(Step::kBleActivate);
      break;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      EnsureBleAdapterIsPoweredBeforeContinuingWithStep(Step::kCableActivate);
      break;
    default:
      break;
  }
}

void AuthenticatorRequestDialogModel::
    EnsureBleAdapterIsPoweredBeforeContinuingWithStep(Step next_step) {
  DCHECK(current_step() == Step::kTransportSelection ||
         current_step() == Step::kWelcomeScreen ||
         current_step() == Step::kUsbInsertAndActivate ||
         current_step() == Step::kTouchId ||
         current_step() == Step::kBleActivate ||
         current_step() == Step::kCableActivate ||
         current_step() == Step::kNotStarted);
  if (ble_adapter_is_powered()) {
    SetCurrentStep(next_step);
  } else {
    next_step_once_ble_powered_ = next_step;
    if (transport_availability()->can_power_on_ble_adapter)
      SetCurrentStep(Step::kBlePowerOnAutomatic);
    else
      SetCurrentStep(Step::kBlePowerOnManual);
  }
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

void AuthenticatorRequestDialogModel::StartBleDiscovery() {
  DCHECK_EQ(current_step(), Step::kBlePairingBegin);
}

void AuthenticatorRequestDialogModel::InitiatePairingDevice(
    base::StringPiece authenticator_id) {
  DCHECK_EQ(current_step(), Step::kBleDeviceSelection);
  DCHECK(saved_authenticators_.GetAuthenticator(authenticator_id));
  selected_authenticator_id_ = authenticator_id.as_string();
  SetCurrentStep(Step::kBlePinEntry);
}

void AuthenticatorRequestDialogModel::FinishPairingWithPin(
    const base::string16& pin) {
  DCHECK_EQ(current_step(), Step::kBlePinEntry);
  const auto* selected_authenticator =
      saved_authenticators_.GetAuthenticator(selected_authenticator_id_);
  if (!selected_authenticator) {
    // TODO(hongjunchoi): Implement an error screen for error encountered when
    // pairing.
    SetCurrentStep(Step::kBleDeviceSelection);
    return;
  }

  DCHECK_EQ(device::FidoTransportProtocol::kBluetoothLowEnergy,
            selected_authenticator->transport());
  ble_pairing_callback_.Run(
      selected_authenticator_id_, base::UTF16ToUTF8(pin),
      base::BindOnce(&AuthenticatorRequestDialogModel::OnPairingSuccess,
                     weak_factory_.GetWeakPtr(), selected_authenticator_id_),
      base::BindOnce(&AuthenticatorRequestDialogModel::OnPairingFailure,
                     weak_factory_.GetWeakPtr()));
  SetCurrentStep(Step::kBleVerifying);
}

void AuthenticatorRequestDialogModel::OnPairingSuccess(
    base::StringPiece authenticator_id) {
  DCHECK_EQ(current_step(), Step::kBleVerifying);
  auto* authenticator =
      saved_authenticators_.GetAuthenticator(authenticator_id);
  if (authenticator)
    return;

  DispatchRequestAsync(authenticator, base::TimeDelta());
}

void AuthenticatorRequestDialogModel::OnPairingFailure() {
  DCHECK_EQ(current_step(), Step::kBleVerifying);
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

  SetCurrentStep(Step::kTouchId);

  auto& authenticators = saved_authenticators_.authenticator_list();
  auto touch_id_authenticator_it =
      std::find_if(authenticators.begin(), authenticators.end(),
                   [](const auto& authenticator) {
                     return authenticator.transport() ==
                            device::FidoTransportProtocol::kInternal;
                   });

  if (touch_id_authenticator_it == authenticators.end())
    return;

  static base::TimeDelta kTouchIdDispatchDelay =
      base::TimeDelta::FromMilliseconds(1250);
  DispatchRequestAsync(&*touch_id_authenticator_it, kTouchIdDispatchDelay);
}

void AuthenticatorRequestDialogModel::Cancel() {
  if (is_request_complete()) {
    SetCurrentStep(Step::kClosed);
    return;
  }

  for (auto& observer : observers_)
    observer.OnCancelRequest();
}

void AuthenticatorRequestDialogModel::Back() {
  SetCurrentStep(Step::kTransportSelection);
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
  DCHECK_NE(current_step(), Step::kClosed);
  if (is_showing_post_mortem())
    return;
  SetCurrentStep(Step::kClosed);
}

void AuthenticatorRequestDialogModel::OnRequestTimeout() {
  DCHECK(!is_request_complete());
  SetCurrentStep(Step::kPostMortemTimedOut);
}

void AuthenticatorRequestDialogModel::OnActivatedKeyNotRegistered() {
  DCHECK(!is_request_complete());
  SetCurrentStep(Step::kPostMortemKeyNotRegistered);
}

void AuthenticatorRequestDialogModel::OnActivatedKeyAlreadyRegistered() {
  DCHECK(!is_request_complete());
  SetCurrentStep(Step::kPostMortemKeyAlreadyRegistered);
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
  saved_authenticators_.ChangeAuthenticatorId(old_authenticator_id,
                                              std::move(new_authenticator_id));
}

void AuthenticatorRequestDialogModel::AddAuthenticator(
    const device::FidoAuthenticator& authenticator) {
  saved_authenticators_.AddAuthenticator(AuthenticatorReference(
      authenticator.GetId(), authenticator.GetDisplayName(),
      authenticator.AuthenticatorTransport(), authenticator.IsInPairingMode()));
}

void AuthenticatorRequestDialogModel::RemoveAuthenticator(
    base::StringPiece authenticator_id) {
  saved_authenticators_.RemoveAuthenticator(authenticator_id);
}

void AuthenticatorRequestDialogModel::DispatchRequestAsync(
    AuthenticatorReference* authenticator,
    base::TimeDelta delay) {
  if (!request_callback_)
    return;

  // Dispatching to the same authenticator twice may result in unexpected
  // behavior.
  if (authenticator->dispatched())
    return;

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(request_callback_, authenticator->authenticator_id()),
      delay);
  authenticator->SetDispatched(true);
}

void AuthenticatorRequestDialogModel::UpdateAuthenticatorReferencePairingMode(
    base::StringPiece authenticator_id,
    bool is_in_pairing_mode) {
  saved_authenticators_.ChangeAuthenticatorPairingMode(authenticator_id,
                                                       is_in_pairing_mode);
}

void AuthenticatorRequestDialogModel::SetSelectedAuthenticatorForTesting(
    AuthenticatorReference test_authenticator) {
  selected_authenticator_id_ = test_authenticator.authenticator_id();
  saved_authenticators_.AddAuthenticator(std::move(test_authenticator));
}

