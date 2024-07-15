// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_namespace_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"

namespace storage {

namespace {

void SessionStorageResponse(base::OnceClosure callback, bool success) {
  std::move(callback).Run();
}

}  // namespace

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    std::string namespace_id,
    SessionStorageDataMap::Listener* data_map_listener,
    SessionStorageAreaImpl::RegisterNewAreaMap register_new_map_callback,
    Delegate* delegate)
    : namespace_id_(std::move(namespace_id)),
      data_map_listener_(data_map_listener),
      register_new_map_callback_(std::move(register_new_map_callback)),
      delegate_(delegate) {}

SessionStorageNamespaceImpl::~SessionStorageNamespaceImpl() {
  DCHECK(child_namespaces_waiting_for_clone_call_.empty());
}

void SessionStorageNamespaceImpl::SetPendingPopulationFromParentNamespace(
    const std::string& from_namespace) {
  pending_population_from_parent_namespace_ = from_namespace;
  state_ = State::kNotPopulatedAndPendingClone;
}

void SessionStorageNamespaceImpl::AddChildNamespaceWaitingForClone(
    const std::string& namespace_id) {
  child_namespaces_waiting_for_clone_call_.insert(namespace_id);
}
bool SessionStorageNamespaceImpl::HasChildNamespacesWaitingForClone() const {
  return !child_namespaces_waiting_for_clone_call_.empty();
}
void SessionStorageNamespaceImpl::ClearChildNamespacesWaitingForClone() {
  child_namespaces_waiting_for_clone_call_.clear();
}

bool SessionStorageNamespaceImpl::HasAreaForStorageKey(
    const blink::StorageKey& storage_key) const {
  return storage_key_areas_.find(storage_key) != storage_key_areas_.end();
}

void SessionStorageNamespaceImpl::PopulateFromMetadata(
    AsyncDomStorageDatabase* database,
    SessionStorageMetadata::NamespaceEntry namespace_metadata) {
  DCHECK(!IsPopulated());
  database_ = database;
  state_ = State::kPopulated;
  pending_population_from_parent_namespace_.clear();
  namespace_entry_ = namespace_metadata;
  for (const auto& pair : namespace_entry_->second) {
    scoped_refptr<SessionStorageDataMap> data_map =
        delegate_->MaybeGetExistingDataMapForId(
            pair.second->MapNumberAsBytes());
    if (!data_map) {
      data_map = SessionStorageDataMap::CreateFromDisk(data_map_listener_,
                                                       pair.second, database_);
    }
    storage_key_areas_[pair.first] = std::make_unique<SessionStorageAreaImpl>(
        namespace_entry_, pair.first, std::move(data_map),
        register_new_map_callback_);
  }
  if (!run_after_population_.empty()) {
    for (base::OnceClosure& callback : run_after_population_)
      std::move(callback).Run();
    run_after_population_.clear();
  }
}

void SessionStorageNamespaceImpl::PopulateAsClone(
    AsyncDomStorageDatabase* database,
    SessionStorageMetadata::NamespaceEntry namespace_metadata,
    const StorageKeyAreas& areas_to_clone) {
  DCHECK(!IsPopulated());
  database_ = database;
  state_ = State::kPopulated;
  pending_population_from_parent_namespace_.clear();
  namespace_entry_ = namespace_metadata;
  base::ranges::transform(
      areas_to_clone,
      std::inserter(storage_key_areas_, storage_key_areas_.begin()),
      [namespace_metadata](const auto& source) {
        return std::make_pair(source.first,
                              source.second->Clone(namespace_metadata));
      });
  if (!run_after_population_.empty()) {
    for (base::OnceClosure& callback : run_after_population_)
      std::move(callback).Run();
    run_after_population_.clear();
  }
}

void SessionStorageNamespaceImpl::Reset() {
  namespace_entry_ = SessionStorageMetadata::NamespaceEntry();
  database_ = nullptr;
  pending_population_from_parent_namespace_.clear();
  bind_waiting_on_population_ = false;
  run_after_population_.clear();
  state_ = State::kNotPopulated;
  child_namespaces_waiting_for_clone_call_.clear();
  storage_key_areas_.clear();
  receivers_.Clear();
}

void SessionStorageNamespaceImpl::Bind(
    mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver) {
  if (!IsPopulated()) {
    bind_waiting_on_population_ = true;
    run_after_population_.push_back(
        base::BindOnce(&SessionStorageNamespaceImpl::Bind,
                       base::Unretained(this), std::move(receiver)));
    return;
  }
  DCHECK(IsPopulated());
  receivers_.Add(this, std::move(receiver));
  bind_waiting_on_population_ = false;
}

void SessionStorageNamespaceImpl::PurgeUnboundAreas() {
  auto it = storage_key_areas_.begin();
  while (it != storage_key_areas_.end()) {
    if (!it->second->IsBound())
      it = storage_key_areas_.erase(it);
    else
      ++it;
  }
}

