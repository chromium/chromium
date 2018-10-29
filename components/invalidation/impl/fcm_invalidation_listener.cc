// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidation_listener.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "components/invalidation/impl/network_channel.h"
#include "components/invalidation/impl/per_user_topic_invalidation_client.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/prefs/pref_service.h"
#include "google/cacheinvalidation/include/types.h"

namespace syncer {

FCMInvalidationListener::Delegate::~Delegate() {}

FCMInvalidationListener::FCMInvalidationListener(
    std::unique_ptr<FCMSyncNetworkChannel> network_channel)
    : network_channel_(std::move(network_channel)),
      delegate_(nullptr),
      subscription_channel_state_(DEFAULT_INVALIDATION_ERROR),
      fcm_network_state_(DEFAULT_INVALIDATION_ERROR),
      weak_factory_(this) {
  network_channel_->AddObserver(this);
}

FCMInvalidationListener::~FCMInvalidationListener() {
  network_channel_->RemoveObserver(this);
  Stop();
  DCHECK(!delegate_);
}

void FCMInvalidationListener::Start(
    CreateInvalidationClientCallback create_invalidation_client_callback,
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
  invalidation_client_ = std::move(create_invalidation_client_callback)
                             .Run(network_channel_.get(), &logger_, this);
  invalidation_client_->Start();
}

void FCMInvalidationListener::UpdateRegisteredTopics(const TopicSet& topics) {
  ids_update_requested_ = true;
  registered_topics_ = topics;
  DoRegistrationUpdate();
}

void FCMInvalidationListener::Ready(InvalidationClient* client) {
  DCHECK_EQ(client, invalidation_client_.get());
  subscription_channel_state_ = INVALIDATIONS_ENABLED;
  EmitStateChange();
  DoRegistrationUpdate();
}

void FCMInvalidationListener::Invalidate(InvalidationClient* client,
                                         const std::string& payload,
                                         const std::string& private_topic_name,
                                         const std::string& public_topic_name,
                                         int64_t version) {
  DCHECK_EQ(client, invalidation_client_.get());

  TopicInvalidationMap invalidations;
  Invalidation inv =
      Invalidation::Init(ConvertTopicToId(public_topic_name), version, payload);
  inv.SetAckHandler(AsWeakPtr(), base::ThreadTaskRunnerHandle::Get());
  DVLOG(1) << "Received invalidation with version " << inv.version() << " for "
           << public_topic_name;

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

void FCMInvalidationListener::InformTokenRecieved(InvalidationClient* client,
                                                  const std::string& token) {
  DCHECK_EQ(client, invalidation_client_.get());
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

void FCMInvalidationListener::StopForTest() {
  Stop();
}

TopicSet FCMInvalidationListener::GetRegisteredIdsForTest() const {
  return registered_topics_;
}

base::WeakPtr<FCMInvalidationListener> FCMInvalidationListener::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FCMInvalidationListener::Stop() {
  if (!invalidation_client_) {
    return;
  }

  invalidation_client_->Stop();

  invalidation_client_.reset();
  delegate_ = nullptr;

  if (per_user_topic_registration_manager_) {
    per_user_topic_registration_manager_->RemoveObserver(this);
  }
  per_user_topic_registration_manager_.reset();

  subscription_channel_state_ = DEFAULT_INVALIDATION_ERROR;
  fcm_network_state_ = DEFAULT_INVALIDATION_ERROR;
}

InvalidatorState FCMInvalidationListener::GetState() const {
  if (subscription_channel_state_ == INVALIDATION_CREDENTIALS_REJECTED ||
      fcm_network_state_ == INVALIDATION_CREDENTIALS_REJECTED) {
    // If either the ticl or the push client rejected our credentials,
    // return INVALIDATION_CREDENTIALS_REJECTED.
    return INVALIDATION_CREDENTIALS_REJECTED;
  }
  if (subscription_channel_state_ == INVALIDATIONS_ENABLED &&
      fcm_network_state_ == INVALIDATIONS_ENABLED) {
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

void FCMInvalidationListener::OnFCMSyncNetworkChannelStateChanged(
    InvalidatorState invalidator_state) {
  fcm_network_state_ = invalidator_state;
  EmitStateChange();
}

void FCMInvalidationListener::OnSubscriptionChannelStateChanged(
    InvalidatorState invalidator_state) {
  subscription_channel_state_ = invalidator_state;
  EmitStateChange();
}

}  // namespace syncer
