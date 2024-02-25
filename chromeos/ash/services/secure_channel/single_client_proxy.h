// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_PROXY_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_PROXY_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom-forward.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom-forward.h"

namespace ash::secure_channel {

// Proxies the communication channel between clients and remote devices.
//
// When the client makes a request to send message, register incoming file
// payload, or get the connection metadata, the request will be forwarded to its
// delegate.
//
// When messages are received from the remote device, HandleReceivedMessage()
// should be called so that the message can be passed to the client.
class SingleClientProxy {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnSendMessageRequested(const std::string& message_feaure,
                                        const std::string& message_payload,
                                        base::OnceClosure on_sent_callback) = 0;
    virtual void RegisterPayloadFile(
        int64_t payload_id,
        mojom::PayloadFilesPtr payload_files,
        FileTransferUpdateCallback file_transfer_update_callback,
        base::OnceCallback<void(bool)> registration_result_callback) = 0;
    virtual void GetConnectionMetadata(
        base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) = 0;
    virtual void OnClientDisconnected(
        const base::UnguessableToken& proxy_id) = 0;
  };

  SingleClientProxy(const SingleClientProxy&) = delete;
  SingleClientProxy& operator=(const SingleClientProxy&) = delete;

  virtual ~SingleClientProxy();

  // Should be called when any message is received over the connection.
  virtual void HandleReceivedMessage(const std::string& feature,
                                     const std::string& payload) = 0;

  virtual void HandleNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result) = 0;

  // Should be called when the underlying connection to the remote device has
  // been disconnected (e.g., because the other device closed the connection or
  // because of instability on the communication channel).
  virtual void HandleRemoteDeviceDisconnection() = 0;

  virtual const base::UnguessableToken& GetProxyId() = 0;

 protected:
  SingleClientProxy(Delegate* delegate);

  void NotifySendMessageRequested(const std::string& message_feature,
                                  const std::string& message_payload,
                                  base::OnceClosure on_sent_callback);
  void NotifyClientDisconnected();
  void RegisterPayloadFileWithDelegate(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      FileTransferUpdateCallback file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback);
  void GetConnectionMetadataFromDelegate(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback);

 private:
  raw_ptr<Delegate> delegate_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SINGLE_CLIENT_PROXY_H_
