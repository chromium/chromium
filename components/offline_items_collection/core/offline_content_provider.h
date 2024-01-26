// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OFFLINE_CONTENT_PROVIDER_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OFFLINE_CONTENT_PROVIDER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "components/offline_items_collection/core/launch_location.h"
#include "components/offline_items_collection/core/open_params.h"
#include "components/offline_items_collection/core/rename_result.h"
#include "components/offline_items_collection/core/update_delta.h"
#include "url/gurl.h"

namespace offline_items_collection {

struct ContentId;
struct OfflineItem;
struct OfflineItemShareInfo;
struct OfflineItemVisuals;

// A provider of a set of OfflineItems that are meant to be exposed to the UI.
class OfflineContentProvider {
 public:
  using OfflineItemList = std::vector<OfflineItem>;
  using VisualsCallback =
      base::OnceCallback<void(const ContentId&,
                              std::unique_ptr<OfflineItemVisuals>)>;
  using ShareCallback =
      base::OnceCallback<void(const ContentId&,
                              std::unique_ptr<OfflineItemShareInfo>)>;
  using MultipleItemCallback = base::OnceCallback<void(const OfflineItemList&)>;
  using SingleItemCallback =
      base::OnceCallback<void(const std::optional<OfflineItem>&)>;
  using RenameCallback = base::OnceCallback<void(RenameResult)>;
  using DownloadRenameCallback = base::OnceCallback<RenameCallback>;

  // Used by GetVisualsForItem to specify which visuals are needed.
  struct GetVisualsOptions {
    bool get_icon;
    bool get_custom_favicon;
    static GetVisualsOptions NoVisuals() { return GetVisualsOptions(); }
    static GetVisualsOptions IconOnly() {
      GetVisualsOptions options;
      options.get_icon = true;
      return options;
    }
    static GetVisualsOptions CustomFaviconOnly() {
      GetVisualsOptions options;
      options.get_custom_favicon = true;
      return options;
    }
    static GetVisualsOptions IconAndCustomFavicon() {
      GetVisualsOptions options;
      options.get_icon = true;
      options.get_custom_favicon = true;
      return options;
    }
  };

  // An observer class that should be notified of relevant changes to the
  // underlying data source.
  // For the Observer that maintains its own cache of items, populated via
  // GetAllItems method, it is possible to receive notifications that are
  // out-of-sync with the cache content. See notes on the methods.
  class Observer : public base::CheckedObserver {
   public:
    // Called when one or more OfflineItems have been added and should be shown
    // in the UI.  This should only be called for actual new items that are
    // added during this session.  Existing items that are loaded on startup do
    // not need to trigger this.  Most UI surfaces should query the existing
    // list of items if they want to get the current state of the world.
    // If Observer maintains a cache of items, the specified items may already
    // be in the cache, in which case this call has to be ignored.
    virtual void OnItemsAdded(const OfflineItemList& items) = 0;

    // Called when the OfflineItem represented by |id| should be removed from
    // the UI.
    // If Observer maintains a cache of items, item with the specified id may
    // not be present in the cache, in which case this call should be ignored.
    virtual void OnItemRemoved(const ContentId& id) = 0;

    // Called when the contents of |item| have been updated and the UI should be
    // refreshed for that item.
    // TODO(dtrainor): Make this take a list of OfflineItems.
    // If Observer maintains a cache of items, the changes may already be
    // applied to the items in the cache, so there is no difference between
    // items. This can be used in conjunction with the |update_delta| to
    // determine whether this call should be ignored.
    virtual void OnItemUpdated(
        const OfflineItem& item,
        const std::optional<UpdateDelta>& update_delta) = 0;

    // Called right before this object gets destroyed, to lets observers
    // perform cleanup.
    virtual void OnContentProviderGoingDown() = 0;

   protected:
    ~Observer() override = default;
  };

  // Called to trigger opening an OfflineItem represented by |id|.
  virtual void OpenItem(const OpenParams& open_params, const ContentId& id) = 0;

  // Called to trigger removal of an OfflineItem represented by |id|.
  virtual void RemoveItem(const ContentId& id) = 0;

  // Called to cancel a download of an OfflineItem represented by |id|.
  virtual void CancelDownload(const ContentId& id) = 0;

  // Called to pause a download of an OfflineItem represented by |id|.
  virtual void PauseDownload(const ContentId& id) = 0;

  // Called to resume a paused download of an OfflineItem represented by |id|.
  virtual void ResumeDownload(const ContentId& id) = 0;

  // Requests for an OfflineItem represented by |id|. The implementer should
  // post any replies even if the result is available immediately to prevent
  // reentrancy and for consistent behavior.
  virtual void GetItemById(const ContentId& id,
                           SingleItemCallback callback) = 0;

  // Requests for all the OfflineItems from this particular provider. The
  // implementer should post any replies even if the results are available
  // immediately to prevent reentrancy and for consistent behavior.
  virtual void GetAllItems(MultipleItemCallback callback) = 0;

  // Asks for an OfflineItemVisuals struct for an OfflineItem represented by
  // |id| or |nullptr| if one doesn't exist.  The implementer should post any
  // replies even if the results are available immediately to prevent reentrancy
  // and for consistent behavior. |options| may be set to let the implementer
  // know that it need not create the |icon| or |custom_favicon| members.
  // |callback| should be called no matter what (error, unavailable content,
  // etc.).
  virtual void GetVisualsForItem(const ContentId& id,
                                 GetVisualsOptions options,
                                 VisualsCallback callback) = 0;

  // Asks for the right URI to use to share an OfflineItem represented by |id|
  // or |nullptr| if there is no associated information to use to share the
  // item.  Implementer should post any replies even if the results are
  // available immediately to prevent reentrancy and for consistent behavior.
  // |callback| should be called no matter what (error, unavailable content,
  // etc.).
  virtual void GetShareInfoForItem(const ContentId& id,
                                   ShareCallback callback) = 0;

  // Called to rename a downloaded OfflineItem represented by |id|, new name is
  // given by |name|. Implementer should post and replies the results of rename
  // attempt.
  virtual void RenameItem(const ContentId& id,
                          const std::string& name,
                          RenameCallback callback) = 0;

  // Adds an observer that should be notified of OfflineItem list modifications.
  void AddObserver(Observer* observer);

  // Removes an observer.  No further notifications should be sent to it.
  void RemoveObserver(Observer* observer);

 protected:
  OfflineContentProvider();
  virtual ~OfflineContentProvider();

  // Used in tests.
  bool HasObserver(Observer* observer);

  // Notify observers via OnItemsAdded(), OnItemRemoved() or OnItemUpdated().
  void NotifyItemsAdded(const OfflineItemList& items);
  void NotifyItemRemoved(const ContentId& id);
  void NotifyItemUpdated(const OfflineItem& item,
                         const std::optional<UpdateDelta>& update_delta);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OFFLINE_CONTENT_PROVIDER_H_
