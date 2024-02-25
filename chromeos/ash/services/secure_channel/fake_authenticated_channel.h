// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_AUTHENTICATED_CHANNEL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_AUTHENTICATED_CHANNEL_H_

#include <string>
#include <tuple>
#include <vector>

#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/ash/services/secure_channel/register_payload_file_request.h"

namespace ash::secure_channel {

// Test AuthenticatedChannel implementation.
class FakeAuthenticatedChannel : public AuthenticatedChannel {
 public:
  FakeAuthenticatedChannel();

  FakeAuthenticatedChannel(const FakeAuthenticatedChannel&) = delete;
  FakeAuthenticatedChannel& operator=(const FakeAuthenticatedChannel&) = delete;

  ~FakeAuthenticatedChannel() override;

  std::vector<std::tuple<std::string, std::string, base::OnceClosure>>&
  sent_messages() {
    return sent_messages_;
  }

  const std::vector<RegisterPayloadFileRequest>&
  reigster_payload_file_requests() const {
    return reigster_payload_file_requests_;
  }

  bool has_disconnection_been_requested() {
    return has_disconnection_been_requested_;
  }

  void set_connection_metadata_for_next_call(
      mojom::ConnectionMetadataPtr connection_metadata_for_next_call) {
    connection_metadata_for_next_call_ =
        std::move(connection_metadata_for_next_call);
  }

  // AuthenticatedChannel:
  void GetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) override;
  void PerformSendMessage(const std::string& feature,
                          const std::string& payload,
                          base::OnceClosure on_sent_callback) override;
  void PerformRegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      FileTransferUpdateCallback file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) override;
  void PerformDisconnection() override;

  // Make Notify{Disconnected|MessageReceived}() public for testing.
  using AuthenticatedChannel::NotifyDisconnected;
  using AuthenticatedChannel::NotifyMessageReceived;

 private:
  mojom::ConnectionMetadataPtr connection_metadata_for_next_call_;
  bool has_disconnection_been_requested_ = false;
  std::vector<std::tuple<std::string, std::string, base::OnceClosure>>
      sent_messages_;
  std::vector<RegisterPayloadFileRequest> reigster_payload_file_requests_;
};

// Test AuthenticatedChannel::Observer implementation.
class FakeAuthenticatedChannelObserver : public AuthenticatedChannel::Observer {
 public:
  FakeAuthenticatedChannelObserver();
  ~FakeAuthenticatedChannelObserver() override;

  bool has_been_notified_of_disconnection() {
    return has_been_notified_of_disconnection_;
  }

  const std::vector<std::pair<std::string, std::string>>& received_messages() {
    return received_messages_;
  }

  // AuthenticatedChannel::Observer:
  void OnDisconnected() override;
  void OnMessageReceived(const std::string& feature,
                         const std::string& payload) override;
  void OnNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result) override;

 private:
  bool has_been_notified_of_disconnection_ = false;
  std::vector<std::pair<std::string, std::string>> received_messages_;
  mojom::NearbyConnectionStep nearby_connection_step_;
  mojom::NearbyConnectionStepResult nearby_connection_step_result_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_AUTHENTICATED_CHANNEL_H_
