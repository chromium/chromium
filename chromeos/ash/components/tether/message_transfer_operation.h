// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_MESSAGE_TRANSFER_OPERATION_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_MESSAGE_TRANSFER_OPERATION_H_

#include <map>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/tether/message_wrapper.h"
#include "chromeos/ash/components/tether/proto/tether.pb.h"
#include "chromeos/ash/components/tether/tether_host.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/client_channel.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace ash::secure_channel {
class SecureChannelClient;
}

namespace cross_device {
class TimerFactory;
}  // namespace cross_device

namespace ash::tether {

// Abstract base class used for operations which send and/or receive messages
// from remote devices.
class MessageTransferOperation
    : public secure_channel::ClientChannel::Observer,
      public secure_channel::ConnectionAttempt::Delegate {
 public:
  MessageTransferOperation(
      const TetherHost& tether_host,
      secure_channel::ConnectionPriority connection_priority,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client);

  MessageTransferOperation(const MessageTransferOperation&) = delete;
  MessageTransferOperation& operator=(const MessageTransferOperation&) = delete;

  ~MessageTransferOperation() override;

  // Initializes the operation by registering device connection listeners with
  // SecureChannel.
  void Initialize();

 protected:
  // Manually ends the operation.
  void StopOperation();

  // Sends |message_wrapper|'s message to |remote_device_| and returns the
  // associated message's sequence number.
  int SendMessageToDevice(std::unique_ptr<MessageWrapper> message_wrapper);

  // Callback executed when a device is authenticated (i.e., it is in a state
  // which allows messages to be sent/received). Should be overridden by derived
  // classes which intend to send a message to |remote_device_| as soon as an
  // authenticated channel has been established to that device.
  virtual void OnDeviceAuthenticated() {}

  // Callback executed when a tether protocol message is received. Should be
  // overridden by derived classes which intend to handle messages received from
  // |remote_device_|.
  virtual void OnMessageReceived(
      std::unique_ptr<MessageWrapper> message_wrapper) {}

  // Callback executed a tether protocol message is sent. |sequence_number| is
  // the value returned by SendMessageToDevice().
  virtual void OnMessageSent(int sequence_number) {}

  // Callback executed when the operation has started (i.e., in Initialize()).
  virtual void OnOperationStarted() {}

  // Callback executed when the operation has finished (i.e., when all devices
  // have been unregistered).
  virtual void OnOperationFinished() {}

  // Returns the type of message that this operation intends to send.
  virtual MessageType GetMessageTypeForConnection() = 0;

  // The number of seconds that this operation should wait to let messages be
  // sent and received before unregistering a device after it has been
  // authenticated if it has not been explicitly unregistered. If
  // ShouldOperationUseTimeout() returns false, this method is never used.
  virtual uint32_t GetMessageTimeoutSeconds();

  const std::string GetDeviceId(bool truncate_for_logs) const;

  std::unique_ptr<secure_channel::ConnectionAttempt> connection_attempt_;
  std::unique_ptr<secure_channel::ClientChannel> client_channel_;

 private:
  friend class ConnectTetheringOperationTest;
  friend class DisconnectTetheringOperationTest;
  friend class TetherAvailabilityOperationTest;
  friend class KeepAliveOperationTest;
  friend class MessageTransferOperationTest;

  // secure_channel::ConnectionAttempt::Delegate:
  void OnConnectionAttemptFailure(
      secure_channel::mojom::ConnectionAttemptFailureReason reason) override;
  void OnConnection(
      std::unique_ptr<secure_channel::ClientChannel> channel) override;

  // secure_channel::ClientChannel::Observer:
  void OnDisconnected() override;
  void OnMessageReceived(const std::string& payload) override;

  // The maximum expected time to connect to a remote device, if it can be
  // connected to. This number has been determined by examining metrics.
  static constexpr const uint32_t kConnectionTimeoutSeconds = 15;

  // The default number of seconds an operation should wait to send and receive
  // messages before a timeout occurs. Once this amount of time passes, the
  // connection will be closed. Classes deriving from MessageTransferOperation
  // should override GetMessageTimeoutSeconds() if they desire a different
  // duration.
  static constexpr const uint32_t kDefaultMessageTimeoutSeconds = 10;

  // Start the timer while waiting for a connection to |remote_device|. See
  // |kConnectionTimeoutSeconds|.
  void StartConnectionTimerForDevice();

  // Start the timer while waiting for messages to be sent to and received by
  // |remote_device|. See |kDefaultMessageTimeoutSeconds|.
  void StartMessageTimerForDevice();

  void StartTimerForDevice(uint32_t timeout_seconds);
  void StopTimerForDeviceIfRunning();
  void OnTimeout();

  void SetTimerFactoryForTest(
      std::unique_ptr<cross_device::TimerFactory> timer_factory_for_test);

  TetherHost tether_host_;
  raw_ptr<device_sync::DeviceSyncClient> device_sync_client_;
  raw_ptr<secure_channel::SecureChannelClient> secure_channel_client_;
  const secure_channel::ConnectionPriority connection_priority_;

  std::unique_ptr<cross_device::TimerFactory> timer_factory_;

  bool initialized_ = false;
  bool shutting_down_ = false;
  MessageType message_type_for_connection_;

  int next_message_sequence_number_ = 0;

  std::unique_ptr<base::OneShotTimer> remote_device_timer_;

  base::WeakPtrFactory<MessageTransferOperation> weak_ptr_factory_{this};
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_MESSAGE_TRANSFER_OPERATION_H_
