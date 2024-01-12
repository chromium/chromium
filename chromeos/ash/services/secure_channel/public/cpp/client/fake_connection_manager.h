// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_MANAGER_H_

#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash::secure_channel {

class FakeConnectionManager : public ConnectionManager {
 public:
  FakeConnectionManager();
  ~FakeConnectionManager() override;

  using ConnectionManager::NotifyMessageReceived;

  void SetStatus(Status status);
  const std::vector<std::string>& sent_messages() const {
    return sent_messages_;
  }

  void set_register_payload_file_result(bool result) {
    register_payload_file_result_ = result;
  }

  void SendFileTransferUpdate(mojom::FileTransferUpdatePtr update);

  size_t num_attempt_connection_calls() const {
    return num_attempt_connection_calls_;
  }

  size_t num_disconnect_calls() const { return num_disconnect_calls_; }

 private:
  // ConnectionManager:
  Status GetStatus() const override;
  bool AttemptNearbyConnection() override;
  void Disconnect() override;
  void SendMessage(const std::string& payload) override;
  void RegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>
          file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) override;
  void GetHostLastSeenTimestamp(
      base::OnceCallback<void(std::optional<base::Time>)> callback) override;

  Status status_;
  std::vector<std::string> sent_messages_;
  bool register_payload_file_result_ = true;
  base::flat_map<int64_t,
                 base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>>
      file_transfer_update_callbacks_;
  size_t num_attempt_connection_calls_ = 0;
  size_t num_disconnect_calls_ = 0;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_CONNECTION_MANAGER_H_
