// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_AUTHENTICATED_CHANNEL_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_AUTHENTICATED_CHANNEL_IMPL_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/ash/services/secure_channel/secure_channel.h"

namespace ash::secure_channel {

// Concrete AuthenticatedChannel implementation, whose send/receive mechanisms
// are implemented via SecureChannel.
class AuthenticatedChannelImpl : public AuthenticatedChannel,
                                 public SecureChannel::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<AuthenticatedChannel> Create(
        const std::vector<mojom::ConnectionCreationDetail>&
            connection_creation_details,
        std::unique_ptr<SecureChannel> secure_channel);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<AuthenticatedChannel> CreateInstance(
        const std::vector<mojom::ConnectionCreationDetail>&
            connection_creation_details,
        std::unique_ptr<SecureChannel> secure_channel) = 0;

   private:
    static Factory* test_factory_;
  };

  AuthenticatedChannelImpl(const AuthenticatedChannelImpl&) = delete;
  AuthenticatedChannelImpl& operator=(const AuthenticatedChannelImpl&) = delete;

  ~AuthenticatedChannelImpl() override;

 private:
  AuthenticatedChannelImpl(const std::vector<mojom::ConnectionCreationDetail>&
                               connection_creation_details,
                           std::unique_ptr<SecureChannel> secure_channel);

  // AuthenticatedChannel:
  void GetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) override;
  void PerformSendMessage(const std::string& feature,
                          const std::string& payload,
                          base::OnceClosure on_sent_callback) final;
  void PerformRegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      FileTransferUpdateCallback file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) final;
  void PerformDisconnection() override;

  // SecureChannel::Observer:
  void OnSecureChannelStatusChanged(
      SecureChannel* secure_channel,
      const SecureChannel::Status& old_status,
      const SecureChannel::Status& new_status) override;
  void OnMessageReceived(SecureChannel* secure_channel,
                         const std::string& feature,
                         const std::string& payload) override;
  void OnMessageSent(SecureChannel* secure_channel,
                     int sequence_number) override;
  void OnNearbyConnectionStateChanged(
      SecureChannel* secure_channel,
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result) override;

  void OnRssiFetched(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback,
      std::optional<int32_t> current_rssi);

  const std::vector<mojom::ConnectionCreationDetail>
      connection_creation_details_;
  std::unique_ptr<SecureChannel> secure_channel_;
  std::unordered_map<int, base::OnceClosure> sequence_number_to_callback_map_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_AUTHENTICATED_CHANNEL_IMPL_H_
