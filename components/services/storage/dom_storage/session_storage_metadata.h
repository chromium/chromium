// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_METADATA_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_METADATA_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

// Holds the metadata information for a session storage database. This includes
// logic for parsing and saving database content.
class SessionStorageMetadata {
 public:
  using NamespaceStorageKeyMap =
      std::map<std::string,
               std::map<blink::StorageKey,
                        scoped_refptr<DomStorageDatabase::SharedMapLocator>>>;
  using NamespaceEntry = NamespaceStorageKeyMap::iterator;

  // Populates the `DomStorageDatabase::Metadata::map_metadata` vector with the
  // `session_id`, storage key, and map ID for each `SharedMapLocator` in
  // `session`.
  static DomStorageDatabase::Metadata ToDomStorageMetadata(
      NamespaceEntry session);

  SessionStorageMetadata();
  ~SessionStorageMetadata();

  // Populates `namespace_storage_key_map_` and sets `next_map_id_` using
  // `source`.
  void Initialize(DomStorageDatabase::Metadata source);

  // Creates new map data for the given namespace-StorageKey area. If the area
  // entry exists, then it will decrement the refcount of the old map.
  //
  // NOTE: It is invalid to call this method for an area that has a map with
  // only one reference.
  scoped_refptr<DomStorageDatabase::SharedMapLocator> RegisterNewMap(
      const std::string& namespace_id,
      const blink::StorageKey& storage_key);

  // Registers an StorageKey-map in the |destination_namespace| from every
  // StorageKey-map in the |source_namespace|. The |destination_namespace| must
  // have no StorageKey-maps. All maps in the destination namespace are the same
  // maps as the source namespace.
  void RegisterShallowClonedNamespace(NamespaceEntry source_namespace,
                                      NamespaceEntry destination_namespace);

  // Removes and returns all of a namespace's `SharedMapLocator` instances from
  // `namespace_storage_key_map_`. Removes `namespace_id` from the session IDs
  // of each returned `SharedMapLocator`.  Other namespaces in
  // `namespace_storage_key_map_` may still reference the returned
  // `SharedMapLocator` instances, which will have `session_ids()` if in-use.
  std::map<blink::StorageKey,
           scoped_refptr<DomStorageDatabase::SharedMapLocator>>
  TakeNamespace(const std::string& namespace_id);

  // Removes and returns a `SharedMapLocator` from `namespace_storage_key_map_`,
  // removing `namespace_id` from its session IDs.  Returns nullptr when
  // `SharedMapLocator` is not found.
  scoped_refptr<DomStorageDatabase::SharedMapLocator> TakeExistingMap(
      const std::string& namespace_id,
      const blink::StorageKey& storage_key);

  NamespaceEntry GetOrCreateNamespaceEntry(const std::string& namespace_id);

  const NamespaceStorageKeyMap& namespace_storage_key_map() const {
    return namespace_storage_key_map_;
  }

  int64_t NextMapId() const { return next_map_id_; }

 private:
  int64_t next_map_id_ = 0;
  NamespaceStorageKeyMap namespace_storage_key_map_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_METADATA_H_
