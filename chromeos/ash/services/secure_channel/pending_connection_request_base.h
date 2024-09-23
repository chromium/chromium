// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_BASE_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_BASE_H_

#include <memory>
#include <optional>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/pending_connection_request.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace ash::secure_channel {

// Encapsulates metadata for a pending request for a connection to a remote
// device. Every PendingConnectionRequestBase starts out active (i.e., there
// exists an ongoing attempt to create a connection). The client of this class
// can cancel an active attempt by disconnecting the
// mojo::Remote<ConnectionDelegate> passed PendingConnectionRequestBase's
// constructor; likewise, a PendingConnectionRequestBase can become inactive due
// to connection failures.
//
// Each connection type should implement its own pending request class deriving
// from PendingConnectionRequestBase.
template <typename FailureDetailType>
class PendingConnectionRequestBase
    : public PendingConnectionRequest<FailureDetailType>,
      public ClientConnectionParameters::Observer {
 public:
  PendingConnectionRequestBase(const PendingConnectionRequestBase&) = delete;
  PendingConnectionRequestBase& operator=(const PendingConnectionRequestBase&) =
      delete;

  ~PendingConnectionRequestBase() override {
    if (client_connection_parameters_)
      client_connection_parameters_->RemoveObserver(this);
  }

  // PendingConnectionRequest<FailureDetailType>:
  const base::UnguessableToken& GetRequestId() const override {
    return client_connection_parameters_->id();
  }

 protected:
  PendingConnectionRequestBase(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority,
      const std::string& readable_request_type_for_logging,
      PendingConnectionRequestDelegate* delegate)
      : PendingConnectionRequest<FailureDetailType>(delegate,
                                                    connection_priority),
        client_connection_parameters_(std::move(client_connection_parameters)),
        readable_request_type_for_logging_(readable_request_type_for_logging) {
    client_connection_parameters_->AddObserver(this);
  }

  // Derived classes should invoke this function if they would like to give up
  // on the request due to connection failures.
  void StopRequestDueToConnectionFailures(
      mojom::ConnectionAttemptFailureReason failure_reason) {
    if (has_finished_without_connection_) {
      PA_LOG(WARNING) << "PendingConnectionRequest::"
                      << "StopRequestDueToConnectionFailures() invoked after "
                      << "request had already finished without a connection.";
      return;
    }

    client_connection_parameters_->SetConnectionAttemptFailed(failure_reason);

    OnFinishedWithoutConnection(PendingConnectionRequestDelegate::
                                    FailedConnectionReason::kRequestFailed);
  }

  void UpdateBleDiscoveryState(
      mojom::DiscoveryResult discovery_result,
      std::optional<mojom::DiscoveryErrorCode> potential_error_code) {
    client_connection_parameters_->SetBleDiscoveryState(discovery_result,
                                                        potential_error_code);
  }

  void UpdateNearbyConnectionChange(mojom::NearbyConnectionStep step,
                                    mojom::NearbyConnectionStepResult result) {
    client_connection_parameters_->SetNearbyConnectionState(step, result);
  }

  void UpdateSecureChannelChange(
      mojom::SecureChannelState secure_channel_state) {
    client_connection_parameters_->SetSecureChannelAuthenticationState(
        secure_channel_state);
  }

 private:
  // Make NotifyRequestFinishedWithoutConnection() inaccessible to derived
  // types, which should use StopRequestDueToConnectionFailures() instead.
  using PendingConnectionRequest<
      FailureDetailType>::NotifyRequestFinishedWithoutConnection;

  // PendingConnectionRequest<FailureDetailType>:
  std::unique_ptr<ClientConnectionParameters>
  ExtractClientConnectionParameters() override {
    client_connection_parameters_->RemoveObserver(this);
    return std::move(client_connection_parameters_);
  }

  // ClientConnectionParameters::Observer
  void OnConnectionRequestCanceled() override {
    OnFinishedWithoutConnection(
        PendingConnectionRequestDelegate::FailedConnectionReason::
            kRequestCanceledByClient);
  }

  void OnFinishedWithoutConnection(
      PendingConnectionRequestDelegate::FailedConnectionReason reason) {
    DCHECK(!has_finished_without_connection_);
    has_finished_without_connection_ = true;

    PA_LOG(VERBOSE)
        << "Request finished without connection; notifying delegate. "
        << "Request type: \"" << readable_request_type_for_logging_
        << "\", Reason: " << reason
        << ", Client parameters: " << *client_connection_parameters_;
    NotifyRequestFinishedWithoutConnection(reason);
  }

  std::unique_ptr<ClientConnectionParameters> client_connection_parameters_;
  const std::string readable_request_type_for_logging_;

  bool has_finished_without_connection_ = false;

  base::WeakPtrFactory<PendingConnectionRequestBase> weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_BASE_H_
