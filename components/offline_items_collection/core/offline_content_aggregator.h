// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OFFLINE_CONTENT_AGGREGATOR_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OFFLINE_CONTENT_AGGREGATOR_H_

#include <map>
#include <set>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "url/gurl.h"

namespace offline_items_collection {

struct OfflineItem;

// An implementation of OfflineContentProvider that aggregates multiple other
// providers into a single set of data.  See the OfflineContentProvider header
// for comments on expected behavior of the interface.
//
// Routing to the correct provider:
// - Providers must be registered with a unique namespace.  The OfflineItems
//   created by the provider must also be tagged with the same namespace so that
//   actions taken on the OfflineItem can be routed to the correct internal
//   provider.  The namespace must also be consistent across startups.
//
// Methods on OfflineContentAggregator should be called from the UI thread.
class OfflineContentAggregator : public OfflineContentProvider,
                                 public OfflineContentProvider::Observer,
                                 public base::SupportsUserData,
                                 public KeyedService {
 public:
  OfflineContentAggregator();

  OfflineContentAggregator(const OfflineContentAggregator&) = delete;
  OfflineContentAggregator& operator=(const OfflineContentAggregator&) = delete;

  ~OfflineContentAggregator() override;

  // Creates a unique namespace with the given prefix. Should be called to get
  // the namespace for registration in order to avoid namespace collision.
  // For normal profile, the prefix itself is returned. For incognito profiles,
  // an auto-incrementing number is added to the prefix and returned.
  // Auto-incrementing should be used only for incognito, since for incognito,
  // data should not be persisted after browser restarts and hence change of
  // ContentId(s) across restarts will not be an issue.
  static std::string CreateUniqueNameSpace(const std::string& prefix,
                                           bool is_off_the_record);

  // Registers a provider and associates it with all OfflineItems with
  // |name_space|.  UI actions taken on OfflineItems with |name_space| will be
  // routed to |provider|.  |provider| is expected to only expose OfflineItems
  // with |name_space| set.
  // It is okay to register the same provider with multiple unique namespaces.
  // A provider needs to handle calls to GetAllItems properly (not return
  // any items for a namespace that it didn't register).
  void RegisterProvider(const std::string& name_space,
                        OfflineContentProvider* provider);

  // Removes the OfflineContentProvider associated with |name_space| from this
  // aggregator.
  void UnregisterProvider(const std::string& name_space);

  // OfflineContentProvider implementation.
  void OpenItem(const OpenParams& open_params, const ContentId& id) override;
  void RemoveItem(const ContentId& id) override;
  void CancelDownload(const ContentId& id) override;
  void PauseDownload(const ContentId& id) override;
  void ResumeDownload(const ContentId& id) override;
  void GetItemById(const ContentId& id, SingleItemCallback callback) override;
  void GetAllItems(MultipleItemCallback callback) override;
  void GetVisualsForItem(const ContentId& id,
                         GetVisualsOptions options,
                         VisualsCallback callback) override;
  void GetShareInfoForItem(const ContentId& id,
                           ShareCallback callback) override;
  void RenameItem(const ContentId& id,
                  const std::string& name,
                  RenameCallback callback) override;

 private:
  // OfflineContentProvider::Observer implementation.
  void OnItemsAdded(const OfflineItemList& items) override;
  void OnItemRemoved(const ContentId& id) override;
  void OnItemUpdated(const OfflineItem& item,
                     const std::optional<UpdateDelta>& update_delta) override;
  void OnContentProviderGoingDown() override;

  void OnGetAllItemsDone(OfflineContentProvider* provider,
                         const OfflineItemList& items);
  void OnGetItemByIdDone(SingleItemCallback callback,
                         const std::optional<OfflineItem>& item);

  // Stores a map of name_space -> OfflineContentProvider.  These
  // OfflineContentProviders are all aggregated by this class and exposed to the
  // consumer as a single list.
  using OfflineProviderMap =
      std::map<std::string, raw_ptr<OfflineContentProvider, CtnExperimental>>;
  OfflineProviderMap providers_;

  // Used by GetAllItems and the corresponding callback.
  std::vector<MultipleItemCallback> multiple_item_get_callbacks_;
  OfflineItemList aggregated_items_;
  std::set<raw_ptr<OfflineContentProvider, SetExperimental>> pending_providers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OfflineContentAggregator> weak_ptr_factory_{this};
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OFFLINE_CONTENT_AGGREGATOR_H_
