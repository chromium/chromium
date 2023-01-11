// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/services/storage/dom_storage/session_storage_area_impl.h"
#include "components/services/storage/dom_storage/session_storage_data_map.h"
#include "components/services/storage/dom_storage/session_storage_metadata.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/dom_storage/session_storage_namespace.mojom.h"

namespace storage {

class AsyncDomStorageDatabase;

// Implements the Blink SessionStorageNamespace interface. Stores data maps per
// StorageKey, which are accessible using the StorageArea interface with the
// |OpenArea| call. Supports cloning (shallow cloning with copy-on-write
// behavior) from another SessionStorageNamespaceImpl.
//
// This class is populated & bound in the following patterns:
// 1. The namespace is new or being populated from data on disk, and
//    |PopulateFromMetadata| is called. Afterwards |Bind| can be called.
// 2. The namespace is being created as a clone from a |Clone| call on another
//    SessionStorageNamespaceImpl. PopulateFromMetadata is called with the
//    data from the other namespace, and then |Bind| can be called afterwards.
// 3. The namespace is being created as a clone, but the |Clone| call from the
//    source namespace hasn't been called yet.
//    |set_pending_population_from_namespace| is called first, after which
//    |Bind| can be called. The actually binding doesn't happen until
//    |PopulateAsClone| is finally called with the source namespace data.
// Note: The reason for cases 2 and 3 is because there are two ways the Session
// Storage system knows about clones. First, it gets the |Clone| call on the
// source namespace, coming from the renderer doing the navigation, and in the
// correct order with any session storage modifications from that source
// renderer. Second, the RenderViewHostImpl of the navigated-to-frame will
// create the cloned namespace and expect to manage it's lifetime that way, and
// this can happen before the first case, as they are on different task runners.
class SessionStorageNamespaceImpl final
    : public blink::mojom::SessionStorageNamespace {
 public:
  using StorageKeyAreas =
      std::map<blink::StorageKey, std::unique_ptr<SessionStorageAreaImpl>>;

  enum class State {
    // This is the default state when a namespace is first constructed. It has
    // no |database_| yet and is not connected to disk.
    kNotPopulated,
    // This is the same as kNotPopulated but it also means that this namespace
    // was created by Cloning from a 'parent' namespace (see
    // SessionStorageContext.CloneNamespace), but the Clone call has not
    // yet been called on the parent's SessionStorageNamespaceImpl.
    kNotPopulatedAndPendingClone,
    // This means the namespace is connected to disk, |database_| is populated,
    // and it is operating normally. This happens when PopulateFromMetadata or
    // PopulateAsClone is called.
    kPopulated
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // This is called when the |Clone()| method is called by mojo.
    virtual void RegisterShallowClonedNamespace(
        SessionStorageMetadata::NamespaceEntry source_namespace,
        const std::string& destination_namespace,
        const StorageKeyAreas& areas_to_clone) = 0;

    // This is called when |OpenArea()| is called. The map could have been
    // purged in a call to |PurgeUnboundAreas| but the map could still be alive
    // as a clone, used by another namespace.
    // Returns nullptr if a data map was not found.
    virtual scoped_refptr<SessionStorageDataMap> MaybeGetExistingDataMapForId(
        const std::vector<uint8_t>& map_number_as_bytes) = 0;
  };

  // Constructs a namespace with the given |namespace_id|, expecting to be
  // populated and bound later (see class comment).  The |database| and
  // |data_map_listener| are given to any data maps constructed for this
  // namespace. The |delegate| is called when the |Clone| method
  // is called by mojo, as well as when the |OpenArea| method is called and the
  // map id for that StorageKey is found in our metadata. The
  // |register_new_map_callback| is given to the the
  // SessionStorageAreaImpl's, used per-StorageKey, that are bound to in
  // OpenArea.
  SessionStorageNamespaceImpl(
      std::string namespace_id,
      SessionStorageDataMap::Listener* data_map_listener,
      SessionStorageAreaImpl::RegisterNewAreaMap register_new_map_callback,
      Delegate* delegate);

  ~SessionStorageNamespaceImpl() override;

  State state() const { return state_; }

  // Sets the |pending_population_from_namespace_| to the given namespace and
  // sets the state() to State::kNotPopulatedAndPendingClone.
  void SetPendingPopulationFromParentNamespace(
      const std::string& from_namespace);

  const std::string& pending_population_from_parent_namespace() const {
    return pending_population_from_parent_namespace_;
  }

  void AddChildNamespaceWaitingForClone(const std::string& namespace_id);
  bool HasChildNamespacesWaitingForClone() const;
  void ClearChildNamespacesWaitingForClone();

  // Returns if a storage area exists for the given StorageKey in this map.
  bool HasAreaForStorageKey(const blink::StorageKey& StorageKey) const;

  // Called when this is a new namespace, or when the namespace was loaded from
  // disk. Should be called before |Bind|.
  void PopulateFromMetadata(
      AsyncDomStorageDatabase* database,
      SessionStorageMetadata::NamespaceEntry namespace_metadata);

  // Can either be called before |Bind|, or if the source namespace isn't
  // available yet, |SetWaitingForClonePopulation| can be called. Then |Bind|
  // will work, and hold onto the request until after this method is called.
  void PopulateAsClone(
      AsyncDomStorageDatabase* database,
      SessionStorageMetadata::NamespaceEntry namespace_metadata,
      const StorageKeyAreas& areas_to_clone);

  // Resets to a pre-populated and pre-bound state. Used when the owner needs to
  // delete & recreate the database. This call should happen on every namespace
  // at once, and the logic relies on that.
  // TODO(dmurph): It's unclear if we need this or not - we might just want to
  // destruct the object instead of having this method.
  void Reset();

  SessionStorageMetadata::NamespaceEntry namespace_entry() {
    return namespace_entry_;
  }

  bool IsPopulated() const { return state_ == State::kPopulated; }

  // Must be preceded by a call to |PopulateFromMetadata|, |PopulateAsClone|, or
  // |SetWaitingForClonePopulation|. For the later case, |PopulateAsClone| must
  // eventually be called before the PendingReceiver can be bound.
  void Bind(
      mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver);

  bool IsBound() const {
    return !receivers_.empty() || bind_waiting_on_population_;
  }

  // Removes any StorageAreas bound in |OpenArea| that are no longer bound.
  void PurgeUnboundAreas();

  // Removes data for the given StorageKey from this namespace. If there is no
  // data map for that given StorageKey, this does nothing. Expects that this
  // namespace is either populated or waiting for clone population.
  void RemoveStorageKeyData(const blink::StorageKey& storage_key,
                            base::OnceClosure callback);

  // Connects the given database mojo request to the data map for the given
  // StorageKey. Note that the source of |receiver| must have already been
  // access-checked for access to |StorageKey|.
  void OpenArea(const blink::StorageKey& storage_key,
                mojo::PendingReceiver<blink::mojom::StorageArea> receiver);

  // SessionStorageNamespace:
  void Clone(const std::string& clone_to_namespace) override;

  // Clones all namespaces that are waiting to be cloned from this namespace.
  // This handles edge cases like:
  // * If this namespace is populated
  // * If this namespace has a parent
  // * If the parent is populated
  // * If the parent has a parent.
  void CloneAllNamespacesWaitingForClone(
      AsyncDomStorageDatabase* database,
      SessionStorageMetadata* metadata,
      const std::map<std::string, std::unique_ptr<SessionStorageNamespaceImpl>>&
          namespaces_map);

  void FlushAreasForTesting();
  void FlushStorageKeyForTesting(const blink::StorageKey& storage_key);

 private:
  FRIEND_TEST_ALL_PREFIXES(SessionStorageImplTest,
                           PurgeMemoryDoesNotCrashOrHang);
  FRIEND_TEST_ALL_PREFIXES(SessionStorageNamespaceImplTest,
                           ReopenClonedAreaAfterPurge);

  const std::string namespace_id_;
  SessionStorageMetadata::NamespaceEntry namespace_entry_;
  raw_ptr<AsyncDomStorageDatabase> database_ = nullptr;

  raw_ptr<SessionStorageDataMap::Listener> data_map_listener_;
  SessionStorageAreaImpl::RegisterNewAreaMap register_new_map_callback_;
  raw_ptr<Delegate> delegate_;

  State state_ = State::kNotPopulated;
  std::string pending_population_from_parent_namespace_;
  bool bind_waiting_on_population_ = false;
  std::vector<base::OnceClosure> run_after_population_;

  // Namespaces that are waiting for the |Clone| call to be called on this
  // namespace. If this namespace is destructed, then these namespaces are still
  // waiting and should be unblocked.
  base::flat_set<std::string> child_namespaces_waiting_for_clone_call_;

  StorageKeyAreas storage_key_areas_;
  mojo::ReceiverSet<blink::mojom::SessionStorageNamespace> receivers_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_
