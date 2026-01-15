// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_metadata.h"

#include <algorithm>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "url/gurl.h"

namespace storage {

DomStorageDatabase::Metadata SessionStorageMetadata::ToDomStorageMetadata(
    NamespaceEntry session) {
  const std::map<blink::StorageKey,
                 scoped_refptr<DomStorageDatabase::SharedMapLocator>>&
      session_maps = session->second;

  DomStorageDatabase::Metadata metadata;
  for (const auto& [storage_key, map_locator] : session_maps) {
    metadata.map_metadata.push_back({
        .map_locator{
            /*session_id=*/session->first,
            storage_key,
            map_locator->map_id().value(),
        },
    });
  }
  return metadata;
}

SessionStorageMetadata::SessionStorageMetadata() = default;

SessionStorageMetadata::~SessionStorageMetadata() = default;

void SessionStorageMetadata::Initialize(DomStorageDatabase::Metadata source) {
  namespace_storage_key_map_.clear();
  next_map_id_ = source.next_map_id.value();

  for (DomStorageDatabase::MapMetadata& source_map : source.map_metadata) {
    int64_t map_id = source_map.map_locator.map_id().value();
    if (map_id >= next_map_id_) {
      next_map_id_ = map_id + 1;
    }

    scoped_refptr<DomStorageDatabase::SharedMapLocator> shared_map_locator =
        base::MakeRefCounted<DomStorageDatabase::SharedMapLocator>(
            std::move(source_map.map_locator));

    // Create an entry for each map in the namespace/session.  Entries for
    // cloned maps use the same `shared_map_locator`.
    for (const std::string& namespace_id : shared_map_locator->session_ids()) {
      namespace_storage_key_map_[namespace_id].emplace(std::make_pair(
          shared_map_locator->storage_key(), shared_map_locator));
    }
  }
}

scoped_refptr<DomStorageDatabase::SharedMapLocator>
SessionStorageMetadata::RegisterNewMap(const std::string& namespace_id,
                                       const blink::StorageKey& storage_key) {
  auto new_map_locator =
      base::MakeRefCounted<DomStorageDatabase::SharedMapLocator>(
          DomStorageDatabase::MapLocator(namespace_id, storage_key,
                                         next_map_id_));
  ++next_map_id_;

  NamespaceEntry namespace_entry = GetOrCreateNamespaceEntry(namespace_id);
  std::map<blink::StorageKey,
           scoped_refptr<DomStorageDatabase::SharedMapLocator>>&
      namespace_storage_keys = namespace_entry->second;

  auto namespace_it = namespace_storage_keys.find(storage_key);
  if (namespace_it != namespace_storage_keys.end()) {
    // Create a new map to fork a clone.
    DomStorageDatabase::SharedMapLocator& old_map_locator =
        *namespace_it->second;

    // The new map must have a unique ID.
    CHECK_NE(old_map_locator.map_id().value(),
             new_map_locator->map_id().value());

    // Verify that forking was necessary because multiple clones shared the map,
    // making it read-only.
    CHECK_GT(old_map_locator.session_ids().size(), 1u);

    // Remove `namespace_id` from the source clone's sessions.
    DCHECK(std::ranges::contains(old_map_locator.session_ids(), namespace_id));
    old_map_locator.RemoveSession(namespace_id);

    namespace_it->second = new_map_locator;
  } else {
    // Create a new empty map.
    namespace_storage_keys.insert(std::make_pair(storage_key, new_map_locator));
  }
  return new_map_locator;
}

void SessionStorageMetadata::RegisterShallowClonedNamespace(
    NamespaceEntry source_namespace,
    NamespaceEntry destination_namespace) {
  std::map<blink::StorageKey,
           scoped_refptr<DomStorageDatabase::SharedMapLocator>>&
      source_storage_keys = source_namespace->second;
  std::map<blink::StorageKey,
           scoped_refptr<DomStorageDatabase::SharedMapLocator>>&
      destination_storage_keys = destination_namespace->second;
  DCHECK_EQ(0ul, destination_storage_keys.size())
      << "The destination already has data.";

  for (const auto& storage_key_map_pair : source_storage_keys) {
    destination_storage_keys.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(storage_key_map_pair.first),
        std::forward_as_tuple(storage_key_map_pair.second));
    storage_key_map_pair.second->AddSession(destination_namespace->first);
  }
}

std::map<blink::StorageKey, scoped_refptr<DomStorageDatabase::SharedMapLocator>>
SessionStorageMetadata::TakeNamespace(const std::string& namespace_id) {
  auto it = namespace_storage_key_map_.find(namespace_id);
  if (it == namespace_storage_key_map_.end()) {
    return {};
  }

  std::map<blink::StorageKey,
           scoped_refptr<DomStorageDatabase::SharedMapLocator>>
      storage_keys = std::move(it->second);

  for (const auto& storage_key_map_pair : storage_keys) {
    DomStorageDatabase::SharedMapLocator* map_locator =
        storage_key_map_pair.second.get();
    DCHECK(std::ranges::contains(map_locator->session_ids(), namespace_id));
    map_locator->RemoveSession(namespace_id);
  }
  namespace_storage_key_map_.erase(it);
  return storage_keys;
}

scoped_refptr<DomStorageDatabase::SharedMapLocator>
SessionStorageMetadata::TakeExistingMap(const std::string& namespace_id,
                                        const blink::StorageKey& storage_key) {
  auto ns_entry = namespace_storage_key_map_.find(namespace_id);
  if (ns_entry == namespace_storage_key_map_.end()) {
    return nullptr;
  }

  auto storage_key_map_it = ns_entry->second.find(storage_key);
  if (storage_key_map_it == ns_entry->second.end()) {
    return nullptr;
  }

  scoped_refptr<DomStorageDatabase::SharedMapLocator> map_locator =
      storage_key_map_it->second.get();
  DCHECK(std::ranges::contains(map_locator->session_ids(), namespace_id));
  map_locator->RemoveSession(namespace_id);

  ns_entry->second.erase(storage_key_map_it);
  return map_locator;
}

SessionStorageMetadata::NamespaceEntry
SessionStorageMetadata::GetOrCreateNamespaceEntry(
    const std::string& namespace_id) {
  // Note: if the entry exists, emplace will return the existing entry and NOT
  // insert a new entry.
  return namespace_storage_key_map_
      .emplace(std::piecewise_construct, std::forward_as_tuple(namespace_id),
               std::forward_as_tuple())
      .first;
}

}  // namespace storage
