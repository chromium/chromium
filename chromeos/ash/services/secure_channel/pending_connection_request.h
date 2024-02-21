// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_H_

#include <optional>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/secure_channel/client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/pending_connection_request_delegate.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-shared.h"

namespace ash::secure_channel {

// Encapsulates metadata for a pending request for a connection to a remote
// device. PendingConnectionRequest is templatized so that each derived class
// can specify its own error-handling for connection failures; for instance,
// some derived classes may choose to continue an ongoing connection attempt
// indefinitely, while others may choose to handle connection failures by giving
// up on the request entirely.
template <typename FailureDetailType>
class PendingConnectionRequest {
 public:
  // Extracts |request|'s ClientConnectionParameters. This function deletes
  // |request| as part of this process to ensure that it is no longer used after
  // extraction is complete.
  static std::unique_ptr<ClientConnectionParameters>
  ExtractClientConnectionParameters(
      std::unique_ptr<PendingConnectionRequest<FailureDetailType>> request) {
    return request->ExtractClientConnectionParameters();
  }

  PendingConnectionRequest(const PendingConnectionRequest&) = delete;
  PendingConnectionRequest& operator=(const PendingConnectionRequest&) = delete;

  virtual ~PendingConnectionRequest() = default;

  ConnectionPriority connection_priority() const {
    return connection_priority_;
  }

  // Handles a failed connection attempt. Derived classes may choose to stop
  // trying to connect after some number of failures.
  virtual void HandleConnectionFailure(FailureDetailType failure_detail) = 0;

  virtual void HandleBleDiscoveryStateChange(
      mojom::DiscoveryResult discovery_result,
      std::optional<mojom::DiscoveryErrorCode> potential_error_code) {}
  virtual void HandleNearbyConnectionChange(
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result) {}
  virtual void HandleSecureChannelChanged(
      mojom::SecureChannelState secure_channel_state) {}

  virtual const base::UnguessableToken& GetRequestId() const = 0;

 protected:
  PendingConnectionRequest(PendingConnectionRequestDelegate* delegate,
                           ConnectionPriority connection_priority)
      : delegate_(delegate), connection_priority_(connection_priority) {
    DCHECK(delegate_);
  }

  // Extracts the feature and ConnectionDelegate from this request.
  virtual std::unique_ptr<ClientConnectionParameters>
  ExtractClientConnectionParameters() = 0;

  void NotifyRequestFinishedWithoutConnection(
      PendingConnectionRequestDelegate::FailedConnectionReason reason) {
    delegate_->OnRequestFinishedWithoutConnection(GetRequestId(), reason);
  }

 private:
  raw_ptr<PendingConnectionRequestDelegate> delegate_;
  ConnectionPriority connection_priority_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PENDING_CONNECTION_REQUEST_H_
