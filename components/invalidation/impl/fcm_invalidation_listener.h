// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_LISTENER_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_LISTENER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/invalidation/impl/channels_states.h"
#include "components/invalidation/impl/fcm_sync_network_channel.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/invalidation/impl/unacked_invalidation_set.h"
#include "components/invalidation/public/ack_handler.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"

namespace invalidation {

class TopicInvalidationMap;

// Receives InstanceID tokens and actual invalidations from FCM via
// FCMSyncNetworkChannel, and dispatches them to its delegate (in practice, the
// FCMInvalidationService). Stores invalidations in memory until they were
// actually acked by the corresponding InvalidationHandler (tracked via the
// AckHandler interface).
// Also tracks the set of topics we're interested in (only invalidations for
// these topics will get dispatched) and passes them to
// PerUserTopicSubscriptionManager for subscription or unsubscription.
class FCMInvalidationListener
    : public AckHandler,
      public FCMSyncNetworkChannel::Observer,
      public PerUserTopicSubscriptionManager::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnInvalidate(const TopicInvalidationMap& invalidations) = 0;

    virtual void OnInvalidatorStateChange(InvalidatorState state) = 0;
  };

  explicit FCMInvalidationListener(
      std::unique_ptr<FCMSyncNetworkChannel> network_channel);
  FCMInvalidationListener(const FCMInvalidationListener& other) = delete;
  FCMInvalidationListener& operator=(const FCMInvalidationListener& other) =
      delete;
  ~FCMInvalidationListener() override;

  void Start(Delegate* delegate,
             std::unique_ptr<PerUserTopicSubscriptionManager>
                 per_user_topic_subscription_manager);

  // Update the set of topics for which we want to get invalidations. May be
  // called at any time.
  void UpdateInterestedTopics(const Topics& topics);

  // Called when the InstanceID token is revoked (usually because the InstanceID
  // itself was deleted). Note that while this class receives new tokens
  // internally (via FCMSyncNetworkChannel), the deletion flow is triggered
  // externally, so it needs to be explicitly notified of token revocations.
  void ClearInstanceIDToken();

  // AckHandler implementation.
  void Acknowledge(const Topic& topic, const AckHandle& handle) override;
  void Drop(const Topic& topic, const AckHandle& handle) override;

  // FCMSyncNetworkChannel::Observer implementation.
  void OnFCMChannelStateChanged(FcmChannelState state) override;

  // PerUserTopicSubscriptionManager::Observer implementation.
  void OnSubscriptionChannelStateChanged(
      SubscriptionChannelState state) override;

  virtual void RequestDetailedStatus(
      const base::RepeatingCallback<void(base::Value::Dict)>& callback) const;

  void StartForTest(Delegate* delegate);
  void EmitStateChangeForTest(InvalidatorState state);
  void EmitSavedInvalidationsForTest(const TopicInvalidationMap& to_emit);

 private:
  // Callbacks for the |network_channel_|.
  void InvalidationReceived(const std::string& payload,
                            const std::string& private_topic,
                            const std::string& public_topic,
                            int64_t version);
  void TokenReceived(const std::string& instance_id_token);

  // Passes the |interested_topics_| to |per_user_topic_subscription_manager_|
  // for subscription/unsubscription.
  void DoSubscriptionUpdate();

  void Stop();

  // Derives overall state based on |subscription_channel_state_| and
  // |fcm_network_state_|.
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
  base::Value::Dict CollectDebugData() const;

  std::unique_ptr<FCMSyncNetworkChannel> network_channel_;
  UnackedInvalidationsMap unacked_invalidations_map_;
  raw_ptr<Delegate> delegate_ = nullptr;

  // The set of topics for which we want to receive invalidations. We'll pass
  // these to |per_user_topic_subscription_manager_| for (un)subscription.
  Topics interested_topics_;

  // The states of the HTTP and FCM channel.
  SubscriptionChannelState subscription_channel_state_ =
      SubscriptionChannelState::NOT_STARTED;
  FcmChannelState fcm_network_state_ = FcmChannelState::NOT_STARTED;

  std::unique_ptr<PerUserTopicSubscriptionManager>
      per_user_topic_subscription_manager_;
  std::string instance_id_token_;
  // Prevents call to DoSubscriptionUpdate in cases when
  // UpdateInterestedTopics wasn't called. For example, InformTokenReceived
  // can trigger DoSubscriptionUpdate before any invalidation handler has
  // requested registration for topics.
  bool topics_update_requested_ = false;

  base::WeakPtrFactory<FCMInvalidationListener> weak_factory_{this};
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_LISTENER_H_
