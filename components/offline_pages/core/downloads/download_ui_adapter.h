// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_DOWNLOADS_DOWNLOAD_UI_ADAPTER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_DOWNLOADS_DOWNLOAD_UI_ADAPTER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_page_visuals.h"
#include "url/gurl.h"

using ContentId = offline_items_collection::ContentId;
using LaunchLocation = offline_items_collection::LaunchLocation;
using OfflineContentProvider = offline_items_collection::OfflineContentProvider;
using OfflineContentAggregator =
    offline_items_collection::OfflineContentAggregator;
using OfflineItem = offline_items_collection::OfflineItem;
using UpdateDelta = offline_items_collection::UpdateDelta;
using OfflineItemShareInfo = offline_items_collection::OfflineItemShareInfo;

namespace offline_pages {
class VisualsDecoder;

// C++ side of the UI Adapter. Mimics DownloadManager/Item/History (since we
// share UI with Downloads).
// An instance of this class is owned by OfflinePageModel and is shared between
// UI components if needed. It manages the cache of OfflineItems, which are fed
// to the OfflineContentAggregator which subsequently takes care of notifying
// observers of items being loaded, added, deleted etc. The creator of the
// adapter also passes in the Delegate that determines which items in the
// underlying OfflinePage backend are to be included (visible) in the
// collection.
class DownloadUIAdapter : public OfflineContentProvider,
                          public OfflinePageModel::Observer,
                          public RequestCoordinator::Observer,
                          public base::SupportsUserData::Data {
 public:
  // Delegate, used to customize behavior of this Adapter.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Returns true if the page or request with the specified Client Id should
    // be visible in the collection of items exposed by this Adapter. This also
    // indicates if Observers will be notified about changes for the given page.
    virtual bool IsVisibleInUI(const ClientId& client_id) = 0;

    // Delegates need a reference to the UI adapter in order to notify it about
    // visibility changes.
    virtual void SetUIAdapter(DownloadUIAdapter* ui_adapter) = 0;

    // Opens an offline item.
    virtual void OpenItem(const OfflineItem& item,
                          int64_t offline_id,
                          LaunchLocation launch_location) = 0;

    // Suppresses the download complete notification
    // depending on flags and origin.
    virtual bool MaybeSuppressNotification(const std::string& origin,
                                           const ClientId& id) = 0;

    // Share item to other apps.
    virtual void GetShareInfoForItem(const ContentId& id,
                                     ShareCallback share_callback) = 0;
  };

  // Create the adapter. visuals_decoder may be null, in which case,
  // thumbnails and favicons will not be provided through GetVisualsForItem.
  DownloadUIAdapter(OfflineContentAggregator* aggregator,
                    OfflinePageModel* model,
                    RequestCoordinator* coordinator,
                    std::unique_ptr<VisualsDecoder> visuals_decoder,
                    std::unique_ptr<Delegate> delegate);
  ~DownloadUIAdapter() override;

  static DownloadUIAdapter* FromOfflinePageModel(OfflinePageModel* model);
  static void AttachToOfflinePageModel(
      std::unique_ptr<DownloadUIAdapter> adapter,
      OfflinePageModel* model);

  // OfflineContentProvider implementation.
  void OpenItem(LaunchLocation location, const ContentId& id) override;
  void RemoveItem(const ContentId& id) override;
  void CancelDownload(const ContentId& id) override;
  void PauseDownload(const ContentId& id) override;
  void ResumeDownload(const ContentId& id, bool has_user_gesture) override;
  void GetItemById(
      const ContentId& id,
      OfflineContentProvider::SingleItemCallback callback) override;
  void GetAllItems(
      OfflineContentProvider::MultipleItemCallback callback) override;
  void GetVisualsForItem(const ContentId& id,
                         GetVisualsOptions options,
                         VisualsCallback callback) override;
  void GetShareInfoForItem(const ContentId& id,
                           ShareCallback share_callback) override;
  void RenameItem(const ContentId& id,
                  const std::string& name,
                  RenameCallback callback) override;
  void AddObserver(OfflineContentProvider::Observer* observer) override;
  void RemoveObserver(OfflineContentProvider::Observer* observer) override;

