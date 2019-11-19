// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/macros.h"
#include "chromeos/components/multidevice/remote_device_cache.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/services/secure_channel/secure_channel_base.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace secure_channel {

// Test double SecureChannel implementation.
class FakeSecureChannel : public SecureChannelBase {
 public:
  FakeSecureChannel();
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
      ConnectionPriority connection_priority,
      mojo::PendingRemote<mojom::ConnectionDelegate> delegate) override;
  void InitiateConnectionToDevice(
      const multidevice::RemoteDevice& device_to_connect,
      const multidevice::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      mojo::PendingRemote<mojom::ConnectionDelegate> delegate) override;

  mojo::Remote<mojom::ConnectionDelegate> delegate_from_last_listen_call_;
  mojo::Remote<mojom::ConnectionDelegate> delegate_from_last_initiate_call_;

  DISALLOW_COPY_AND_ASSIGN(FakeSecureChannel);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_H_
