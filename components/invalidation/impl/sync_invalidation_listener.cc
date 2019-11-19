// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/sync_invalidation_listener.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/invalidation/impl/registration_manager.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "google/cacheinvalidation/include/invalidation-client.h"
#include "google/cacheinvalidation/include/types.h"
#include "jingle/notifier/listener/push_client.h"

namespace {

const char kApplicationName[] = "chrome-sync";

}  // namespace

namespace syncer {

SyncInvalidationListener::Delegate::~Delegate() {}

SyncInvalidationListener::SyncInvalidationListener(
    std::unique_ptr<SyncNetworkChannel> network_channel)
    : sync_network_channel_(std::move(network_channel)),
      sync_system_resources_(sync_network_channel_.get(), this),
      delegate_(nullptr),
      ticl_state_(DEFAULT_INVALIDATION_ERROR),
      push_client_state_(DEFAULT_INVALIDATION_ERROR) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_network_channel_->AddObserver(this);
}

SyncInvalidationListener::~SyncInvalidationListener() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_network_channel_->RemoveObserver(this);
  Stop();
  DCHECK(!delegate_);
}

void SyncInvalidationListener::Start(
    const CreateInvalidationClientCallback& create_invalidation_client_callback,
    const std::string& client_id,
    const std::string& client_info,
    const std::string& invalidation_bootstrap_data,
    const UnackedInvalidationsMap& initial_unacked_invalidations,
    const base::WeakPtr<InvalidationStateTracker>& invalidation_state_tracker,
    const scoped_refptr<base::SequencedTaskRunner>&
        invalidation_state_tracker_task_runner,
    Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();

  sync_system_resources_.set_platform(client_info);
  sync_system_resources_.Start();

  // The Storage resource is implemented as a write-through cache.  We populate
  // it with the initial state on startup, so subsequent writes go to disk and
  // update the in-memory cache, while reads just return the cached state.
  sync_system_resources_.storage()->SetInitialState(
      invalidation_bootstrap_data);

  unacked_invalidations_map_ = initial_unacked_invalidations;
  invalidation_state_tracker_ = invalidation_state_tracker;
  invalidation_state_tracker_task_runner_ =
      invalidation_state_tracker_task_runner;
  DCHECK(invalidation_state_tracker_task_runner_);

  DCHECK(!delegate_);
  DCHECK(delegate);
  delegate_ = delegate;

  invalidation_client_.reset(create_invalidation_client_callback.Run(
      &sync_system_resources_,
      sync_network_channel_->GetInvalidationClientType(),
      client_id,
      kApplicationName,
      this));
  invalidation_client_->Start();

  registration_manager_.reset(
      new RegistrationManager(invalidation_client_.get()));
}

void SyncInvalidationListener::UpdateCredentials(
    const CoreAccountId& account_id,
    const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_network_channel_->UpdateCredentials(account_id, token);
}

void SyncInvalidationListener::UpdateRegisteredIds(const ObjectIdSet& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  registered_ids_ = ids;
  // |ticl_state_| can go to INVALIDATIONS_ENABLED even without a
  // working XMPP connection (as observed by us), so check it instead
  // of GetState() (see http://crbug.com/139424).
  if (ticl_state_ == INVALIDATIONS_ENABLED && registration_manager_) {
    DoRegistrationUpdate();
  }
}

void SyncInvalidationListener::Ready(
    invalidation::InvalidationClient* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(client, invalidation_client_.get());
  ticl_state_ = INVALIDATIONS_ENABLED;
  EmitStateChange();
  DoRegistrationUpdate();
}

void SyncInvalidationListener::Invalidate(
    invalidation::InvalidationClient* client,
    const invalidation::Invalidation& invalidation,
    const invalidation::AckHandle& ack_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(client, invalidation_client_.get());
  client->Acknowledge(ack_handle);

  const invalidation::ObjectId& id = invalidation.object_id();

  std::string payload;
  // payload() CHECK()'s has_payload(), so we must check it ourselves first.
  if (invalidation.has_payload())
    payload = invalidation.payload();

  DVLOG(2) << "Received invalidation with version " << invalidation.version()
           << " for " << ObjectIdToString(id);

  ObjectIdInvalidationMap invalidations;
  Invalidation inv = Invalidation::Init(id, invalidation.version(), payload);
  inv.SetAckHandler(AsWeakPtr(), base::ThreadTaskRunnerHandle::Get());
  invalidations.Insert(inv);

  DispatchInvalidations(invalidations);
}

void SyncInvalidationListener::InvalidateUnknownVersion(
    invalidation::InvalidationClient* client,
    const invalidation::ObjectId& object_id,
    const invalidation::AckHandle& ack_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(client, invalidation_client_.get());
  DVLOG(1) << "InvalidateUnknownVersion";
  client->Acknowledge(ack_handle);

  ObjectIdInvalidationMap invalidations;
  Invalidation unknown_version = Invalidation::InitUnknownVersion(object_id);
  unknown_version.SetAckHandler(AsWeakPtr(),
                                base::ThreadTaskRunnerHandle::Get());
  invalidations.Insert(unknown_version);

  DispatchInvalidations(invalidations);
}

