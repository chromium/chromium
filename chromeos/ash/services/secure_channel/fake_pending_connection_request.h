// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_H_

#include <utility>
#include <vector>

#include "base/unguessable_token.h"
#include "chromeos/ash/services/secure_channel/pending_connection_request.h"

namespace ash::secure_channel {

class ClientConnectionParameters;
class PendingConnectionRequestDelegate;

// Fake PendingConnectionRequest implementation.
template <typename FailureDetailType>
class FakePendingConnectionRequest
    : public PendingConnectionRequest<FailureDetailType> {
 public:
  FakePendingConnectionRequest(PendingConnectionRequestDelegate* delegate,
                               ConnectionPriority connection_priority,
                               bool notify_on_failure = false)
      : PendingConnectionRequest<FailureDetailType>(delegate,
                                                    connection_priority),
        id_(base::UnguessableToken::Create()),
        notify_on_failure_(notify_on_failure) {}

  FakePendingConnectionRequest(const FakePendingConnectionRequest&) = delete;
  FakePendingConnectionRequest& operator=(const FakePendingConnectionRequest&) =
      delete;

  ~FakePendingConnectionRequest() override = default;

  const std::vector<FailureDetailType>& handled_failure_details() const {
    return handled_failure_details_;
  }

  void set_client_data_for_extraction(
      std::unique_ptr<ClientConnectionParameters> client_data_for_extraction) {
    client_data_for_extraction_ = std::move(client_data_for_extraction);
  }

  // PendingConnectionRequest<FailureDetailType>:
  const base::UnguessableToken& GetRequestId() const override { return id_; }

  // Make NotifyRequestFinishedWithoutConnection() public for testing.
  using PendingConnectionRequest<
      FailureDetailType>::NotifyRequestFinishedWithoutConnection;

 private:
  // PendingConnectionRequest<FailureDetailType>:
  void HandleConnectionFailure(FailureDetailType failure_detail) override {
    handled_failure_details_.push_back(failure_detail);
    if (notify_on_failure_) {
      NotifyRequestFinishedWithoutConnection(
          PendingConnectionRequestDelegate::FailedConnectionReason::
              kRequestFailed);
    }
  }

  std::unique_ptr<ClientConnectionParameters>
  ExtractClientConnectionParameters() override {
    DCHECK(client_data_for_extraction_);
    return std::move(client_data_for_extraction_);
  }

  const base::UnguessableToken id_;

  std::vector<FailureDetailType> handled_failure_details_;

  std::unique_ptr<ClientConnectionParameters> client_data_for_extraction_;

  bool notify_on_failure_ = false;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_H_
