// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/price_tracking/shopping_list_ui_tab_helper.h"

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace commerce {

namespace {
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("shopping_list_ui_image_fetcher",
                                        R"(
        semantics {
          sender: "Product image fetcher for the shopping list feature."
          description:
            "Retrieves the image for a product that is displayed on the active "
            "web page. This will be shown to the user as part of the "
            "bookmarking or price tracking action."
          trigger:
            "On navigation, if the URL of the page is determined to be a "
            "product that can be price tracked, we will attempt to fetch the "
            "image for it."
          data:
            "An image of a product that can be price tracked."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This fetch is enabled for any user with the 'Shopping List' "
            "feature enabled."
          chrome_policy {
            ShoppingListEnabled {
              policy_options {mode: MANDATORY}
              ShoppingListEnabled: true
            }
          }
        })");

constexpr char kImageFetcherUmaClient[] = "ShoppingList";
}  // namespace

ShoppingListUiTabHelper::ShoppingListUiTabHelper(
    content::WebContents* content,
    ShoppingService* shopping_service,
    image_fetcher::ImageFetcherService* image_fetcher_service,
    PrefService* prefs)
    : content::WebContentsObserver(content),
      content::WebContentsUserData<ShoppingListUiTabHelper>(*content),
      shopping_service_(shopping_service),
      prefs_(prefs) {
  if (image_fetcher_service) {
    // TODO(1360846): Consider using the in-memory cache instead.
    image_fetcher_ = image_fetcher_service->GetImageFetcher(
        image_fetcher::ImageFetcherConfig::kNetworkOnly);
  } else {
    CHECK_IS_TEST();
  }

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(content->GetBrowserContext());

  if (bookmark_model) {
    scoped_observation_.Observe(BookmarkModelFactory::GetForBrowserContext(
        content->GetBrowserContext()));
  } else {
    CHECK_IS_TEST();
  }

  if (!shopping_service_) {
    CHECK_IS_TEST();
  }
}

ShoppingListUiTabHelper::~ShoppingListUiTabHelper() = default;

// static
void ShoppingListUiTabHelper::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kShouldShowPriceTrackFUEBubble, true);
  registry->RegisterBooleanPref(prefs::kShouldShowSidePanelBookmarkTab, false);
}

void ShoppingListUiTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!load_details.is_in_active_page ||
      (web_contents()->GetLastCommittedURL() ==
           load_details.previous_main_frame_url &&
       is_initial_navigation_committed_)) {
    is_initial_navigation_committed_ = true;
    return;
  }

  last_fetched_image_ = gfx::Image();
  last_fetched_image_url_ = GURL();
  is_cluster_id_tracked_by_user_ = false;
  cluster_id_for_page_.reset();

  if (!shopping_service_ || !shopping_service_->IsShoppingListEligible())
    return;

  // Cancel any pending callbacks by invalidating any weak pointers.
  weak_ptr_factory_.InvalidateWeakPtrs();

  shopping_service_->GetProductInfoForUrl(
      web_contents()->GetLastCommittedURL(),
      base::BindOnce(&ShoppingListUiTabHelper::HandleProductInfoResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  UpdatePriceTrackingIconView();
}

void ShoppingListUiTabHelper::BookmarkModelChanged() {}

void ShoppingListUiTabHelper::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked) {
  UpdatePriceTrackingIconView();
}

void ShoppingListUiTabHelper::BookmarkMetaInfoChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  if (!commerce::IsProductBookmark(model, node))
    return;

  UpdatePriceTrackingStateFromSubscriptions();
  UpdatePriceTrackingIconView();
}

bool ShoppingListUiTabHelper::ShouldShowPriceTrackingIconView() {
  return shopping_service_ && shopping_service_->IsShoppingListEligible() &&
         !last_fetched_image_.IsEmpty();
}

void ShoppingListUiTabHelper::HandleProductInfoResponse(
    const GURL& url,
    const absl::optional<ProductInfo>& info) {
  if (url != web_contents()->GetLastCommittedURL())
    return;

  if (!info.has_value() || info.value().image_url.is_empty())
    return;

  cluster_id_for_page_.emplace(info->product_cluster_id);
  UpdatePriceTrackingStateFromSubscriptions();

  // TODO(1360850): Delay this fetch by possibly waiting until page load has
  //                finished.
  image_fetcher_->FetchImage(
      info.value().image_url,
      base::BindOnce(&ShoppingListUiTabHelper::HandleImageFetcherResponse,
                     weak_ptr_factory_.GetWeakPtr(), info.value().image_url),
      image_fetcher::ImageFetcherParams(kTrafficAnnotation,
                                        kImageFetcherUmaClient));
}

void ShoppingListUiTabHelper::UpdatePriceTrackingStateFromSubscriptions() {
  if (!cluster_id_for_page_.has_value())
    return;

  shopping_service_->IsClusterIdTrackedByUser(
      cluster_id_for_page_.value(),
      base::BindOnce(
          [](base::WeakPtr<ShoppingListUiTabHelper> helper, bool is_tracked) {
            if (!helper) {
              return;
            }

            helper->is_cluster_id_tracked_by_user_ = is_tracked;
            helper->UpdatePriceTrackingIconView();
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void ShoppingListUiTabHelper::HandleImageFetcherResponse(
    const GURL image_url,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& request_metadata) {
  if (image.IsEmpty())
    return;

  last_fetched_image_url_ = image_url;
  last_fetched_image_ = image;

  UpdatePriceTrackingIconView();
}

const gfx::Image& ShoppingListUiTabHelper::GetProductImage() {
  return last_fetched_image_;
}

const GURL& ShoppingListUiTabHelper::GetProductImageURL() {
  return last_fetched_image_url_;
}

bool ShoppingListUiTabHelper::IsPriceTracking() {
  if (is_cluster_id_tracked_by_user_) {
    return true;
  }

  if (!web_contents())
    return false;

  bookmarks::BookmarkModel* const bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  const bookmarks::BookmarkNode* bookmark_node =
      bookmark_model->GetMostRecentlyAddedUserNodeForURL(
          web_contents()->GetLastCommittedURL());
  return commerce::IsBookmarkPriceTracked(bookmark_model, bookmark_node);
}

void ShoppingListUiTabHelper::UpdatePriceTrackingIconView() {
  DCHECK(web_contents());

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());

  if (!browser || !browser->window()) {
    return;
  }

  browser->window()->UpdatePageActionIcon(PageActionIconType::kPriceTracking);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ShoppingListUiTabHelper);

}  // namespace commerce
