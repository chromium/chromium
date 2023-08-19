// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_WEBUI_SHOPPING_LIST_HANDLER_H_
#define COMPONENTS_COMMERCE_CORE_WEBUI_SHOPPING_LIST_HANDLER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/mojom/shopping_list.mojom.h"
#include "components/commerce/core/subscriptions/subscriptions_manager.h"
#include "components/commerce/core/subscriptions/subscriptions_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefService;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement
namespace commerce {

class ShoppingService;
struct PriceInsightsInfo;
struct ProductInfo;

// TODO(b:283833590): Rename this class since it serves for all shopping
// features now.
class ShoppingListHandler : public shopping_list::mojom::ShoppingListHandler,
                            public SubscriptionsObserver {
 public:
  // Handles platform specific tasks.
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    virtual absl::optional<GURL> GetCurrentTabUrl() = 0;

    virtual void ShowInsightsSidePanelUI() = 0;

    virtual const bookmarks::BookmarkNode* GetOrAddBookmarkForCurrentUrl() = 0;

    virtual void OpenUrlInNewTab(const GURL& url) = 0;

    virtual void ShowBookmarkEditorForCurrentUrl() = 0;

    virtual void ShowFeedback() = 0;
  };

  ShoppingListHandler(
      mojo::PendingRemote<shopping_list::mojom::Page> page,
      mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandler> receiver,
      bookmarks::BookmarkModel* bookmark_model,
      ShoppingService* shopping_service,
      PrefService* prefs,
      feature_engagement::Tracker* tracker,
      const std::string& locale,
      std::unique_ptr<Delegate> delegate);
  ShoppingListHandler(const ShoppingListHandler&) = delete;
  ShoppingListHandler& operator=(const ShoppingListHandler&) = delete;
  ~ShoppingListHandler() override;

  // shopping_list::mojom::ShoppingListHandler:
  void GetAllPriceTrackedBookmarkProductInfo(
      GetAllPriceTrackedBookmarkProductInfoCallback callback) override;
  void GetAllShoppingBookmarkProductInfo(
      GetAllShoppingBookmarkProductInfoCallback callback) override;
  void TrackPriceForBookmark(int64_t bookmark_id) override;
  void UntrackPriceForBookmark(int64_t bookmark_id) override;
  void GetProductInfoForCurrentUrl(
      GetProductInfoForCurrentUrlCallback callback) override;
  void GetPriceInsightsInfoForCurrentUrl(
      GetPriceInsightsInfoForCurrentUrlCallback callback) override;
  void ShowInsightsSidePanelUI() override;
  void IsShoppingListEligible(IsShoppingListEligibleCallback callback) override;
  void GetPriceTrackingStatusForCurrentUrl(
      GetPriceTrackingStatusForCurrentUrlCallback callback) override;
  void SetPriceTrackingStatusForCurrentUrl(bool track) override;
  void OpenUrlInNewTab(const GURL& url) override;
  void GetParentBookmarkFolderNameForCurrentUrl(
      GetParentBookmarkFolderNameForCurrentUrlCallback callback) override;
  void ShowBookmarkEditorForCurrentUrl() override;
  void ShowFeedback() override;

  // SubscriptionsObserver
  void OnSubscribe(const CommerceSubscription& subscription,
                   bool succeeded) override;
  void OnUnsubscribe(const CommerceSubscription& subscription,
                     bool succeeded) override;

  static std::vector<shopping_list::mojom::BookmarkProductInfoPtr>
  BookmarkListToMojoList(
      bookmarks::BookmarkModel& model,
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks,
      const std::string& locale);

 private:
  void onPriceTrackResult(int64_t bookmark_id,
                          bookmarks::BookmarkModel* model,
                          bool is_tracking,
                          bool success);

  void OnFetchPriceTrackedBookmarks(
      GetAllPriceTrackedBookmarkProductInfoCallback callback,
      std::vector<const bookmarks::BookmarkNode*> bookmarks);

  void HandleSubscriptionChange(const CommerceSubscription& sub,
                                bool is_tracking);

  void OnFetchProductInfoForCurrentUrl(
      GetProductInfoForCurrentUrlCallback callback,
      const GURL& url,
      const absl::optional<ProductInfo>& info);

  void OnFetchPriceInsightsInfoForCurrentUrl(
      GetPriceInsightsInfoForCurrentUrlCallback callback,
      const GURL& url,
      const absl::optional<PriceInsightsInfo>& info);
  void OnGetPriceTrackingStatusForCurrentUrl(
      GetPriceTrackingStatusForCurrentUrlCallback callback,
      bool tracked);

  mojo::Remote<shopping_list::mojom::Page> remote_page_;
  mojo::Receiver<shopping_list::mojom::ShoppingListHandler> receiver_;
  // The bookmark model, shopping service and tracker will outlive this
  // implementation since it is a keyed service bound to the browser context
  // (which in turn has the same lifecycle as the browser). The web UI that
  // hosts this will be shut down prior to the rest of the browser.
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<ShoppingService> shopping_service_;
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  raw_ptr<feature_engagement::Tracker> tracker_;
  const std::string locale_;
  std::unique_ptr<Delegate> delegate_;
  // Automatically remove this observer from its host when destroyed.
  base::ScopedObservation<ShoppingService, SubscriptionsObserver>
      scoped_observation_{this};

  base::WeakPtrFactory<ShoppingListHandler> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_WEBUI_SHOPPING_LIST_HANDLER_H_
