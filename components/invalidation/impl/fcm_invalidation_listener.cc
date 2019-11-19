// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidation_listener.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/invalidation/impl/network_channel.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/prefs/pref_service.h"
#include "google/cacheinvalidation/include/types.h"

namespace syncer {

FCMInvalidationListener::Delegate::~Delegate() {}

FCMInvalidationListener::FCMInvalidationListener(
    std::unique_ptr<FCMSyncNetworkChannel> network_channel)
    : network_channel_(std::move(network_channel)), delegate_(nullptr) {
  network_channel_->AddObserver(this);
}

FCMInvalidationListener::~FCMInvalidationListener() {
  network_channel_->RemoveObserver(this);
  Stop();
  DCHECK(!delegate_);
}

void FCMInvalidationListener::Start(
    Delegate* delegate,
    std::unique_ptr<PerUserTopicRegistrationManager>
        per_user_topic_registration_manager) {
  DCHECK(delegate);
  Stop();
  delegate_ = delegate;
  per_user_topic_registration_manager_ =
      std::move(per_user_topic_registration_manager);
  per_user_topic_registration_manager_->Init();
  per_user_topic_registration_manager_->AddObserver(this);
  network_channel_->SetMessageReceiver(base::BindRepeating(
      &FCMInvalidationListener::Invalidate, weak_factory_.GetWeakPtr()));
  network_channel_->SetTokenReceiver(
      base::BindRepeating(&FCMInvalidationListener::InformTokenReceived,
                          weak_factory_.GetWeakPtr()));
  subscription_channel_state_ = SubscriptionChannelState::ENABLED;

  network_channel_->StartListening();
  EmitStateChange();
  DoRegistrationUpdate();
}

void FCMInvalidationListener::UpdateRegisteredTopics(const Topics& topics) {
  ids_update_requested_ = true;
  registered_topics_ = topics;
  DoRegistrationUpdate();
}

void FCMInvalidationListener::Invalidate(const std::string& payload,
                                         const std::string& private_topic,
                                         const std::string& public_topic,
                                         const std::string& version) {
  // TODO(melandory): use |private_topic| in addition to
  // |registered_topics_| to verify that topic is registered.
  int64_t v;
  if (!base::StringToInt64(version, &v)) {
    // Version must always be in the message and
    // in addition version must be number.
    // TODO(melandory): Report error and consider not to process with the
    // invalidation.
  }
  // Note: |public_topic| is empty for some invalidations (e.g. Drive). Prefer
  // using |*expected_public_topic| over |public_topic|.
  base::Optional<std::string> expected_public_topic =
      per_user_topic_registration_manager_
          ->LookupRegisteredPublicTopicByPrivateTopic(private_topic);
  if (!expected_public_topic ||
      (!public_topic.empty() && public_topic != *expected_public_topic)) {
    DVLOG(1) << "Unexpected invalidation for " << private_topic
             << " with public topic " << public_topic << ". Expected "
             << expected_public_topic.value_or("<None>");
    return;
  }
  TopicInvalidationMap invalidations;
  Invalidation inv =
      Invalidation::Init(ConvertTopicToId(*expected_public_topic), v, payload);
  inv.SetAckHandler(AsWeakPtr(), base::ThreadTaskRunnerHandle::Get());
  DVLOG(1) << "Received invalidation with version " << inv.version() << " for "
           << *expected_public_topic;

  invalidations.Insert(inv);
  DispatchInvalidations(invalidations);
}

void FCMInvalidationListener::DispatchInvalidations(
    const TopicInvalidationMap& invalidations) {
  TopicInvalidationMap to_save = invalidations;
  TopicInvalidationMap to_emit =
      invalidations.GetSubsetWithTopics(registered_topics_);

  SaveInvalidations(to_save);
  EmitSavedInvalidations(to_emit);
}

void FCMInvalidationListener::SaveInvalidations(
    const TopicInvalidationMap& to_save) {
  ObjectIdSet objects_to_save = ConvertTopicsToIds(to_save.GetTopics());
  for (auto it = objects_to_save.begin(); it != objects_to_save.end(); ++it) {
    auto lookup = unacked_invalidations_map_.find(*it);
    if (lookup == unacked_invalidations_map_.end()) {
      lookup = unacked_invalidations_map_
                   .insert(std::make_pair(*it, UnackedInvalidationSet(*it)))
                   .first;
    }
    lookup->second.AddSet(to_save.ForTopic((*it).name()));
  }
}

void FCMInvalidationListener::EmitSavedInvalidations(
    const TopicInvalidationMap& to_emit) {
  delegate_->OnInvalidate(to_emit);
}

void FCMInvalidationListener::InformTokenReceived(const std::string& token) {
  token_ = token;
  DoRegistrationUpdate();
}

void FCMInvalidationListener::Acknowledge(const invalidation::ObjectId& id,
                                          const syncer::AckHandle& handle) {
  auto lookup = unacked_invalidations_map_.find(id);
  if (lookup == unacked_invalidations_map_.end()) {
    DLOG(WARNING) << "Received acknowledgement for untracked object ID";
    return;
  }
  lookup->second.Acknowledge(handle);
}

void FCMInvalidationListener::Drop(const invalidation::ObjectId& id,
                                   const syncer::AckHandle& handle) {
  auto lookup = unacked_invalidations_map_.find(id);
  if (lookup == unacked_invalidations_map_.end()) {
    DLOG(WARNING) << "Received drop for untracked object ID";
    return;
  }
  lookup->second.Drop(handle);
}

void FCMInvalidationListener::DoRegistrationUpdate() {
  if (!per_user_topic_registration_manager_ || token_.empty() ||
      !ids_update_requested_) {
    return;
  }
  per_user_topic_registration_manager_->UpdateRegisteredTopics(
      registered_topics_, token_);

  // TODO(melandory): remove unacked invalidations for unregistered objects.
  ObjectIdInvalidationMap object_id_invalidation_map;
  for (auto& unacked : unacked_invalidations_map_) {
    if (registered_topics_.find(unacked.first.name()) ==
        registered_topics_.end()) {
      continue;
    }

    unacked.second.ExportInvalidations(AsWeakPtr(),
                                       base::ThreadTaskRunnerHandle::Get(),
                                       &object_id_invalidation_map);
  }

  // There's no need to run these through DispatchInvalidations(); they've
  // already been saved to storage (that's where we found them) so all we need
  // to do now is emit them.
  EmitSavedInvalidations(ConvertObjectIdInvalidationMapToTopicInvalidationMap(
      object_id_invalidation_map));
}

void FCMInvalidationListener::RequestDetailedStatus(
    const base::RepeatingCallback<void(const base::DictionaryValue&)>& callback)
    const {
  network_channel_->RequestDetailedStatus(callback);
  callback.Run(CollectDebugData());
}

void FCMInvalidationListener::StopForTest() {
  Stop();
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

Topics FCMInvalidationListener::GetRegisteredIdsForTest() const {
  return registered_topics_;
}

base::WeakPtr<FCMInvalidationListener> FCMInvalidationListener::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FCMInvalidationListener::Stop() {
  delegate_ = nullptr;

  if (per_user_topic_registration_manager_) {
    per_user_topic_registration_manager_->RemoveObserver(this);
  }
  per_user_topic_registration_manager_.reset();
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

base::DictionaryValue FCMInvalidationListener::CollectDebugData() const {
  base::DictionaryValue status =
      per_user_topic_registration_manager_->CollectDebugData();
  status.SetString("InvalidationListener.FCM-channel-state",
                   FcmChannelStateToString(fcm_network_state_));
  status.SetString(
      "InvalidationListener.Subscription-channel-state",
      SubscriptionChannelStateToString(subscription_channel_state_));
  for (const auto& topic : registered_topics_) {
    if (!status.HasKey(topic.first)) {
      status.SetString(topic.first, "Unregistered");
    }
  }
  return status;
}

}  // namespace syncer
