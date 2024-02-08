// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_CONNECTION_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_CONNECTION_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/secure_channel/connection.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/ash/services/secure_channel/register_payload_file_request.h"
#include "chromeos/ash/services/secure_channel/secure_channel.h"

namespace ash::secure_channel {

// A fake implementation of SecureChannel to use in tests.
class FakeSecureChannelConnection : public SecureChannel {
 public:
  FakeSecureChannelConnection(std::unique_ptr<Connection> connection);

  FakeSecureChannelConnection(const FakeSecureChannelConnection&) = delete;
  FakeSecureChannelConnection& operator=(const FakeSecureChannelConnection&) =
      delete;

  ~FakeSecureChannelConnection() override;

  void set_destructor_callback(base::OnceClosure destructor_callback) {
    destructor_callback_ = std::move(destructor_callback);
  }

  bool was_initialized() { return was_initialized_; }

  void set_rssi_to_return(const std::optional<int32_t>& rssi_to_return) {
    rssi_to_return_ = rssi_to_return;
  }

  void set_channel_binding_data(
      const std::optional<std::string>& channel_binding_data) {
    channel_binding_data_ = channel_binding_data;
  }

  struct SentMessage {
    SentMessage(const std::string& feature, const std::string& payload);

    std::string feature;
    std::string payload;
  };

  void ChangeStatus(const Status& new_status);
  void ReceiveMessage(const std::string& feature, const std::string& payload);
  void CompleteSendingMessage(int sequence_number);
  void ChangeNearbyConnectionState(
      mojom::NearbyConnectionStep nearby_connection_step,
      mojom::NearbyConnectionStepResult result);
  void ChangeSecureChannelAuthenticationState(
      mojom::SecureChannelState secure_channel_authentication_state);

  std::vector<raw_ptr<Observer, VectorExperimental>> observers() {
    return observers_;
  }

  std::vector<SentMessage> sent_messages() { return sent_messages_; }

  const std::vector<RegisterPayloadFileRequest>&
  register_payload_file_requests() const {
    return register_payload_file_requests_;
  }

  // SecureChannel:
  void Initialize() override;
  int SendMessage(const std::string& feature,
                  const std::string& payload) override;
  void RegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      FileTransferUpdateCallback file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) override;
  void Disconnect() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void GetConnectionRssi(
      base::OnceCallback<void(std::optional<int32_t>)> callback) override;
  std::optional<std::string> GetChannelBindingData() override;

 private:
  int next_sequence_number_ = 0;
  bool was_initialized_ = false;
  std::vector<raw_ptr<Observer, VectorExperimental>> observers_;
  std::vector<SentMessage> sent_messages_;
  std::vector<RegisterPayloadFileRequest> register_payload_file_requests_;
  std::optional<int32_t> rssi_to_return_;
  std::optional<std::string> channel_binding_data_;

  base::OnceClosure destructor_callback_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_CONNECTION_H_
