// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/secure_channel/connection.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/ash/services/secure_channel/register_payload_file_request.h"

namespace ash::secure_channel {

class ConnectionObserver;

// A fake implementation of Connection to use in tests.
class FakeConnection : public Connection {
 public:
  FakeConnection(multidevice::RemoteDeviceRef remote_device);
  FakeConnection(multidevice::RemoteDeviceRef remote_device,
                 bool should_auto_connect);

  FakeConnection(const FakeConnection&) = delete;
  FakeConnection& operator=(const FakeConnection&) = delete;

  ~FakeConnection() override;

  void set_rssi_to_return(const std::optional<int32_t>& rssi_to_return) {
    rssi_to_return_ = rssi_to_return;
  }

  // Connection:
  void Connect() override;
  void Disconnect() override;
  std::string GetDeviceAddress() override;
  void AddObserver(ConnectionObserver* observer) override;
  void RemoveObserver(ConnectionObserver* observer) override;
  void GetConnectionRssi(
      base::OnceCallback<void(std::optional<int32_t>)> callback) override;

  // Completes a connection attempt which was originally started via a call to
  // |Connect()|. If |success| is true, the connection's status shifts to
  // |CONNECTED|; otherwise, the status shifts to |DISCONNECTED|. Note that this
  // function should only be called when |should_auto_connect| is false.
  void CompleteInProgressConnection(bool success);

  // Completes the current send operation with success |success|.
  void FinishSendingMessageWithSuccess(bool success);

  // Simulates receiving a wire message with the given |payload|, bypassing the
  // container WireMessage format.
  void ReceiveMessage(const std::string& feature, const std::string& payload);

  // Returns the current message in progress of being sent.
  WireMessage* current_message() { return current_message_.get(); }

  const std::vector<RegisterPayloadFileRequest>&
  reigster_payload_file_requests() const {
    return reigster_payload_file_requests_;
  }

  std::vector<raw_ptr<ConnectionObserver, VectorExperimental>>& observers() {
    return observers_;
  }

  using Connection::SetStatus;

 private:
  // Connection:
  void SendMessageImpl(std::unique_ptr<WireMessage> message) override;
  void RegisterPayloadFileImpl(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      FileTransferUpdateCallback file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) override;
  std::unique_ptr<WireMessage> DeserializeWireMessage(
      bool* is_incomplete_message) override;

  // The message currently being sent. Only set between a call to
  // SendMessageImpl() and FinishSendingMessageWithSuccess().
  std::unique_ptr<WireMessage> current_message_;

  // The feature and payload that should be returned when
  // DeserializeWireMessage() is called.
  std::string pending_feature_;
  std::string pending_payload_;

  std::vector<RegisterPayloadFileRequest> reigster_payload_file_requests_;

  std::vector<raw_ptr<ConnectionObserver, VectorExperimental>> observers_;

  std::optional<int32_t> rssi_to_return_;
  const bool should_auto_connect_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_H_
