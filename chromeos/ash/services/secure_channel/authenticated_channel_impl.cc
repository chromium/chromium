// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/authenticated_channel_impl.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash::secure_channel {

// static
AuthenticatedChannelImpl::Factory*
    AuthenticatedChannelImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<AuthenticatedChannel> AuthenticatedChannelImpl::Factory::Create(
    const std::vector<mojom::ConnectionCreationDetail>&
        connection_creation_details,
    std::unique_ptr<SecureChannel> secure_channel) {
  if (test_factory_) {
    return test_factory_->CreateInstance(connection_creation_details,
                                         std::move(secure_channel));
  }

  return base::WrapUnique(new AuthenticatedChannelImpl(
      connection_creation_details, std::move(secure_channel)));
}

// static
void AuthenticatedChannelImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

AuthenticatedChannelImpl::Factory::~Factory() = default;

AuthenticatedChannelImpl::AuthenticatedChannelImpl(
    const std::vector<mojom::ConnectionCreationDetail>&
        connection_creation_details,
    std::unique_ptr<SecureChannel> secure_channel)
    : AuthenticatedChannel(),
      connection_creation_details_(connection_creation_details),
      secure_channel_(std::move(secure_channel)) {
  // |secure_channel_| should be a valid and already authenticated.
  DCHECK(secure_channel_);
  DCHECK_EQ(secure_channel_->status(), SecureChannel::Status::AUTHENTICATED);

  secure_channel_->AddObserver(this);
}

AuthenticatedChannelImpl::~AuthenticatedChannelImpl() {
  secure_channel_->RemoveObserver(this);
}

void AuthenticatedChannelImpl::GetConnectionMetadata(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) {
  secure_channel_->GetConnectionRssi(
      base::BindOnce(&AuthenticatedChannelImpl::OnRssiFetched,
                     base::Unretained(this), std::move(callback)));
}

void AuthenticatedChannelImpl::PerformSendMessage(
    const std::string& feature,
    const std::string& payload,
    base::OnceClosure on_sent_callback) {
  DCHECK_EQ(secure_channel_->status(), SecureChannel::Status::AUTHENTICATED);

  int sequence_number = secure_channel_->SendMessage(feature, payload);

  if (base::Contains(sequence_number_to_callback_map_, sequence_number)) {
    PA_LOG(ERROR) << "AuthenticatedChannelImpl::SendMessage(): Started sending "
                  << "a message whose sequence number already exists in the "
                  << "map.";
    NOTREACHED_IN_MIGRATION();
  }

  sequence_number_to_callback_map_[sequence_number] =
      std::move(on_sent_callback);
}

void AuthenticatedChannelImpl::PerformRegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    FileTransferUpdateCallback file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  DCHECK_EQ(secure_channel_->status(), SecureChannel::Status::AUTHENTICATED);
  secure_channel_->RegisterPayloadFile(payload_id, std::move(payload_files),
                                       std::move(file_transfer_update_callback),
                                       std::move(registration_result_callback));
}

void AuthenticatedChannelImpl::PerformDisconnection() {
  secure_channel_->Disconnect();
}

void AuthenticatedChannelImpl::OnSecureChannelStatusChanged(
    SecureChannel* secure_channel,
    const SecureChannel::Status& old_status,
    const SecureChannel::Status& new_status) {
  DCHECK_EQ(secure_channel_.get(), secure_channel);

  // The only expected status changes are AUTHENTICATING => AUTHENTICATED,
  // AUTHENTICATED => DISCONNECTING, AUTHENTICATED => DISCONNECTED, and
  // DISCONNECTING => DISCONNECTED.
  DCHECK(old_status == SecureChannel::Status::AUTHENTICATING ||
         old_status == SecureChannel::Status::AUTHENTICATED ||
         old_status == SecureChannel::Status::DISCONNECTING);
  DCHECK(new_status == SecureChannel::Status::AUTHENTICATED ||
         new_status == SecureChannel::Status::DISCONNECTING ||
         new_status == SecureChannel::Status::DISCONNECTED);

  if (new_status == SecureChannel::Status::DISCONNECTED)
    NotifyDisconnected();
}

void AuthenticatedChannelImpl::OnMessageReceived(SecureChannel* secure_channel,
                                                 const std::string& feature,
                                                 const std::string& payload) {
  DCHECK_EQ(secure_channel_.get(), secure_channel);
  NotifyMessageReceived(feature, payload);
}

void AuthenticatedChannelImpl::OnNearbyConnectionStateChanged(
    SecureChannel* secure_channel,
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  DCHECK_EQ(secure_channel_.get(), secure_channel);
  NotifyNearbyConnectionStateChanged(step, result);
}

void AuthenticatedChannelImpl::OnMessageSent(SecureChannel* secure_channel,
                                             int sequence_number) {
  DCHECK_EQ(secure_channel_.get(), secure_channel);

  if (!base::Contains(sequence_number_to_callback_map_, sequence_number)) {
    PA_LOG(WARNING) << "AuthenticatedChannelImpl::OnMessageSent(): Sent a "
                    << "message whose sequence number did not exist in the "
                    << "map. Disregarding.";
    // Note: No DCHECK() is performed here, since |secure_channel_| could have
    // already been in the process of sending a message before the
    // AuthenticatedChannelImpl object was created.
    return;
  }

  std::move(sequence_number_to_callback_map_[sequence_number]).Run();
  sequence_number_to_callback_map_.erase(sequence_number);
}

void AuthenticatedChannelImpl::OnRssiFetched(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback,
    std::optional<int32_t> current_rssi) {
  mojom::BluetoothConnectionMetadataPtr bluetooth_connection_metadata_ptr;
  if (current_rssi) {
    bluetooth_connection_metadata_ptr =
        mojom::BluetoothConnectionMetadata::New(*current_rssi);
  }

  // The SecureChannel must have channel binding data if it is authenticated.
  DCHECK(secure_channel_->GetChannelBindingData());

  std::move(callback).Run(mojom::ConnectionMetadata::New(
      connection_creation_details_,
      std::move(bluetooth_connection_metadata_ptr),
      *secure_channel_->GetChannelBindingData()));
}

}  // namespace ash::secure_channel
