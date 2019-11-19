// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_LISTENER_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_LISTENER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/invalidation/impl/channels_states.h"
#include "components/invalidation/impl/fcm_sync_network_channel.h"
#include "components/invalidation/impl/invalidation_listener.h"
#include "components/invalidation/impl/logger.h"
#include "components/invalidation/impl/per_user_topic_registration_manager.h"
#include "components/invalidation/impl/unacked_invalidation_set.h"
#include "components/invalidation/public/ack_handler.h"
#include "components/invalidation/public/invalidation_object_id.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace syncer {

class TopicInvalidationMap;

// By implementing the AckHandler interface it tracks the messages
// which were passed to InvalidationHandlers.
class FCMInvalidationListener : public InvalidationListener,
                                public AckHandler,
                                FCMSyncNetworkChannel::Observer,
                                PerUserTopicRegistrationManager::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    virtual void OnInvalidate(const TopicInvalidationMap& invalidations) = 0;

    virtual void OnInvalidatorStateChange(InvalidatorState state) = 0;
  };

  explicit FCMInvalidationListener(
      std::unique_ptr<FCMSyncNetworkChannel> network_channel);

  ~FCMInvalidationListener() override;

  void Start(
      Delegate* delegate,
      std::unique_ptr<PerUserTopicRegistrationManager>
          per_user_topic_registration_manager);

  // Update the set of object IDs that we're interested in getting
  // notifications for. May be called at any time.
  void UpdateRegisteredTopics(const Topics& topics);

  // InvalidationListener implementation.
  void Invalidate(const std::string& payload,
                  const std::string& private_topic,
                  const std::string& public_topic,
                  const std::string& version) override;
  void InformTokenReceived(const std::string& token) override;

  // AckHandler implementation.
  void Acknowledge(const invalidation::ObjectId& id,
                   const syncer::AckHandle& handle) override;
  void Drop(const invalidation::ObjectId& id,
            const syncer::AckHandle& handle) override;

  // FCMSyncNetworkChannel::Observer implementation.
  void OnFCMChannelStateChanged(FcmChannelState state) override;

  // PerUserTopicRegistrationManager::Observer implementation.
  void OnSubscriptionChannelStateChanged(
      SubscriptionChannelState state) override;

  void DoRegistrationUpdate();

  virtual void RequestDetailedStatus(
      const base::RepeatingCallback<void(const base::DictionaryValue&)>&
          callback) const;

  void StopForTest();
  void StartForTest(Delegate* delegate);
  void EmitStateChangeForTest(InvalidatorState state);
  void EmitSavedInvalidationsForTest(const TopicInvalidationMap& to_emit);

  Topics GetRegisteredIdsForTest() const;

  base::WeakPtr<FCMInvalidationListener> AsWeakPtr();

 private:
  void Stop();

  InvalidatorState GetState() const;

  void EmitStateChange();

  // Sends invalidations to their appropriate destination.
  //
  // If there are no observers registered for them, they will be saved for
  // later.
  //
  // If there are observers registered, they will be saved (to make sure we
  // don't drop them until they've been acted on) and emitted to the observers.
  void DispatchInvalidations(const TopicInvalidationMap& invalidations);

  // Saves invalidations.
  //
  // This call isn't synchronous so we can't guarantee these invalidations will
  // be safely on disk by the end of the call, but it should ensure that the
  // data makes it to disk eventually.
  void SaveInvalidations(const TopicInvalidationMap& to_save);
  // Emits previously saved invalidations to their registered observers.
  void EmitSavedInvalidations(const TopicInvalidationMap& to_emit);

  // Generate a Dictionary with all the debugging information.
  base::DictionaryValue CollectDebugData() const;

  std::unique_ptr<FCMSyncNetworkChannel> network_channel_;
  UnackedInvalidationsMap unacked_invalidations_map_;
  Delegate* delegate_;
  Logger logger_;

  // Stored to pass to |per_user_topic_registration_manager_| on start.
  Topics registered_topics_;

  // The states of the HTTP and FCM channel.
  SubscriptionChannelState subscription_channel_state_ =
      SubscriptionChannelState::NOT_STARTED;
  FcmChannelState fcm_network_state_ = FcmChannelState::NOT_STARTED;

  std::unique_ptr<PerUserTopicRegistrationManager>
      per_user_topic_registration_manager_;
  std::string token_;
  // Prevents call to DoRegistrationUpdate in cases when
  // UpdateRegisteredTopics wasn't called. For example, InformTokenReceived
  // can trigger DoRegistrationUpdate before any invalidation handler has
  // requested registration for topics.
  bool ids_update_requested_ = false;

  base::WeakPtrFactory<FCMInvalidationListener> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FCMInvalidationListener);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_LISTENER_H_
