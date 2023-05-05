// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/price_tracking/shopping_list_ui_tab_helper.h"

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
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

constexpr base::TimeDelta kDelayPriceTrackingchip = base::Seconds(1);

bool ShouldDelayChipUpdate() {
  return static_cast<commerce::PriceTrackingChipExperimentVariation>(
             commerce::kCommercePriceTrackingChipExperimentVariation.Get()) ==
         commerce::PriceTrackingChipExperimentVariation::kDelayChip;
}
}  // namespace

ShoppingListUiTabHelper::ShoppingListUiTabHelper(
    content::WebContents* content,
    ShoppingService* shopping_service,
    bookmarks::BookmarkModel* model,
    image_fetcher::ImageFetcher* image_fetcher)
    : content::WebContentsObserver(content),
      content::WebContentsUserData<ShoppingListUiTabHelper>(*content),
      shopping_service_(shopping_service),
      bookmark_model_(model),
      image_fetcher_(image_fetcher) {
  if (!image_fetcher_) {
    CHECK_IS_TEST();
  }

  if (shopping_service_) {
    scoped_observation_.Observe(shopping_service_);
  } else {
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
      IsInitialNavigationCommitted(load_details) ||
      IsSameDocumentWithSameCommittedUrl(load_details)) {
    is_initial_navigation_committed_ = true;
    return;
  }

  last_fetched_image_ = gfx::Image();
  last_fetched_image_url_ = GURL();
  is_cluster_id_tracked_by_user_ = false;
  cluster_id_for_page_.reset();
  pending_tracking_state_.reset();
  is_first_load_for_nav_finished_ = false;

  if (!shopping_service_ || !shopping_service_->IsShoppingListEligible())
    return;

  // Cancel any pending callbacks by invalidating any weak pointers.
  weak_ptr_factory_.InvalidateWeakPtrs();

  UpdatePriceTrackingIconView();

  shopping_service_->GetProductInfoForUrl(
      web_contents()->GetLastCommittedURL(),
      base::BindOnce(&ShoppingListUiTabHelper::HandleProductInfoResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool ShoppingListUiTabHelper::IsInitialNavigationCommitted(
    const content::LoadCommittedDetails& load_details) {
  return load_details.previous_main_frame_url ==
             web_contents()->GetLastCommittedURL() &&
         is_initial_navigation_committed_;
}

bool ShoppingListUiTabHelper::IsSameDocumentWithSameCommittedUrl(
    const content::LoadCommittedDetails& load_details) {
  return load_details.previous_main_frame_url ==
             web_contents()->GetLastCommittedURL() &&
         load_details.is_same_document;
}

void ShoppingListUiTabHelper::DidStopLoading() {
  if (!web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame() ||
      !ShouldDelayChipUpdate() || is_first_load_for_nav_finished_) {
    return;
  }
  is_first_load_for_nav_finished_ = true;

  TriggerUpdateForIconView();
}

void ShoppingListUiTabHelper::TriggerUpdateForIconView() {
  if (!ShouldDelayChipUpdate()) {
    UpdatePriceTrackingIconView();
    return;
  }

  if (last_fetched_image_.IsEmpty() || !is_first_load_for_nav_finished_) {
    return;
  }

  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ShoppingListUiTabHelper::UpdatePriceTrackingIconView,
                         weak_ptr_factory_.GetWeakPtr()),
          kDelayPriceTrackingchip);
}

void ShoppingListUiTabHelper::OnSubscribe(
    const CommerceSubscription& subscription,
    bool succeeded) {
  HandleSubscriptionChange(subscription);
}

void ShoppingListUiTabHelper::OnUnsubscribe(
    const CommerceSubscription& subscription,
    bool succeeded) {
  HandleSubscriptionChange(subscription);
}

void ShoppingListUiTabHelper::HandleSubscriptionChange(
    const CommerceSubscription& sub) {
  if (sub.id_type == IdentifierType::kProductClusterId &&
      sub.id == base::NumberToString(
                    cluster_id_for_page_.value_or(kInvalidSubscriptionId))) {
    UpdatePriceTrackingStateFromSubscriptions();
    UpdatePriceTrackingIconView();
  }
}

void ShoppingListUiTabHelper::SetShoppingServiceForTesting(
    ShoppingService* shopping_service) {
  CHECK_IS_TEST();
  shopping_service_ = shopping_service;
  scoped_observation_.Reset();
  if (shopping_service_) {
    scoped_observation_.Observe(shopping_service_);
  }
}

bool ShoppingListUiTabHelper::ShouldShowPriceTrackingIconView() {
  bool should_show = shopping_service_ &&
                     shopping_service_->IsShoppingListEligible() &&
                     !last_fetched_image_.IsEmpty();

  return ShouldDelayChipUpdate()
             ? should_show && is_first_load_for_nav_finished_
             : should_show;
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

void ShoppingListUiTabHelper::SetPriceTrackingState(
    bool enable,
    bool is_new_bookmark,
    base::OnceCallback<void(bool)> callback) {
  const bookmarks::BookmarkNode* node =
      bookmark_model_->GetMostRecentlyAddedUserNodeForURL(
          web_contents()->GetLastCommittedURL());

  base::OnceCallback<void(bool)> wrapped_callback = base::BindOnce(
      [](base::WeakPtr<ShoppingListUiTabHelper> helper,
         base::OnceCallback<void(bool)> callback, bool success) {
        if (helper) {
          if (success) {
            helper->is_cluster_id_tracked_by_user_ =
                helper->pending_tracking_state_.value();
          }
          helper->pending_tracking_state_.reset();
        }

        std::move(callback).Run(success);
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  pending_tracking_state_.emplace(enable);

  if (node) {
    commerce::SetPriceTrackingStateForBookmark(
        shopping_service_.get(), bookmark_model_.get(), node, enable,
        std::move(wrapped_callback), enable && is_new_bookmark);
  } else {
    DCHECK(!enable);
    absl::optional<commerce::ProductInfo> info =
        shopping_service_->GetAvailableProductInfoForUrl(
            web_contents()->GetLastCommittedURL());
    if (info.has_value()) {
      commerce::SetPriceTrackingStateForClusterId(
          shopping_service_.get(), bookmark_model_, info->product_cluster_id,
          enable, std::move(wrapped_callback));
    }
  }
}

void ShoppingListUiTabHelper::UpdatePriceTrackingStateFromSubscriptions() {
  if (!cluster_id_for_page_.has_value())
    return;

  const bookmarks::BookmarkNode* bookmark_node =
      bookmark_model_->GetMostRecentlyAddedUserNodeForURL(
          web_contents()->GetLastCommittedURL());
  commerce::IsBookmarkPriceTracked(
      shopping_service_, bookmark_model_, bookmark_node,
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

  TriggerUpdateForIconView();
}

const gfx::Image& ShoppingListUiTabHelper::GetProductImage() {
  return last_fetched_image_;
}

const GURL& ShoppingListUiTabHelper::GetProductImageURL() {
  return last_fetched_image_url_;
}

bool ShoppingListUiTabHelper::IsPriceTracking() {
  return pending_tracking_state_.value_or(is_cluster_id_tracked_by_user_);
}

void ShoppingListUiTabHelper::UpdatePriceTrackingIconView() {
  DCHECK(web_contents());

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());

  if (!browser || !browser->window()) {
    return;
  }

  browser->window()->UpdatePageActionIcon(PageActionIconType::kPriceTracking);
}

const absl::optional<bool>&
ShoppingListUiTabHelper::GetPendingTrackingStateForTesting() {
  return pending_tracking_state_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ShoppingListUiTabHelper);

}  // namespace commerce
