// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "chromeos/ash/components/multidevice/remote_device_cache.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/secure_channel_base.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::secure_channel {

// Test double SecureChannel implementation.
class FakeSecureChannel : public SecureChannelBase {
 public:
  FakeSecureChannel();

  FakeSecureChannel(const FakeSecureChannel&) = delete;
  FakeSecureChannel& operator=(const FakeSecureChannel&) = delete;

  ~FakeSecureChannel() override;

  mojo::Remote<mojom::ConnectionDelegate> delegate_from_last_listen_call() {
    return std::move(delegate_from_last_listen_call_);
  }

  mojo::Remote<mojom::ConnectionDelegate> delegate_from_last_initiate_call() {
    return std::move(delegate_from_last_initiate_call_);
  }

 private:
  // mojom::SecureChannel:
  void ListenForConnectionFromDevice(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      mojo::PendingRemote<mojom::ConnectionDelegate> delegate) override;
  void InitiateConnectionToDevice(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionMedium connection_medium,
      ConnectionPriority connection_priority,
      mojo::PendingRemote<mojom::ConnectionDelegate> delegate,
      mojo::PendingRemote<mojom::SecureChannelStructuredMetricsLogger>
          secure_channel_structured_metrics_logger) override;
  void SetNearbyConnector(
      mojo::PendingRemote<mojom::NearbyConnector> nearby_connector) override {}
  void GetLastSeenTimestamp(const std::string& remote_device_id,
                            GetLastSeenTimestampCallback callback) override;

  mojo::Remote<mojom::ConnectionDelegate> delegate_from_last_listen_call_;
  mojo::Remote<mojom::ConnectionDelegate> delegate_from_last_initiate_call_;
  mojo::Remote<mojom::SecureChannelStructuredMetricsLogger>
      secure_channel_structured_metrics_logger_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_H_
