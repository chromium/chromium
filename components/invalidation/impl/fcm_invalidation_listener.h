// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_LISTENER_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_LISTENER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/invalidation/impl/channels_states.h"
#include "components/invalidation/impl/fcm_sync_network_channel.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/invalidation/public/ack_handler.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"

namespace invalidation {

class Invalidation;

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

    virtual void OnInvalidate(const Invalidation& invalidation) = 0;

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
  void UpdateInterestedTopics(const TopicMap& topics);

  // Called when the InstanceID token is revoked (usually because the InstanceID
  // itself was deleted). Note that while this class receives new tokens
  // internally (via FCMSyncNetworkChannel), the deletion flow is triggered
  // externally, so it needs to be explicitly notified of token revocations.
  void ClearInstanceIDToken();

  // AckHandler implementation.
  void Acknowledge(const Topic& topic, const AckHandle& handle) override;

  // FCMSyncNetworkChannel::Observer implementation.
  void OnFCMChannelStateChanged(FcmChannelState state) override;

  // PerUserTopicSubscriptionManager::Observer implementation.
  void OnSubscriptionChannelStateChanged(
      SubscriptionChannelState state) override;
  void OnSubscriptionRequestStarted(Topic topic) override;
  void OnSubscriptionRequestFinished(Topic topic, Status code) override;

  void StartForTest(Delegate* delegate);
  void EmitStateChangeForTest(InvalidatorState state);
  void EmitSavedInvalidationForTest(const Invalidation& invalidation);

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

  // Cache `invalidation` and emit it to registered handlers (if any).
  void DispatchInvalidation(const Invalidation& invalidation);

  // Emits previously saved invalidation to their registered observers.
  void EmitSavedInvalidation(const Invalidation& invalidation);

  std::unique_ptr<FCMSyncNetworkChannel> network_channel_;
  std::map<Topic, Invalidation> unacked_invalidations_map_;
  raw_ptr<Delegate> delegate_ = nullptr;

  // The set of topics for which we want to receive invalidations. We'll pass
  // these to |per_user_topic_subscription_manager_| for (un)subscription.
  TopicMap interested_topics_;

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