// This should behave as if we got an invalidation with version
// UNKNOWN_OBJECT_VERSION for all known data types.
void SyncInvalidationListener::InvalidateAll(
    invalidation::InvalidationClient* client,
    const invalidation::AckHandle& ack_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(client, invalidation_client_.get());
  DVLOG(1) << "InvalidateAll";
  client->Acknowledge(ack_handle);

  ObjectIdInvalidationMap invalidations;
  for (auto it = registered_ids_.begin(); it != registered_ids_.end(); ++it) {
    Invalidation unknown_version = Invalidation::InitUnknownVersion(*it);
    unknown_version.SetAckHandler(AsWeakPtr(),
                                  base::ThreadTaskRunnerHandle::Get());
    invalidations.Insert(unknown_version);
  }

  DispatchInvalidations(invalidations);
}

// If a handler is registered, emit right away.  Otherwise, save it for later.
void SyncInvalidationListener::DispatchInvalidations(
    const ObjectIdInvalidationMap& invalidations) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ObjectIdInvalidationMap to_save = invalidations;
  ObjectIdInvalidationMap to_emit =
      invalidations.GetSubsetWithObjectIds(registered_ids_);

  SaveInvalidations(to_save);
  EmitSavedInvalidations(to_emit);
}

void SyncInvalidationListener::SaveInvalidations(
    const ObjectIdInvalidationMap& to_save) {
  ObjectIdSet objects_to_save = to_save.GetObjectIds();
  for (auto it = objects_to_save.begin(); it != objects_to_save.end(); ++it) {
    auto lookup = unacked_invalidations_map_.find(*it);
    if (lookup == unacked_invalidations_map_.end()) {
      lookup = unacked_invalidations_map_.insert(
          std::make_pair(*it, UnackedInvalidationSet(*it))).first;
    }
    lookup->second.AddSet(to_save.ForObject(*it));
  }

  invalidation_state_tracker_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InvalidationStateTracker::SetSavedInvalidations,
                     invalidation_state_tracker_, unacked_invalidations_map_));
}

void SyncInvalidationListener::EmitSavedInvalidations(
    const ObjectIdInvalidationMap& to_emit) {
  DVLOG(2) << "Emitting invalidations: " << to_emit.ToString();
  delegate_->OnInvalidate(to_emit);
}

void SyncInvalidationListener::InformRegistrationStatus(
      invalidation::InvalidationClient* client,
      const invalidation::ObjectId& object_id,
      InvalidationListener::RegistrationState new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(client, invalidation_client_.get());
  DVLOG(1) << "InformRegistrationStatus: "
           << ObjectIdToString(object_id) << " " << new_state;

  if (new_state != InvalidationListener::REGISTERED) {
    // Let |registration_manager_| handle the registration backoff policy.
    registration_manager_->MarkRegistrationLost(object_id);
  }
}

void SyncInvalidationListener::InformRegistrationFailure(
    invalidation::InvalidationClient* client,
    const invalidation::ObjectId& object_id,
    bool is_transient,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(client, invalidation_client_.get());
  DVLOG(1) << "InformRegistrationFailure: "
           << ObjectIdToString(object_id)
           << "is_transient=" << is_transient
           << ", message=" << error_message;

  if (is_transient) {
    // We don't care about |unknown_hint|; we let
    // |registration_manager_| handle the registration backoff policy.
    registration_manager_->MarkRegistrationLost(object_id);
  } else {
    // Non-transient failures require an action to resolve. This could happen
    // because:
    // - the server doesn't yet recognize the data type, which could happen for
    //   brand-new data types.
    // - the user has changed their password and hasn't updated it yet locally.
    // Either way, block future registration attempts for |object_id|. However,
    // we don't forget any saved invalidation state since we may use it once the
    // error is addressed.
    registration_manager_->DisableId(object_id);
  }
}

void SyncInvalidationListener::ReissueRegistrations(
    invalidation::InvalidationClient* client,
    const std::string& prefix,
    int prefix_length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(client, invalidation_client_.get());
  DVLOG(1) << "AllRegistrationsLost";
  registration_manager_->MarkAllRegistrationsLost();
}

void SyncInvalidationListener::InformError(
    invalidation::InvalidationClient* client,
    const invalidation::ErrorInfo& error_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(client, invalidation_client_.get());
  LOG(ERROR) << "Ticl error " << error_info.error_reason() << ": "
             << error_info.error_message()
             << " (transient = " << error_info.is_transient() << ")";
  if (error_info.error_reason() == invalidation::ErrorReason::AUTH_FAILURE) {
    ticl_state_ = INVALIDATION_CREDENTIALS_REJECTED;
  } else {
    ticl_state_ = TRANSIENT_INVALIDATION_ERROR;
  }
  EmitStateChange();
}