  // OfflinePageModel::Observer
  void OfflinePageModelLoaded(OfflinePageModel* model) override;
  void OfflinePageAdded(OfflinePageModel* model,
                        const OfflinePageItem& added_page) override;
  void OfflinePageDeleted(const OfflinePageItem& item) override;
  void ThumbnailAdded(OfflinePageModel* model,
                      const int64_t offline_id,
                      const std::string& thumbnail) override;

  // RequestCoordinator::Observer
  void OnAdded(const SavePageRequest& request) override;
  void OnCompleted(const SavePageRequest& request,
                   RequestNotifier::BackgroundSavePageResult status) override;
  void OnChanged(const SavePageRequest& request) override;
  void OnNetworkProgress(const SavePageRequest& request,
                         int64_t received_bytes) override;

  Delegate* delegate() { return delegate_.get(); }

 private:
  using VisualResultCallback = base::OnceCallback<void(
      std::unique_ptr<offline_items_collection::OfflineItemVisuals>)>;

  // Task callbacks.
  void PauseDownloadContinuation(
      const std::string& guid,
      std::vector<std::unique_ptr<SavePageRequest>> requests);
  void ResumeDownloadContinuation(
      const std::string& guid,
      std::vector<std::unique_ptr<SavePageRequest>> requests);
  void OnOfflinePagesLoaded(
      OfflineContentProvider::MultipleItemCallback callback,
      std::unique_ptr<OfflineContentProvider::OfflineItemList> offline_items,
      const MultipleOfflinePageItemResult& pages);
  void OnVisualsLoaded(GetVisualsOptions options,
                       VisualResultCallback callback,
                       std::unique_ptr<OfflinePageVisuals> visuals);

  void DecodeThumbnail(std::unique_ptr<OfflinePageVisuals> visuals,
                       GetVisualsOptions options,
                       VisualResultCallback callback);
  void DecodeFavicon(std::string favicon,
                     GetVisualsOptions options,
                     VisualResultCallback callback,
                     const gfx::Image& thumbnail);

  void OnRequestsLoaded(
      OfflineContentProvider::MultipleItemCallback callback,
      std::unique_ptr<OfflineContentProvider::OfflineItemList> offline_items,
      std::vector<std::unique_ptr<SavePageRequest>> requests);
  void OnPageGetForVisuals(const ContentId& id,
                           GetVisualsOptions options,
                           VisualsCallback visuals_callback,
                           const std::vector<OfflinePageItem>& pages);
  void OnPageGetForGetItem(const ContentId& id,
                           OfflineContentProvider::SingleItemCallback callback,
                           const std::vector<OfflinePageItem>& pages);
  void OnAllRequestsGetForGetItem(
      const ContentId& id,
      OfflineContentProvider::SingleItemCallback callback,
      std::vector<std::unique_ptr<SavePageRequest>> requests);

  void OnPageGetForOpenItem(LaunchLocation location,
                            const std::vector<OfflinePageItem>& pages);
  void OnPageGetForThumbnailAdded(const OfflinePageItem* page);

  void OnDeletePagesDone(DeletePageResult result);

  void OpenItemByGuid(const std::string& guid);

  // A valid offline content aggregator, supplied at construction.
  OfflineContentAggregator* aggregator_;

  // Always valid, this class is a member of the model.
  OfflinePageModel* model_;

  // Always valid, a service.
  RequestCoordinator* request_coordinator_;

  // May be null if thumbnails are not required.
  std::unique_ptr<VisualsDecoder> visuals_decoder_;

  // A delegate, supplied at construction.
  std::unique_ptr<Delegate> delegate_;

  // The observers.
  base::ObserverList<OfflineContentProvider::Observer>::Unchecked observers_;

  base::WeakPtrFactory<DownloadUIAdapter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DownloadUIAdapter);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_DOWNLOADS_DOWNLOAD_UI_ADAPTER_H_
