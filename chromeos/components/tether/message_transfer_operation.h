// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_MESSAGE_TRANSFER_OPERATION_H_
#define CHROMEOS_COMPONENTS_TETHER_MESSAGE_TRANSFER_OPERATION_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace tether {

class MessageWrapper;
class TimerFactory;

// Abstract base class used for operations which send and/or receive messages
// from remote devices.
class MessageTransferOperation {
 public:
  MessageTransferOperation(
      const multidevice::RemoteDeviceRefList& devices_to_connect,
      secure_channel::ConnectionPriority connection_priority,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client);
  virtual ~MessageTransferOperation();

  // Initializes the operation by registering device connection listeners with
  // SecureChannel.
  void Initialize();

 protected:
  // Unregisters |remote_device| for the MessageType returned by
  // GetMessageTypeForConnection().
  void UnregisterDevice(multidevice::RemoteDeviceRef remote_device);

  // Sends |message_wrapper|'s message to |remote_device| and returns the
  // associated message's sequence number.
  int SendMessageToDevice(multidevice::RemoteDeviceRef remote_device,
                          std::unique_ptr<MessageWrapper> message_wrapper);

  // Callback executed whena device is authenticated (i.e., it is in a state
  // which allows messages to be sent/received). Should be overridden by derived
  // classes which intend to send a message to |remote_device| as soon as an
  // authenticated channel has been established to that device.
  virtual void OnDeviceAuthenticated(
      multidevice::RemoteDeviceRef remote_device) {}

  // Callback executed when a tether protocol message is received. Should be
  // overriden by derived classes which intend to handle messages received from
  // |remote_device|.
  virtual void OnMessageReceived(
      std::unique_ptr<MessageWrapper> message_wrapper,
      multidevice::RemoteDeviceRef remote_device) {}

  // Callback executed when any message is received on the "magic_tether"
  // feature.
  virtual void OnMessageReceived(const std::string& device_id,
                                 const std::string& payload);

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

  multidevice::RemoteDeviceRefList& remote_devices() { return remote_devices_; }

 private:
  friend class ConnectTetheringOperationTest;
  friend class DisconnectTetheringOperationTest;
  friend class HostScannerOperationTest;
  friend class KeepAliveOperationTest;
  friend class MessageTransferOperationTest;

  class ConnectionAttemptDelegate
      : public secure_channel::ConnectionAttempt::Delegate {
   public:
    ConnectionAttemptDelegate(
        MessageTransferOperation* operation,
        multidevice::RemoteDeviceRef remote_device,
        std::unique_ptr<secure_channel::ConnectionAttempt> connection_attempt);
    ~ConnectionAttemptDelegate() override;

    // secure_channel::ConnectionAttempt::Delegate:
    void OnConnectionAttemptFailure(
        secure_channel::mojom::ConnectionAttemptFailureReason reason) override;
    void OnConnection(
        std::unique_ptr<secure_channel::ClientChannel> channel) override;

   private:
    MessageTransferOperation* operation_;
    multidevice::RemoteDeviceRef remote_device_;
    std::unique_ptr<secure_channel::ConnectionAttempt> connection_attempt_;
  };

  class ClientChannelObserver : public secure_channel::ClientChannel::Observer {
   public:
    ClientChannelObserver(
        MessageTransferOperation* operation,
        multidevice::RemoteDeviceRef remote_device,
        std::unique_ptr<secure_channel::ClientChannel> client_channel);
    ~ClientChannelObserver() override;

    // secure_channel::ClientChannel::Observer:
    void OnDisconnected() override;
    void OnMessageReceived(const std::string& payload) override;

    secure_channel::ClientChannel* channel() { return client_channel_.get(); }

   private:
    MessageTransferOperation* operation_;
    multidevice::RemoteDeviceRef remote_device_;
    std::unique_ptr<secure_channel::ClientChannel> client_channel_;
  };

  // The maximum expected time to connect to a remote device, if it can be
  // connected to. This number has been determined by examining metrics.
  static constexpr const uint32_t kConnectionTimeoutSeconds = 15;

  // The default number of seconds an operation should wait to send and receive
  // messages before a timeout occurs. Once this amount of time passes, the
  // connection will be closed. Classes deriving from MessageTransferOperation
  // should override GetMessageTimeoutSeconds() if they desire a different
  // duration.
  static constexpr const uint32_t kDefaultMessageTimeoutSeconds = 10;

  void OnConnectionAttemptFailure(
      multidevice::RemoteDeviceRef remote_device,
      secure_channel::mojom::ConnectionAttemptFailureReason reason);
  void OnConnection(multidevice::RemoteDeviceRef remote_device,
                    std::unique_ptr<secure_channel::ClientChannel> channel);
  void OnDisconnected(multidevice::RemoteDeviceRef remote_device);

  // Start the timer while waiting for a connection to |remote_device|. See
  // |kConnectionTimeoutSeconds|.
  void StartConnectionTimerForDevice(
      multidevice::RemoteDeviceRef remote_device);

  // Start the timer while waiting for messages to be sent to and received by
  // |remote_device|. See |kDefaultMessageTimeoutSeconds|.
  void StartMessageTimerForDevice(multidevice::RemoteDeviceRef remote_device);

  void StartTimerForDevice(multidevice::RemoteDeviceRef remote_device,
                           uint32_t timeout_seconds);
  void StopTimerForDeviceIfRunning(multidevice::RemoteDeviceRef remote_device);
  void OnTimeout(multidevice::RemoteDeviceRef remote_device);
  base::Optional<multidevice::RemoteDeviceRef> GetRemoteDevice(
      const std::string& device_id);

  void SetTimerFactoryForTest(
      std::unique_ptr<TimerFactory> timer_factory_for_test);

  multidevice::RemoteDeviceRefList remote_devices_;
  device_sync::DeviceSyncClient* device_sync_client_;
  secure_channel::SecureChannelClient* secure_channel_client_;
  const secure_channel::ConnectionPriority connection_priority_;

  std::unique_ptr<TimerFactory> timer_factory_;

  bool initialized_ = false;
  bool shutting_down_ = false;
  MessageType message_type_for_connection_;

  base::flat_map<multidevice::RemoteDeviceRef,
                 std::unique_ptr<ConnectionAttemptDelegate>>
      remote_device_to_connection_attempt_delegate_map_;
  base::flat_map<multidevice::RemoteDeviceRef,
                 std::unique_ptr<ClientChannelObserver>>
      remote_device_to_client_channel_observer_map_;
  int next_message_sequence_number_ = 0;

  base::flat_map<multidevice::RemoteDeviceRef,
                 std::unique_ptr<base::OneShotTimer>>
      remote_device_to_timer_map_;
  base::WeakPtrFactory<MessageTransferOperation> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MessageTransferOperation);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_MESSAGE_TRANSFER_OPERATION_H_