void SyncInvalidationListener::Acknowledge(
  const invalidation::ObjectId& id,
  const syncer::AckHandle& handle) {
  auto lookup = unacked_invalidations_map_.find(id);
  if (lookup == unacked_invalidations_map_.end()) {
    DLOG(WARNING) << "Received acknowledgement for untracked object ID";
    return;
  }
  lookup->second.Acknowledge(handle);
  invalidation_state_tracker_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InvalidationStateTracker::SetSavedInvalidations,
                     invalidation_state_tracker_, unacked_invalidations_map_));
}

void SyncInvalidationListener::Drop(
    const invalidation::ObjectId& id,
    const syncer::AckHandle& handle) {
  auto lookup = unacked_invalidations_map_.find(id);
  if (lookup == unacked_invalidations_map_.end()) {
    DLOG(WARNING) << "Received drop for untracked object ID";
    return;
  }
  lookup->second.Drop(handle);
  invalidation_state_tracker_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InvalidationStateTracker::SetSavedInvalidations,
                     invalidation_state_tracker_, unacked_invalidations_map_));
}

void SyncInvalidationListener::WriteState(const std::string& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "WriteState";
  invalidation_state_tracker_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&InvalidationStateTracker::SetBootstrapData,
                                invalidation_state_tracker_, state));
}

void SyncInvalidationListener::DoRegistrationUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const ObjectIdSet& unregistered_ids =
      registration_manager_->UpdateRegisteredIds(registered_ids_);
  for (auto it = unregistered_ids.begin(); it != unregistered_ids.end(); ++it) {
    unacked_invalidations_map_.erase(*it);
  }
  invalidation_state_tracker_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InvalidationStateTracker::SetSavedInvalidations,
                     invalidation_state_tracker_, unacked_invalidations_map_));

  ObjectIdInvalidationMap object_id_invalidation_map;
  for (auto map_it = unacked_invalidations_map_.begin();
       map_it != unacked_invalidations_map_.end(); ++map_it) {
    if (registered_ids_.find(map_it->first) == registered_ids_.end()) {
      continue;
    }
    map_it->second.ExportInvalidations(AsWeakPtr(),
                                       base::ThreadTaskRunnerHandle::Get(),
                                       &object_id_invalidation_map);
  }

  // There's no need to run these through DispatchInvalidations(); they've
  // already been saved to storage (that's where we found them) so all we need
  // to do now is emit them.
  EmitSavedInvalidations(object_id_invalidation_map);
}

void SyncInvalidationListener::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_network_channel_->RequestDetailedStatus(callback);
  callback.Run(*CollectDebugData());
}

std::unique_ptr<base::DictionaryValue>
SyncInvalidationListener::CollectDebugData() const {
  std::unique_ptr<base::DictionaryValue> return_value(
      new base::DictionaryValue());
  return_value->SetString(
      "SyncInvalidationListener.PushClientState",
      std::string(InvalidatorStateToString(push_client_state_)));
  return_value->SetString("SyncInvalidationListener.TiclState",
                          std::string(InvalidatorStateToString(ticl_state_)));
  std::unique_ptr<base::DictionaryValue> unacked_map(
      new base::DictionaryValue());
  for (auto it = unacked_invalidations_map_.begin();
       it != unacked_invalidations_map_.end(); ++it) {
    unacked_map->Set((it->first).name(), (it->second).ToValue());
  }
  return_value->Set("SyncInvalidationListener.UnackedInvalidationsMap",
                    std::move(unacked_map));
  return return_value;
}

void SyncInvalidationListener::StopForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

void SyncInvalidationListener::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!invalidation_client_) {
    return;
  }

  registration_manager_.reset();
  sync_system_resources_.Stop();
  invalidation_client_->Stop();

  invalidation_client_.reset();
  delegate_ = nullptr;

  ticl_state_ = DEFAULT_INVALIDATION_ERROR;
  push_client_state_ = DEFAULT_INVALIDATION_ERROR;
}

InvalidatorState SyncInvalidationListener::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ticl_state_ == INVALIDATION_CREDENTIALS_REJECTED ||
      push_client_state_ == INVALIDATION_CREDENTIALS_REJECTED) {
    // If either the ticl or the push client rejected our credentials,
    // return INVALIDATION_CREDENTIALS_REJECTED.
    return INVALIDATION_CREDENTIALS_REJECTED;
  }
  if (ticl_state_ == INVALIDATIONS_ENABLED &&
      push_client_state_ == INVALIDATIONS_ENABLED) {
    // If the ticl is ready and the push client notifications are
    // enabled, return INVALIDATIONS_ENABLED.
    return INVALIDATIONS_ENABLED;
  }
  // Otherwise, we have a transient error.
  return TRANSIENT_INVALIDATION_ERROR;
}

void SyncInvalidationListener::EmitStateChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnInvalidatorStateChange(GetState());
}

base::WeakPtr<AckHandler> SyncInvalidationListener::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::WeakPtr<AckHandler> weak_ptr = weak_ptr_factory_.GetWeakPtr();
  weak_ptr.get();  // Binds the pointer to this thread.
  return weak_ptr;
}

void SyncInvalidationListener::OnNetworkChannelStateChanged(
    InvalidatorState invalidator_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  push_client_state_ = invalidator_state;
  EmitStateChange();
}

}  // namespace syncer
