// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidation_listener.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/prefs/pref_service.h"

namespace invalidation {

FCMInvalidationListener::FCMInvalidationListener(
    std::unique_ptr<FCMSyncNetworkChannel> network_channel)
    : network_channel_(std::move(network_channel)) {
  network_channel_->AddObserver(this);
}

FCMInvalidationListener::~FCMInvalidationListener() {
  network_channel_->RemoveObserver(this);
  Stop();
  DCHECK(!delegate_);
}

void FCMInvalidationListener::Start(
    Delegate* delegate,
    std::unique_ptr<PerUserTopicSubscriptionManager>
        per_user_topic_subscription_manager) {
  DCHECK(delegate);
  Stop();
  delegate_ = delegate;
  per_user_topic_subscription_manager_ =
      std::move(per_user_topic_subscription_manager);
  per_user_topic_subscription_manager_->Init();
  per_user_topic_subscription_manager_->AddObserver(this);
  network_channel_->SetMessageReceiver(
      base::BindRepeating(&FCMInvalidationListener::InvalidationReceived,
                          weak_factory_.GetWeakPtr()));
  network_channel_->SetTokenReceiver(base::BindRepeating(
      &FCMInvalidationListener::TokenReceived, weak_factory_.GetWeakPtr()));
  subscription_channel_state_ = SubscriptionChannelState::ENABLED;

  network_channel_->StartListening();
  EmitStateChange();
  DoSubscriptionUpdate();
}

void FCMInvalidationListener::UpdateInterestedTopics(const Topics& topics) {
  topics_update_requested_ = true;
  interested_topics_ = topics;
  DoSubscriptionUpdate();
}

void FCMInvalidationListener::ClearInstanceIDToken() {
  TokenReceived(std::string());
}

void FCMInvalidationListener::InvalidationReceived(
    const std::string& payload,
    const std::string& private_topic,
    const std::string& public_topic,
    int64_t version) {
  // Note: |public_topic| is empty for some invalidations (e.g. Drive). Prefer
  // using |*expected_public_topic| over |public_topic|.
  absl::optional<std::string> expected_public_topic =
      per_user_topic_subscription_manager_
          ->LookupSubscribedPublicTopicByPrivateTopic(private_topic);
  if (!expected_public_topic ||
      (!public_topic.empty() && public_topic != *expected_public_topic)) {
    DVLOG(1) << "Unexpected invalidation for " << private_topic
             << " with public topic " << public_topic << ". Expected "
             << expected_public_topic.value_or("<None>");
    return;
  }
  TopicInvalidationMap invalidations;
  Invalidation inv =
      Invalidation::Init(*expected_public_topic, version, payload);
  inv.SetAckHandler(weak_factory_.GetWeakPtr(),
                    base::SingleThreadTaskRunner::GetCurrentDefault());
  DVLOG(1) << "Received invalidation with version " << inv.version() << " for "
           << *expected_public_topic;

  invalidations.Insert(inv);
  DispatchInvalidations(invalidations);
}

void FCMInvalidationListener::DispatchInvalidations(
    const TopicInvalidationMap& invalidations) {
  TopicInvalidationMap to_save = invalidations;
  TopicInvalidationMap to_emit =
      invalidations.GetSubsetWithTopics(interested_topics_);

  SaveInvalidations(to_save);
  EmitSavedInvalidations(to_emit);
}

void FCMInvalidationListener::SaveInvalidations(
    const TopicInvalidationMap& to_save) {
  for (const Topic& topic : to_save.GetTopics()) {
    auto lookup = unacked_invalidations_map_.find(topic);
    if (lookup == unacked_invalidations_map_.end()) {
      lookup = unacked_invalidations_map_
                   .emplace(topic, UnackedInvalidationSet(topic))
                   .first;
    }
    lookup->second.AddSet(to_save.ForTopic(topic));
  }
}

void FCMInvalidationListener::EmitSavedInvalidations(
    const TopicInvalidationMap& to_emit) {
  delegate_->OnInvalidate(to_emit);
}

void FCMInvalidationListener::TokenReceived(
    const std::string& instance_id_token) {
  instance_id_token_ = instance_id_token;
  if (instance_id_token_.empty()) {
    if (per_user_topic_subscription_manager_) {
      per_user_topic_subscription_manager_->ClearInstanceIDToken();
    }
  } else {
    DoSubscriptionUpdate();
  }
}

void FCMInvalidationListener::Acknowledge(const Topic& topic,
                                          const AckHandle& handle) {
  auto lookup = unacked_invalidations_map_.find(topic);
  if (lookup == unacked_invalidations_map_.end()) {
    DLOG(WARNING) << "Received acknowledgement for untracked topic";
    return;
  }
  lookup->second.Acknowledge(handle);
}

void FCMInvalidationListener::Drop(const Topic& topic,
                                   const AckHandle& handle) {
  auto lookup = unacked_invalidations_map_.find(topic);
  if (lookup == unacked_invalidations_map_.end()) {
    DLOG(WARNING) << "Received drop for untracked topic";
    return;
  }
  lookup->second.Drop(handle);
}

void FCMInvalidationListener::DoSubscriptionUpdate() {
  if (!per_user_topic_subscription_manager_ || instance_id_token_.empty() ||
      !topics_update_requested_) {
    return;
  }
  per_user_topic_subscription_manager_->UpdateSubscribedTopics(
      interested_topics_, instance_id_token_);

  // Go over all stored unacked invalidations and dispatch them if their topics
  // have become interesting.
  // Note: We might dispatch invalidations for a second time here, if they were
  // already dispatched but not acked yet.
  // TODO(melandory): remove unacked invalidations for unregistered topics.
  TopicInvalidationMap topic_invalidation_map;
  for (const auto& unacked : unacked_invalidations_map_) {
    if (interested_topics_.find(unacked.first) == interested_topics_.end()) {
      continue;
    }

    unacked.second.ExportInvalidations(
        weak_factory_.GetWeakPtr(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        &topic_invalidation_map);
  }

  // There's no need to run these through DispatchInvalidations(); they've
  // already been saved to storage (that's where we found them) so all we need
  // to do now is emit them.
  EmitSavedInvalidations(topic_invalidation_map);
}

void FCMInvalidationListener::RequestDetailedStatus(
    const base::RepeatingCallback<void(base::Value::Dict)>& callback) const {
  network_channel_->RequestDetailedStatus(callback);
  callback.Run(CollectDebugData());
}

void FCMInvalidationListener::StartForTest(Delegate* delegate) {
  delegate_ = delegate;
}

void FCMInvalidationListener::EmitStateChangeForTest(InvalidatorState state) {
  delegate_->OnInvalidatorStateChange(state);
}

void FCMInvalidationListener::EmitSavedInvalidationsForTest(
    const TopicInvalidationMap& to_emit) {
  EmitSavedInvalidations(to_emit);
}

void FCMInvalidationListener::Stop() {
  delegate_ = nullptr;

  if (per_user_topic_subscription_manager_) {
    per_user_topic_subscription_manager_->RemoveObserver(this);
  }
  per_user_topic_subscription_manager_.reset();
  network_channel_->StopListening();

  subscription_channel_state_ = SubscriptionChannelState::NOT_STARTED;
  fcm_network_state_ = FcmChannelState::NOT_STARTED;
}

InvalidatorState FCMInvalidationListener::GetState() const {
  if (subscription_channel_state_ ==
      SubscriptionChannelState::ACCESS_TOKEN_FAILURE) {
    return INVALIDATION_CREDENTIALS_REJECTED;
  }
  if (subscription_channel_state_ == SubscriptionChannelState::ENABLED &&
      fcm_network_state_ == FcmChannelState::ENABLED) {
    // If the ticl is ready and the push client notifications are
    // enabled, return INVALIDATIONS_ENABLED.
    return INVALIDATIONS_ENABLED;
  }

  // Otherwise, we have a transient error.
  return TRANSIENT_INVALIDATION_ERROR;
}

void FCMInvalidationListener::EmitStateChange() {
  delegate_->OnInvalidatorStateChange(GetState());
}

void FCMInvalidationListener::OnFCMChannelStateChanged(FcmChannelState state) {
  fcm_network_state_ = state;
  EmitStateChange();
}

void FCMInvalidationListener::OnSubscriptionChannelStateChanged(
    SubscriptionChannelState state) {
  subscription_channel_state_ = state;
  EmitStateChange();
}

void FCMInvalidationListener::OnSubscriptionRequestStarted(Topic topic) {}

void FCMInvalidationListener::OnSubscriptionRequestFinished(Topic topic,
                                                            Status code) {}

base::Value::Dict FCMInvalidationListener::CollectDebugData() const {
  base::Value::Dict status =
      per_user_topic_subscription_manager_->CollectDebugData();
  status.SetByDottedPath("InvalidationListener.FCM-channel-state",
                         FcmChannelStateToString(fcm_network_state_));
  status.SetByDottedPath(
      "InvalidationListener.Subscription-channel-state",
      SubscriptionChannelStateToString(subscription_channel_state_));
  for (const auto& topic : interested_topics_) {
    if (!status.Find(topic.first)) {
      status.Set(topic.first, "Unsubscribed");
    }
  }
  return status;
}

}  // namespace invalidation
