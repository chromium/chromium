// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/authenticated_channel.h"

#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash::secure_channel {

AuthenticatedChannel::Observer::~Observer() = default;

AuthenticatedChannel::AuthenticatedChannel() = default;

AuthenticatedChannel::~AuthenticatedChannel() = default;

bool AuthenticatedChannel::SendMessage(const std::string& feature,
                                       const std::string& payload,
                                       base::OnceClosure on_sent_callback) {
  if (is_disconnected_)
    return false;

  PerformSendMessage(feature, payload, std::move(on_sent_callback));
  return true;
}

void AuthenticatedChannel::RegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    FileTransferUpdateCallback file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  if (is_disconnected_) {
    std::move(registration_result_callback).Run(/*success=*/false);
    return;
  }

  PerformRegisterPayloadFile(payload_id, std::move(payload_files),
                             std::move(file_transfer_update_callback),
                             std::move(registration_result_callback));
}

void AuthenticatedChannel::Disconnect() {
  // Clients should not attempt to disconnect an already-disconnected channel.
  DCHECK(!is_disconnected_);
  PerformDisconnection();
}

void AuthenticatedChannel::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AuthenticatedChannel::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AuthenticatedChannel::NotifyDisconnected() {
  is_disconnected_ = true;

  for (auto& observer : observer_list_)
    observer.OnDisconnected();
}

void AuthenticatedChannel::NotifyMessageReceived(const std::string& feature,
                                                 const std::string& payload) {
  // Make a copy before notifying observers to ensure that if one observer
  // deletes |this| before the next observer is able to be processed, a segfault
  // is prevented.
  const std::string feature_copy = feature;
  const std::string payload_copy = payload;

  for (auto& observer : observer_list_)
    observer.OnMessageReceived(feature_copy, payload_copy);
}

void AuthenticatedChannel::NotifyNearbyConnectionStateChanged(
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  for (auto& observer : observer_list_) {
    observer.OnNearbyConnectionStateChanged(step, result);
  }
}

}  // namespace ash::secure_channel
