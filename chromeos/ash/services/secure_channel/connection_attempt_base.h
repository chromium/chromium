// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_BASE_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_BASE_H_

#include <optional>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_clock.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/connect_to_device_operation.h"
#include "chromeos/ash/services/secure_channel/connection_attempt.h"
#include "chromeos/ash/services/secure_channel/connection_attempt_details.h"
#include "chromeos/ash/services/secure_channel/connection_details.h"
#include "chromeos/ash/services/secure_channel/pending_connection_request.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"

namespace ash::secure_channel {

// ConnectionAttempt implementation which stays active for as long as at least
// one of its requests has not yet completed. While a ConnectionAttemptBase is
// active, it starts one or more operations to connect to the device. If an
// operation succeeds in connecting, the ConnectionAttempt notifies its delegate
// of success.
//
// If an operation fails to connect, ConnectionAttemptBase alerts each of its
// PendingConnectionRequests of the failure to connect. Each request can
// decide to give up connecting due to the client canceling the request or
// due to handling too many failures of individual operations. A
// ConnectionAttemptBase alerts its delegate of a failure if all of its
// associated PendingConnectionRequests have given up trying to connect.
//
// When an operation fails but there still exist active requests,
// ConnectionAttempt simply starts up a new operation and retries the
// connection.
template <typename FailureDetailType>
class ConnectionAttemptBase : public ConnectionAttempt<FailureDetailType> {
 public:
  ConnectionAttemptBase(const ConnectionAttemptBase&) = delete;
  ConnectionAttemptBase& operator=(const ConnectionAttemptBase&) = delete;

 protected:
  ConnectionAttemptBase(
      ConnectionAttemptDelegate* delegate,
      const ConnectionAttemptDetails& connection_attempt_details,
      base::Clock* clock = base::DefaultClock::GetInstance())
      : ConnectionAttempt<FailureDetailType>(delegate,
                                             clock,
                                             connection_attempt_details) {}

  ~ConnectionAttemptBase() override {
    if (operation_)
      operation_->Cancel();
  }

  virtual std::unique_ptr<ConnectToDeviceOperation<FailureDetailType>>
  CreateConnectToDeviceOperation(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      typename ConnectToDeviceOperation<
          FailureDetailType>::ConnectionSuccessCallback success_callback,
      const typename ConnectToDeviceOperation<
          FailureDetailType>::ConnectionFailedCallback& failure_callback) = 0;

  void OnBleDiscoveryStateChanged(
      mojom::DiscoveryResult discovery_result,
      std::optional<mojom::DiscoveryErrorCode> potential_result) {
    for (auto it = id_to_request_map_.begin();
         it != id_to_request_map_.end();) {
      auto it_copy = it++;
      it_copy->second->HandleBleDiscoveryStateChange(discovery_result,
                                                     potential_result);
    }
  }
  void OnNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result) {
    for (auto it = id_to_request_map_.begin();
         it != id_to_request_map_.end();) {
      auto it_copy = it++;
      it_copy->second->HandleNearbyConnectionChange(step, result);
    }
  }
  void OnSecureChannelStateChanged(
      mojom::SecureChannelState secure_channel_state) {
    for (auto it = id_to_request_map_.begin();
         it != id_to_request_map_.end();) {
      auto it_copy = it++;
      it_copy->second->HandleSecureChannelChanged(secure_channel_state);
    }
  }

