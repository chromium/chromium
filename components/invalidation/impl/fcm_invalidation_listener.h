// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_LISTENER_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_LISTENER_H_

#include <map>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/invalidation/impl/channels_states.h"
#include "components/invalidation/impl/fcm_sync_network_channel.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"

namespace invalidation {

class Invalidation;

// Receives InstanceID tokens and actual invalidations from FCM via
// FCMSyncNetworkChannel, and dispatches them to its delegate (in practice, the
// FCMInvalidationService). Stores invalidations in memory if no handler is
// currently interested in the topic.
// Also tracks the set of topics we're interested in (only invalidations for
// these topics will get dispatched) and passes them to
// PerUserTopicSubscriptionManager for subscription or unsubscription.
class FCMInvalidationListener
    : public FCMSyncNetworkChannel::Observer,
      public PerUserTopicSubscriptionManager::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Handles the invalidation, e.g. by dispatching it to handlers.
    // Returns the invalidation in case it could not be handled.
    virtual std::optional<Invalidation> OnInvalidate(
        const Invalidation& invalidation) = 0;

    virtual void OnInvalidatorStateChange(InvalidatorState state) = 0;

    virtual void OnSuccessfullySubscribed(const Topic& topic) = 0;
  };

  explicit FCMInvalidationListener(
      std::unique_ptr<FCMSyncNetworkChannel> network_channel);
  FCMInvalidationListener(const FCMInvalidationListener& other) = delete;
  FCMInvalidationListener& operator=(const FCMInvalidationListener& other) =
      delete;
  ~FCMInvalidationListener() override;

  // Creates a FCMInvalidationListener instance.
  static std::unique_ptr<FCMInvalidationListener> Create(
      std::unique_ptr<FCMSyncNetworkChannel> network_channel);

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

  // FCMSyncNetworkChannel::Observer implementation.
  void OnFCMChannelStateChanged(FcmChannelState state) override;

  // PerUserTopicSubscriptionManager::Observer implementation.
  void OnSubscriptionChannelStateChanged(
      SubscriptionChannelState state) override;
  void OnSubscriptionRequestFinished(
      Topic topic,
      PerUserTopicSubscriptionManager::RequestType request_type,
      Status code) override;

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

  // Emits `invalidation` to registered handlers (if any).
  // If no handler is interested, the invalidation is cached.
  void DispatchInvalidation(const Invalidation& invalidation);

  // Emits invalidation to their registered observers.
  // Optionally returns the invalidation if no observer was interested.
  std::optional<Invalidation> EmitInvalidation(
      const Invalidation& invalidation);

  std::unique_ptr<FCMSyncNetworkChannel> network_channel_;
  std::map<Topic, Invalidation> undispatched_invalidations_;
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
