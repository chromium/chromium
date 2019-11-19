// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/ota_activator_impl.h"

#include <sstream>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_activation_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "url/gurl.h"

namespace chromeos {

namespace cellular_setup {

namespace {

OtaActivatorImpl::Factory* g_test_factory = nullptr;

void OnModemResetError(const std::string& error_name,
                       const std::string& error_message) {
  NET_LOG(ERROR) << "ShillDeviceClient::Reset() failed. " << error_name << ": "
                 << error_message << ".";
}

void OnNetworkConnectionError(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  NET_LOG(ERROR) << "ConnectToNetwork() failed. Error name: " << error_name;
}

}  // namespace

// static
std::unique_ptr<OtaActivator> OtaActivatorImpl::Factory::Create(
    mojo::PendingRemote<mojom::ActivationDelegate> activation_delegate,
    base::OnceClosure on_finished_callback,
    NetworkStateHandler* network_state_handler,
    NetworkConnectionHandler* network_connection_handler,
    NetworkActivationHandler* network_activation_handler,
    scoped_refptr<base::TaskRunner> task_runner) {
  if (g_test_factory) {
    return g_test_factory->BuildInstance(
        std::move(activation_delegate), std::move(on_finished_callback),
        network_state_handler, network_connection_handler,
        network_activation_handler, task_runner);
  }

  return base::WrapUnique(new OtaActivatorImpl(
      std::move(activation_delegate), std::move(on_finished_callback),
      network_state_handler, network_connection_handler,
      network_activation_handler, task_runner));
}

// static
void OtaActivatorImpl::Factory::SetFactoryForTesting(Factory* test_factory) {
  g_test_factory = test_factory;
}

OtaActivatorImpl::Factory::~Factory() = default;

OtaActivatorImpl::OtaActivatorImpl(
    mojo::PendingRemote<mojom::ActivationDelegate> activation_delegate,
    base::OnceClosure on_finished_callback,
    NetworkStateHandler* network_state_handler,
    NetworkConnectionHandler* network_connection_handler,
    NetworkActivationHandler* network_activation_handler,
    scoped_refptr<base::TaskRunner> task_runner)
    : OtaActivator(std::move(on_finished_callback)),
      activation_delegate_(std::move(activation_delegate)),
      network_state_handler_(network_state_handler),
      network_connection_handler_(network_connection_handler),
      network_activation_handler_(network_activation_handler) {
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&OtaActivatorImpl::StartActivation,
                                       weak_ptr_factory_.GetWeakPtr()));
}

OtaActivatorImpl::~OtaActivatorImpl() {
  // If this object is being deleted but it never finished the flow, consider
  // this a failure.
  if (state_ != State::kFinished)
    FinishActivationAttempt(mojom::ActivationResult::kFailedToActivate);
}

void OtaActivatorImpl::OnCarrierPortalStatusChange(
    mojom::CarrierPortalStatus status) {
  if (last_carrier_portal_status_) {
    NET_LOG(USER) << "OtaActivatorImpl: Carrier portal status updated. "
                  << *last_carrier_portal_status_ << " => " << status;
  } else {
    NET_LOG(USER) << "OtaActivatorImpl: Carrier portal status updated. "
                  << "Status: " << status;
  }

  last_carrier_portal_status_ = status;
  AttemptNextActivationStep();
}

void OtaActivatorImpl::NetworkListChanged() {
  AttemptNextActivationStep();
}

void OtaActivatorImpl::DeviceListChanged() {
  AttemptNextActivationStep();
}

void OtaActivatorImpl::NetworkPropertiesUpdated(const NetworkState* network) {
  AttemptNextActivationStep();
}

void OtaActivatorImpl::DevicePropertiesUpdated(const DeviceState* device) {
  AttemptNextActivationStep();
}

void OtaActivatorImpl::OnShuttingDown() {
  // |network_state_handler_| is shutting down before activation was able to
  // complete.
  FinishActivationAttempt(mojom::ActivationResult::kFailedToActivate);
}

const DeviceState* OtaActivatorImpl::GetCellularDeviceState() const {
  return network_state_handler_->GetDeviceStateByType(
      NetworkTypePattern::Cellular());
}

const NetworkState* OtaActivatorImpl::GetCellularNetworkState() const {
  // Note: Chrome OS only supports up to one Cellular network at a time. Other
  // configurations (e.g., a USB modem dongle in addition to an integrated SIM)
  // are not supported.
  return network_state_handler_->FirstNetworkByType(
      NetworkTypePattern::Cellular());
}