 private:
  // ConnectionAttempt<FailureDetailType>:
  void ProcessAddingNewConnectionRequest(
      std::unique_ptr<PendingConnectionRequest<FailureDetailType>> request)
      override {
    ConnectionPriority priority_before_add =
        GetHighestRemainingConnectionPriority();

    if (base::Contains(id_to_request_map_, request->GetRequestId())) {
      PA_LOG(ERROR) << "ConnectionAttemptBase::"
                    << "ProcessAddingNewConnectionRequest(): Processing "
                    << "request whose ID has already been processed.";
      NOTREACHED_IN_MIGRATION();
    }

    bool was_empty = id_to_request_map_.empty();
    id_to_request_map_[request->GetRequestId()] = std::move(request);

    // In the case that this ConnectionAttempt was just created and had not yet
    // received a request yet, start up its operation.
    if (was_empty) {
      operation_ = CreateConnectToDeviceOperation(
          this->connection_attempt_details().device_id_pair(),
          GetHighestRemainingConnectionPriority(),
          base::BindOnce(
              &ConnectionAttemptBase<
                  FailureDetailType>::OnConnectToDeviceOperationSuccess,
              weak_ptr_factory_.GetWeakPtr()),
          base::BindRepeating(
              &ConnectionAttemptBase<
                  FailureDetailType>::OnConnectToDeviceOperationFailure,
              weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    ConnectionPriority priority_after_add =
        GetHighestRemainingConnectionPriority();
    if (priority_before_add != priority_after_add)
      operation_->UpdateConnectionPriority(priority_after_add);
  }

  std::vector<std::unique_ptr<ClientConnectionParameters>>
  ExtractClientConnectionParameters() override {
    std::vector<std::unique_ptr<ClientConnectionParameters>> data_list;
    for (auto& map_entry : id_to_request_map_) {
      data_list.push_back(
          PendingConnectionRequest<FailureDetailType>::
              ExtractClientConnectionParameters(std::move(map_entry.second)));
    }
    return data_list;
  }

  // PendingConnectionRequestDelegate:
  void OnRequestFinishedWithoutConnection(
      const base::UnguessableToken& request_id,
      PendingConnectionRequestDelegate::FailedConnectionReason reason)
      override {
    ConnectionPriority priority_before_removal =
        GetHighestRemainingConnectionPriority();

    size_t removed_element_count = id_to_request_map_.erase(request_id);
    if (removed_element_count != 1) {
      DCHECK(removed_element_count == 0);
      PA_LOG(ERROR) << "ConnectionAttemptBase::"
                    << "OnRequestFinishedWithoutConnection(): Request "
                    << "finished, but it was missing from the map.";
    }

    ConnectionPriority priority_after_removal =
        GetHighestRemainingConnectionPriority();
    if (priority_before_removal != priority_after_removal)
      operation_->UpdateConnectionPriority(priority_after_removal);

    // If there are no longer any active entries, this attempt is finished.
    if (id_to_request_map_.empty())
      this->OnConnectionAttemptFinishedWithoutConnection();
  }

  void OnConnectToDeviceOperationSuccess(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
    DCHECK(operation_);
    operation_.reset();
    this->OnConnectionAttemptSucceeded(std::move(authenticated_channel));
  }

  void OnConnectToDeviceOperationFailure(FailureDetailType failure_detail) {
    // The call to HandleConnectionFailure() will generally remove the item from
    // the map, so we use a std::map instead of base::flat_map and an idiom that
    // allows us to safely remove items while iterating.
    for (auto it = id_to_request_map_.begin();
         it != id_to_request_map_.end();) {
      auto it_copy = it++;
      it_copy->second->HandleConnectionFailure(failure_detail);
    }
  }

  ConnectionPriority GetHighestRemainingConnectionPriority() {
    ConnectionPriority highest_priority = ConnectionPriority::kLow;
    for (const auto& map_entry : id_to_request_map_) {
      if (map_entry.second->connection_priority() > highest_priority)
        highest_priority = map_entry.second->connection_priority();
    }
    return highest_priority;
  }

  std::unique_ptr<ConnectToDeviceOperation<FailureDetailType>> operation_;
  std::map<base::UnguessableToken,
           std::unique_ptr<PendingConnectionRequest<FailureDetailType>>>
      id_to_request_map_;

  base::WeakPtrFactory<ConnectionAttemptBase<FailureDetailType>>
      weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_BASE_H_
