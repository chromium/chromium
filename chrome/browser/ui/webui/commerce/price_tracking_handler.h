// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMERCE_PRICE_TRACKING_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_COMMERCE_PRICE_TRACKING_HANDLER_H_

#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/mojom/price_tracking.mojom.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/subscriptions_observer.h"
#include "components/feature_engagement/public/tracker.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace commerce {

class PriceTrackingHandler : public price_tracking::mojom::PriceTrackingHandler,
                             public bookmarks::BaseBookmarkModelObserver,
                             public SubscriptionsObserver {
 public:
  explicit PriceTrackingHandler(
      mojo::PendingRemote<price_tracking::mojom::Page> remote_page,
      mojo::PendingReceiver<price_tracking::mojom::PriceTrackingHandler>
          receiver,
      content::WebUI* web_ui,
      ShoppingService* shopping_service,
      feature_engagement::Tracker* tracker,
      bookmarks::BookmarkModel* bookmark_model);
  PriceTrackingHandler(const PriceTrackingHandler&) = delete;
  PriceTrackingHandler& operator=(const PriceTrackingHandler&) = delete;
  ~PriceTrackingHandler() override;

  // price_tracking::mojom::PriceTrackingHandler
  void TrackPriceForBookmark(int64_t bookmark_id) override;
  void UntrackPriceForBookmark(int64_t bookmark_id) override;
  void SetPriceTrackingStatusForCurrentUrl(bool track) override;
  void GetAllPriceTrackedBookmarkProductInfo(
      GetAllPriceTrackedBookmarkProductInfoCallback callback) override;
  void GetAllShoppingBookmarkProductInfo(
      GetAllShoppingBookmarkProductInfoCallback callback) override;
  void GetShoppingCollectionBookmarkFolderId(
      GetShoppingCollectionBookmarkFolderIdCallback callback) override;
  void GetParentBookmarkFolderNameForCurrentUrl(
      GetParentBookmarkFolderNameForCurrentUrlCallback callback) override;
  void ShowBookmarkEditorForCurrentUrl() override;

  // bookmarks::BaseBookmarkModelObserver
  void BookmarkModelChanged() override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;

  // SubscriptionsObserver
  void OnSubscribe(const CommerceSubscription& subscription,
                   bool succeeded) override;
  void OnUnsubscribe(const CommerceSubscription& subscription,
                     bool succeeded) override;

  void HandleSubscriptionChange(const CommerceSubscription& sub,
                                bool is_tracking);

  static std::vector<shared::mojom::BookmarkProductInfoPtr>
  BookmarkListToMojoList(
      bookmarks::BookmarkModel& model,
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks,
      const std::string& locale);

 private:
  std::optional<GURL> GetCurrentTabUrl();
  ukm::SourceId GetCurrentTabUkmSourceId();

  const bookmarks::BookmarkNode* GetOrAddBookmarkForCurrentUrl();

  void OnPriceTrackResult(int64_t bookmark_id,
                          bookmarks::BookmarkModel* model,
                          bool is_tracking,
                          bool success);
  void OnFetchPriceTrackedBookmarks(
      GetAllPriceTrackedBookmarkProductInfoCallback callback,
      std::vector<const bookmarks::BookmarkNode*> bookmarks);

  mojo::Remote<price_tracking::mojom::Page> remote_page_;
  mojo::Receiver<price_tracking::mojom::PriceTrackingHandler> receiver_;

  std::string locale_;
  raw_ptr<content::WebUI> web_ui_;
  raw_ptr<feature_engagement::Tracker> tracker_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<ShoppingService> shopping_service_;

  // Automatically remove this observer from its host when destroyed.
  base::ScopedObservation<ShoppingService, SubscriptionsObserver>
      scoped_subscriptions_observation_{this};
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      scoped_bookmark_model_observation_{this};

  base::WeakPtrFactory<PriceTrackingHandler> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_WEBUI_COMMERCE_PRICE_TRACKING_HANDLER_H_