void OtaActivatorImpl::StartActivation() {
  network_state_handler_->AddObserver(this, FROM_HERE);

  // If |activation_delegate_| becomes disconnected, the activation request is
  // considered canceled.
  activation_delegate_.set_disconnect_handler(base::BindOnce(
      &OtaActivatorImpl::FinishActivationAttempt, base::Unretained(this),
      mojom::ActivationResult::kFailedToActivate));

  ChangeStateAndAttemptNextStep(State::kWaitingForValidSimToBecomePresent);
}

void OtaActivatorImpl::ChangeStateAndAttemptNextStep(State state) {
  DCHECK_NE(state, state_);
  NET_LOG(DEBUG) << "OtaActivatorImpl: " << state_ << " => " << state << ".";
  state_ = state;
  AttemptNextActivationStep();
}

void OtaActivatorImpl::AttemptNextActivationStep() {
  switch (state_) {
    case State::kNotYetStarted:
      // The flow either has not yet started; nothing to do.
      break;
    case State::kWaitingForValidSimToBecomePresent:
      AttemptToDiscoverSim();
      break;
    case State::kWaitingForCellularConnection:
      AttemptConnectionToCellularNetwork();
      break;
    case State::kWaitingForCellularPayment:
      AttemptToSendMetadataToDelegate();
      break;
    case State::kWaitingForActivation:
      AttemptToCompleteActivation();
      break;
    case State::kFinished:
      InvokeOnFinishedCallback();
      break;
  }
}

void OtaActivatorImpl::FinishActivationAttempt(
    mojom::ActivationResult activation_result) {
  DCHECK(network_state_handler_);
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  network_state_handler_ = nullptr;

  NET_LOG(EVENT) << "Finished attempt with result " << activation_result << ".";

  if (activation_delegate_)
    activation_delegate_->OnActivationFinished(activation_result);

  ChangeStateAndAttemptNextStep(State::kFinished);
}

void OtaActivatorImpl::AttemptToDiscoverSim() {
  DCHECK(state_ == State::kWaitingForValidSimToBecomePresent);

  const DeviceState* cellular_device = GetCellularDeviceState();

  // If the Cellular device is not present, either this machine does not support
  // cellular connections or the modem on the device is in the process of
  // restarting.
  if (!cellular_device)
    return;

  // If no SIM card is present, it may be due to the fact that some devices do
  // not have hardware support for determining whether a SIM has been inserted.
  // Restart the modem to see if the SIM is detected when the modem powers back
  // on.
  if (!cellular_device->sim_present()) {
    NET_LOG(DEBUG) << "No SIM detected; restarting modem.";
    ShillDeviceClient::Get()->Reset(
        dbus::ObjectPath(cellular_device->path()),
        base::Bind(&OtaActivatorImpl::AttemptNextActivationStep,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&OnModemResetError));
    return;
  }

  // The device must have the properties required for the activation flow;
  // namely, the operator name, IMEI, and MDN must be available. Return
  // and wait to see if DevicePropertiesUpdated() is invoked with valid
  // properties.
  if (cellular_device->operator_name().empty() ||
      cellular_device->imei().empty() || cellular_device->mdn().empty()) {
    NET_LOG(DEBUG) << "Insufficient activation data: "
                   << "Operator name: " << cellular_device->operator_name()
                   << ", IMEI: " << cellular_device->imei() << ", "
                   << "MDN: " << cellular_device->mdn();
    return;
  }

  ChangeStateAndAttemptNextStep(State::kWaitingForCellularConnection);
}

