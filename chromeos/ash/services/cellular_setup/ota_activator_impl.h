// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_OTA_ACTIVATOR_IMPL_H_
#define CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_OTA_ACTIVATOR_IMPL_H_

#include <memory>
#include <ostream>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/services/cellular_setup/ota_activator.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class NetworkActivationHandler;
class NetworkConnectionHandler;
class NetworkState;
class NetworkStateHandler;

namespace cellular_setup {

// Concrete OtaActivator implementation. This class activates a SIM using the
// following steps:
//   (1) Find a valid SIM in the device. In this context, a SIM is only valid if
//       it is present in the machine and has an associated carrier, MEID, IMEI,
//       and MDN. If a valid SIM is not present, this class reboots the modem to
//       see if the SIM can be detected after a restart.
//   (2) Ensure an eligible cellular connection is active. In this context, a
//       cellular network is only eligible for activation if it has associated
//       payment metadata which can be provided to the carrier portal. If such
//       a network is available, this class connects to that network.
//   (3) Wait for carrier payment to complete. This class impelments
//       CarrierPortalHandler to receive updates about the payment status.
//   (4) Complete activation via Shill.
class OtaActivatorImpl : public OtaActivator,
                         public NetworkStateHandlerObserver {
 public:
  class Factory {
   public:
    static std::unique_ptr<OtaActivator> Create(
        mojo::PendingRemote<mojom::ActivationDelegate> activation_delegate,
        base::OnceClosure on_finished_callback,
        NetworkStateHandler* network_state_handler,
        NetworkConnectionHandler* network_connection_handler,
        NetworkActivationHandler* network_activation_handler,
        scoped_refptr<base::TaskRunner> task_runner =
            base::SingleThreadTaskRunner::GetCurrentDefault());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<OtaActivator> CreateInstance(
        mojo::PendingRemote<mojom::ActivationDelegate> activation_delegate,
        base::OnceClosure on_finished_callback,
        NetworkStateHandler* network_state_handler,
        NetworkConnectionHandler* network_connection_handler,
        NetworkActivationHandler* network_activation_handler,
        scoped_refptr<base::TaskRunner> task_runner) = 0;
  };

  OtaActivatorImpl(const OtaActivatorImpl&) = delete;
  OtaActivatorImpl& operator=(const OtaActivatorImpl&) = delete;

  ~OtaActivatorImpl() override;

 private:
  // Delay for first connection retry attempt. Delay doubles for every
  // subsequent attempt.
  static const base::TimeDelta kConnectRetryDelay;
  // Maximum number of connection retry attempts.
  static const size_t kMaxConnectRetryAttempt;

  friend class CellularSetupOtaActivatorImplTest;

  enum class State {
    kNotYetStarted,
    kWaitingForValidSimToBecomePresent,
    kWaitingForCellularConnection,
    kWaitingForCellularPayment,
    kWaitingForActivation,
    kFinished
  };
  friend std::ostream& operator<<(std::ostream& stream, const State& state);

  OtaActivatorImpl(
      mojo::PendingRemote<mojom::ActivationDelegate> activation_delegate,
      base::OnceClosure on_finished_callback,
      NetworkStateHandler* network_state_handler,
      NetworkConnectionHandler* network_connection_handler,
      NetworkActivationHandler* network_activation_handler,
      scoped_refptr<base::TaskRunner> task_runner);

  // mojom::CarrierPortalHandler:
  void OnCarrierPortalStatusChange(mojom::CarrierPortalStatus status) override;

  // NetworkStateHandlerObserver:
  void NetworkListChanged() override;
  void DeviceListChanged() override;
  void NetworkPropertiesUpdated(const NetworkState* network) override;
  void DevicePropertiesUpdated(const DeviceState* device) override;
  void OnShuttingDown() override;

  const DeviceState* GetCellularDeviceState() const;
  const NetworkState* GetCellularNetworkState() const;

  void StartActivation();
  void ChangeStateAndAttemptNextStep(State state);
  void AttemptNextActivationStep();
  void FinishActivationAttempt(mojom::ActivationResult activation_result);

  void AttemptToDiscoverSim();
  void AttemptConnectionToCellularNetwork();
  void AttemptToSendMetadataToDelegate();
  void AttemptToCompleteActivation();

  void OnCompleteActivationError(const std::string& error_name);
  void OnNetworkConnectionError(const std::string& error_name);

  void FlushForTesting();

  mojo::Remote<mojom::ActivationDelegate> activation_delegate_;
  raw_ptr<NetworkStateHandler> network_state_handler_;
  raw_ptr<NetworkConnectionHandler> network_connection_handler_;
  raw_ptr<NetworkActivationHandler> network_activation_handler_;

  NetworkStateHandlerScopedObservation network_state_handler_observer_{this};

  State state_ = State::kNotYetStarted;
  std::optional<mojom::CarrierPortalStatus> last_carrier_portal_status_;
  std::string iccid_;
  bool has_sent_metadata_ = false;
  bool has_called_complete_activation_ = false;
  base::OneShotTimer connect_retry_timer_;
  size_t connect_retry_attempts_ = 0;

  base::WeakPtrFactory<OtaActivatorImpl> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& stream,
                         const OtaActivatorImpl::State& state);

}  // namespace cellular_setup
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_OTA_ACTIVATOR_IMPL_H_
