// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidation_listener.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"

namespace invalidation {

namespace {

// Insert or update the invalidation in the map at `invalidation.topic()`.
// If `map` does not have an invalidation for that topic, a copy of `inv` will
// be inserted.
// Otherwise, the existing invalidation for the topic will be replaced by `inv`
// if and only if `inv` has a higher version than `map.at(inv.topic())`.
void Upsert(std::map<Topic, Invalidation>& map,
            const std::optional<Invalidation>& invalidation) {
  if (!invalidation) {
    return;
  }
  auto it = map.find(invalidation->topic());
  if (it == map.end()) {
    map.emplace(invalidation->topic(), *invalidation);
    return;
  }
  if (it->second.version() < invalidation->version()) {
    it->second = *invalidation;
    return;
  }
}

}  // namespace

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

// static
std::unique_ptr<FCMInvalidationListener> FCMInvalidationListener::Create(
    std::unique_ptr<FCMSyncNetworkChannel> network_channel) {
  return std::make_unique<FCMInvalidationListener>(std::move(network_channel));
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

void FCMInvalidationListener::UpdateInterestedTopics(const TopicMap& topics) {
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
  std::optional<std::string> expected_public_topic =
      per_user_topic_subscription_manager_
          ->LookupSubscribedPublicTopicByPrivateTopic(private_topic);
  if (!expected_public_topic ||
      (!public_topic.empty() && public_topic != *expected_public_topic)) {
    DVLOG(1) << "Unexpected invalidation for " << private_topic
             << " with public topic " << public_topic << ". Expected "
             << expected_public_topic.value_or("<None>");
    return;
  }
  Invalidation inv = Invalidation(*expected_public_topic, version, payload);
  DVLOG(1) << "Received invalidation with version " << inv.version() << " for "
           << *expected_public_topic;

  DispatchInvalidation(inv);
}

void FCMInvalidationListener::DispatchInvalidation(
    const Invalidation& invalidation) {
  const auto uninteresting_invalidation = EmitInvalidation(invalidation);
  Upsert(undispatched_invalidations_, uninteresting_invalidation);
}

std::optional<Invalidation> FCMInvalidationListener::EmitInvalidation(
    const Invalidation& invalidation) {
  if (!interested_topics_.contains(invalidation.topic())) {
    return invalidation;
  }
  return delegate_->OnInvalidate(invalidation);
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

void FCMInvalidationListener::DoSubscriptionUpdate() {
  if (!per_user_topic_subscription_manager_ || instance_id_token_.empty() ||
      !topics_update_requested_) {
    return;
  }
  per_user_topic_subscription_manager_->UpdateSubscribedTopics(
      interested_topics_, instance_id_token_);

  // Emit all cached invalidations, as some might be interesting now.
  // The non-dispatched invalidations form the new cache.
  std::map<Topic, Invalidation> new_cache;
  for (const auto& [topic, invalidation] : undispatched_invalidations_) {
    const auto uninteresting_invalidation = EmitInvalidation(invalidation);
    Upsert(new_cache, uninteresting_invalidation);
  }
  swap(new_cache, undispatched_invalidations_);
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
  if (subscription_channel_state_ == SubscriptionChannelState::ENABLED &&
      fcm_network_state_ == FcmChannelState::ENABLED) {
    return InvalidatorState::kEnabled;
  }

  // Otherwise, we have a (possibly transient) error.
  return InvalidatorState::kDisabled;
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

void FCMInvalidationListener::OnSubscriptionRequestFinished(
    Topic topic,
    PerUserTopicSubscriptionManager::RequestType request_type,
    Status code) {
  if (request_type ==
          PerUserTopicSubscriptionManager::RequestType::kSubscribe &&
      code.IsSuccess()) {
    delegate_->OnSuccessfullySubscribed(topic);
  }
}

}  // namespace invalidation