void OtaActivatorImpl::AttemptConnectionToCellularNetwork() {
  DCHECK(state_ == State::kWaitingForCellularConnection);

  const NetworkState* cellular_network = GetCellularNetworkState();

  // There is no cellular network to be connected; return early and wait for
  // NetworkListChanged() to be called if/when one becomes available.
  if (!cellular_network)
    return;

  // If the network is already activated, there is no need to complete the rest
  // of the flow.
  if (cellular_network->activation_state() ==
      shill::kActivationStateActivated) {
    FinishActivationAttempt(mojom::ActivationResult::kAlreadyActivated);
    return;
  }

  // The network must have payment information; at minimum, a payment URL is
  // required in order to contact the carrier payment portal. Return and wait to
  // see if NetworkPropertiesUpdated() is invoked with valid properties.
  if (cellular_network->payment_url().empty()) {
    NET_LOG(DEBUG) << "Insufficient activation data: "
                   << "Payment URL: " << cellular_network->payment_url() << ", "
                   << "Post Data: " << cellular_network->payment_post_data();
    return;
  }

  // The network is disconnected; trigger a connection and wait for
  // NetworkPropertiesUpdated() to be called when the network connects.
  if (!cellular_network->IsConnectingOrConnected()) {
    network_connection_handler_->ConnectToNetwork(
        cellular_network->path(), base::DoNothing(),
        base::Bind(&OnNetworkConnectionError), false /* check_error_state */,
        ConnectCallbackMode::ON_STARTED);
    return;
  }

  // The network is connecting; return early and wait for
  // NetworkPropertiesUpdated() to be called if/when the network connects.
  if (cellular_network->IsConnectingState())
    return;

  ChangeStateAndAttemptNextStep(State::kWaitingForCellularPayment);
}

void OtaActivatorImpl::AttemptToSendMetadataToDelegate() {
  DCHECK(state_ == State::kWaitingForCellularPayment);

  // Metadata should only be sent to the delegate once.
  if (!has_sent_metadata_) {
    has_sent_metadata_ = true;

    const DeviceState* cellular_device = GetCellularDeviceState();
    const NetworkState* cellular_network = GetCellularNetworkState();

    NET_LOG(DEBUG) << "Sending CellularMetadata. "
                   << "Payment URL: " << cellular_network->payment_url() << ", "
                   << "Post data: " << cellular_network->payment_post_data()
                   << ", Carrier: " << cellular_device->operator_name() << ", "
                   << "MEID: " << cellular_device->meid() << ", "
                   << "IMEI: " << cellular_device->imei() << ", "
                   << "MDN: " << cellular_device->mdn();
    activation_delegate_->OnActivationStarted(mojom::CellularMetadata::New(
        GURL(cellular_network->payment_url()),
        cellular_network->payment_post_data(), cellular_device->operator_name(),
        cellular_device->meid(), cellular_device->imei(),
        cellular_device->mdn()));
  }

  // The user must successfully pay via the carrier portal before continuing.
  if (last_carrier_portal_status_ !=
      mojom::CarrierPortalStatus::kPortalLoadedAndUserCompletedPayment) {
    return;
  }

  ChangeStateAndAttemptNextStep(State::kWaitingForActivation);
}

void OtaActivatorImpl::AttemptToCompleteActivation() {
  DCHECK(state_ == State::kWaitingForActivation);

  // CompleteActivation() should only be called once.
  if (has_called_complete_activation_)
    return;
  has_called_complete_activation_ = true;

  network_activation_handler_->CompleteActivation(
      GetCellularNetworkState()->path(),
      base::Bind(&OtaActivatorImpl::FinishActivationAttempt,
                 weak_ptr_factory_.GetWeakPtr(),
                 mojom::ActivationResult::kSuccessfullyStartedActivation),
      base::Bind(&OtaActivatorImpl::OnCompleteActivationError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void OtaActivatorImpl::OnCompleteActivationError(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  NET_LOG(ERROR) << "CompleteActivation() failed. Error name: " << error_name;
  FinishActivationAttempt(mojom::ActivationResult::kFailedToActivate);
}

void OtaActivatorImpl::FlushForTesting() {
  if (activation_delegate_)
    activation_delegate_.FlushForTesting();
}

std::ostream& operator<<(std::ostream& stream,
                         const OtaActivatorImpl::State& state) {
  switch (state) {
    case OtaActivatorImpl::State::kNotYetStarted:
      stream << "[Not yet started]";
      break;
    case OtaActivatorImpl::State::kWaitingForValidSimToBecomePresent:
      stream << "[Waiting for SIM to become present]";
      break;
    case OtaActivatorImpl::State::kWaitingForCellularConnection:
      stream << "[Waiting for connected cellular network]";
      break;
    case OtaActivatorImpl::State::kWaitingForCellularPayment:
      stream << "[Waiting cellular payment payment to complete]";
      break;
    case OtaActivatorImpl::State::kWaitingForActivation:
      stream << "[Waiting for Shill activation to complete]";
      break;
    case OtaActivatorImpl::State::kFinished:
      stream << "[Finished]";
      break;
  }
  return stream;
}

}  // namespace cellular_setup

}  // namespace chromeos
