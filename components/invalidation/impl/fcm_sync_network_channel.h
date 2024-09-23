// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_SYNC_NETWORK_CHANNEL_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_SYNC_NETWORK_CHANNEL_H_

#include <string>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/invalidation/impl/channels_states.h"

namespace invalidation {

// FCMSyncNetworkChannel implements common tasks needed from the network by
// client:
//  - registering message callbacks
//  - notifying on network problems
class FCMSyncNetworkChannel {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnFCMChannelStateChanged(
        FcmChannelState invalidator_state) = 0;
  };

  // See SetMessageReceiver below.
  //   payload - additional info specific to the invalidation
  //   private_topic - the internal (to FCM) representation for the public topic
  //   public_topic - the topic which was invalidated, e.g. in case of Chrome
  //                  Sync it'll be BOOKMARK or PASSWORD
  //   version - version number of the invalidation
  using MessageCallback =
      base::RepeatingCallback<void(const std::string& payload,
                                   const std::string& private_topic,
                                   const std::string& public_topic,
                                   int64_t version)>;

  using TokenCallback = base::RepeatingCallback<void(const std::string& token)>;

  FCMSyncNetworkChannel();
  virtual ~FCMSyncNetworkChannel();

  virtual void StartListening() = 0;
  virtual void StopListening() = 0;

  // Sets the receiver to which messages from the data center will be delivered.
  // The callback will be invoked whenever an invalidation message is received
  // from FCM. It is *not* guaranteed to be invoked exactly once or in-order
  // (with respect to the invalidation's version number).
  void SetMessageReceiver(MessageCallback incoming_receiver);

  // Sets the receiver to which FCM registration token will be delivered.
  // The callback will be invoked whenever a new InstanceID token becomes
  // available.
  void SetTokenReceiver(TokenCallback token_receiver);

  // Classes interested in network channel state changes should implement
  // FCMSyncNetworkChannel::Observer and register here.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // Subclass should notify about connection state through
  // NotifyChannelStateChange.
  void NotifyChannelStateChange(FcmChannelState invalidator_state);

  // Subclass should call DeliverIncomingMessage for message to reach
  // invalidations library.
  bool DeliverIncomingMessage(const std::string& payload,
                              const std::string& private_topic,
                              const std::string& public_topic,
                              int64_t version);

  // Subclass should call DeliverToken for token to reach registration
  // manager.
  bool DeliverToken(const std::string& token);

 private:
  // Callbacks into invalidation library
  MessageCallback incoming_receiver_;
  TokenCallback token_receiver_;

  int received_messages_count_;
  std::string token_;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_SYNC_NETWORK_CHANNEL_H_