void SessionStorageNamespaceImpl::RemoveStorageKeyData(
    const blink::StorageKey& storage_key,
    base::OnceClosure callback) {
  DCHECK_NE(state_, State::kNotPopulated);
  if (!IsPopulated()) {
    run_after_population_.push_back(base::BindOnce(
        &SessionStorageNamespaceImpl::RemoveStorageKeyData,
        base::Unretained(this), storage_key, std::move(callback)));
    return;
  }
  DCHECK(IsPopulated());
  auto it = storage_key_areas_.find(storage_key);
  if (it == storage_key_areas_.end()) {
    std::move(callback).Run();
    return;
  }
  // Renderer process expects |source| to always be two newline separated
  // strings.
  it->second->DeleteAll(
      "\n", /*new_observer=*/mojo::NullRemote(),
      base::BindOnce(&SessionStorageResponse, std::move(callback)));
  it->second->NotifyObserversAllDeleted();
  it->second->data_map()->storage_area()->ScheduleImmediateCommit();
}

void SessionStorageNamespaceImpl::OpenArea(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  if (!IsPopulated()) {
    run_after_population_.push_back(base::BindOnce(
        &SessionStorageNamespaceImpl::OpenArea, base::Unretained(this),
        storage_key, std::move(receiver)));
    return;
  }

  auto it = storage_key_areas_.find(storage_key);
  if (it == storage_key_areas_.end()) {
    // The area may have been purged due to lack of bindings, so check the
    // metadata for the map.
    scoped_refptr<SessionStorageDataMap> data_map;
    auto map_data_it = namespace_entry_->second.find(storage_key);
    if (map_data_it != namespace_entry_->second.end()) {
      // The map exists already, either on disk or being used by another
      // namespace.
      scoped_refptr<SessionStorageMetadata::MapData> map_data =
          map_data_it->second;
      data_map =
          delegate_->MaybeGetExistingDataMapForId(map_data->MapNumberAsBytes());
      if (!data_map) {
        data_map = SessionStorageDataMap::CreateFromDisk(data_map_listener_,
                                                         map_data, database_);
      }
    } else {
      // The map doesn't exist yet.
      data_map = SessionStorageDataMap::CreateEmpty(
          data_map_listener_,
          register_new_map_callback_.Run(namespace_entry_, storage_key),
          database_);
    }
    it = storage_key_areas_
             .emplace(std::make_pair(
                 storage_key,
                 std::make_unique<SessionStorageAreaImpl>(
                     namespace_entry_, storage_key, std::move(data_map),
                     register_new_map_callback_)))
             .first;
  }
  it->second->Bind(std::move(receiver));
}

void SessionStorageNamespaceImpl::Clone(const std::string& clone_to_namespace) {
  DCHECK(IsPopulated());
  child_namespaces_waiting_for_clone_call_.erase(clone_to_namespace);
  delegate_->RegisterShallowClonedNamespace(
      namespace_entry_, clone_to_namespace, storage_key_areas_);
}

void SessionStorageNamespaceImpl::CloneAllNamespacesWaitingForClone(
    AsyncDomStorageDatabase* database,
    SessionStorageMetadata* metadata,
    const std::map<std::string, std::unique_ptr<SessionStorageNamespaceImpl>>&
        namespaces_map) {
  SessionStorageNamespaceImpl* parent = this;
  // If the current state is kNotPopulatedAndPendingClone, then the children can
  // all be cloned from our parent instead of us.
  if (state() == State::kNotPopulatedAndPendingClone) {
    auto parent_it =
        namespaces_map.find(pending_population_from_parent_namespace_);
    // The parent must be in the map, because the only way to remove something
    // from the map is to call DeleteNamespace, which would have called this
    // method on the parent if there were children, and resolved our clone
    // dependency.
    CHECK(parent_it != namespaces_map.end(), base::NotFatalUntil::M130);
    parent = parent_it->second.get();
  }

  if (parent->state() == State::kNotPopulated) {
    // Populate the namespace to prepare for copy.
    parent->PopulateFromMetadata(
        database, metadata->GetOrCreateNamespaceEntry(parent->namespace_id_));
  }

  auto* delegate = parent->delegate_.get();
  for (const std::string& destination_namespace :
       child_namespaces_waiting_for_clone_call_) {
    if (parent->IsPopulated()) {
      delegate->RegisterShallowClonedNamespace(parent->namespace_entry(),
                                               destination_namespace,
                                               parent->storage_key_areas_);
    } else {
      parent->AddChildNamespaceWaitingForClone(destination_namespace);
      parent->run_after_population_.push_back(
          base::BindOnce(&SessionStorageNamespaceImpl::Clone,
                         base::Unretained(parent), destination_namespace));
      auto child_it = namespaces_map.find(destination_namespace);
      // The child must be in the map, as the only way to add it to
      // |child_namespaces_waiting_for_clone_call_| is to call
      // CloneNamespace, which always adds it to the map.
      CHECK(child_it != namespaces_map.end(), base::NotFatalUntil::M130);
      child_it->second->SetPendingPopulationFromParentNamespace(
          parent->namespace_id_);
    }
  }
  child_namespaces_waiting_for_clone_call_.clear();
}

void SessionStorageNamespaceImpl::FlushAreasForTesting() {
  for (auto& area : storage_key_areas_)
    area.second->FlushForTesting();
}

void SessionStorageNamespaceImpl::FlushStorageKeyForTesting(
    const blink::StorageKey& storage_key) {
  if (!IsPopulated())
    return;
  auto it = storage_key_areas_.find(storage_key);
  if (it == storage_key_areas_.end())
    return;
  it->second->data_map()->storage_area()->ScheduleImmediateCommit();
}

}  // namespace storage
