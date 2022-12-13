// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_PRICE_TRACKING_SHOPPING_LIST_UI_TAB_HELPER_H_
#define CHROME_BROWSER_UI_COMMERCE_PRICE_TRACKING_SHOPPING_LIST_UI_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/shopping_service.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/image/image.h"

class GURL;
class PrefService;

namespace content {
class WebContents;
}  // namespace content

namespace image_fetcher {
class ImageFetcherService;
}

namespace commerce {

// This tab helper is used to update and maintain the state of the shopping list
// and price tracking UI on desktop.
class ShoppingListUiTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ShoppingListUiTabHelper>,
      public bookmarks::BaseBookmarkModelObserver {
 public:
  ~ShoppingListUiTabHelper() override;
  ShoppingListUiTabHelper(const ShoppingListUiTabHelper& other) = delete;
  ShoppingListUiTabHelper& operator=(const ShoppingListUiTabHelper& other) =
      delete;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Get the image for the last fetched product URL. A reference to this object
  // should not be kept directly, if one is needed, a copy should be made.
  virtual const gfx::Image& GetProductImage();
  // Return whether the PriceTrackingIconView is visible.
  virtual bool ShouldShowPriceTrackingIconView();

  // The URL for the last fetched product image. A reference to this object
  // should not be kept directly, if one is needed, a copy should be made.
  const GURL& GetProductImageURL();

  // Returns whether the current page has a product that is being price tracked.
  virtual bool IsPriceTracking();

  // content::WebContentsObserver implementation
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

  // bookmarks::BaseBookmarkModelObserver
  void BookmarkModelChanged() override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& no_longer_bookmarked) override;
  void BookmarkMetaInfoChanged(bookmarks::BookmarkModel* model,
                               const bookmarks::BookmarkNode* node) override;

 protected:
  ShoppingListUiTabHelper(
      content::WebContents* contents,
      ShoppingService* shopping_service,
      image_fetcher::ImageFetcherService* image_fetcher_service,
      PrefService* prefs);

 private:
  friend class content::WebContentsUserData<ShoppingListUiTabHelper>;

  void HandleProductInfoResponse(const GURL& url,
                                 const absl::optional<ProductInfo>& info);

  void HandleImageFetcherResponse(
      const GURL image_url,
      const gfx::Image& image,
      const image_fetcher::RequestMetadata& request_metadata);

  void UpdatePriceTrackingIconView();

  // Update the flag tracking the price tracking state of the product from
  // subscriptions.
  void UpdatePriceTrackingStateFromSubscriptions();

  // The shopping service is tied to the lifetime of the browser context
  // which will always outlive this tab helper.
  raw_ptr<ShoppingService, DanglingUntriaged> shopping_service_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  // The URL of the last product image that was fetched.
  GURL last_fetched_image_url_;

  // The last image that was fetched. See |last_image_fetched_url_| for the
  // URL that was used.
  gfx::Image last_fetched_image_;

  // Whether the product shown on the current page is tracked by the user.
  bool is_cluster_id_tracked_by_user_{false};

  // The cluster ID for the current page, if applicable.
  absl::optional<uint64_t> cluster_id_for_page_;

  // Automatically remove this observer from its host when destroyed.
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      scoped_observation_{this};

  base::WeakPtrFactory<ShoppingListUiTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_COMMERCE_PRICE_TRACKING_SHOPPING_LIST_UI_TAB_HELPER_H_
