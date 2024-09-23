// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/price_tracking_page_action_controller.h"

#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/image_fetcher/core/image_fetcher.h"

namespace commerce {

namespace {
constexpr net::NetworkTrafficAnnotationTag kShoppingListTrafficAnnotation =
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
          user_data {
            type: NONE
          }
          data: "No user data."
          internal {
            contacts {
                email: "chrome-shopping@google.com"
            }
          }
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2024-01-11"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This fetch is enabled for any user with the 'Shopping List' "
            "feature enabled."
          chrome_policy {
            ShoppingListEnabled {
              policy_options {mode: MANDATORY}
              ShoppingListEnabled: false
            }
          }
        })");

constexpr char kImageFetcherUmaClient[] = "ShoppingList";

// The minimum price that the price tracking UI always wants to expand at.
constexpr int64_t kAlwaysExpandChipPriceMicros = 100000000L;

}  // namespace

PriceTrackingPageActionController::PriceTrackingPageActionController(
    base::RepeatingCallback<void()> notify_callback,
    ShoppingService* shopping_service,
    image_fetcher::ImageFetcher* image_fetcher,
    feature_engagement::Tracker* tracker)
    : CommercePageActionController(std::move(notify_callback)),
      shopping_service_(shopping_service),
      image_fetcher_(image_fetcher),
      tracker_(tracker) {
  if (shopping_service_) {
    scoped_observation_.Observe(shopping_service_);
    shopping_service_->WaitForReady(base::BindOnce(
        [](base::WeakPtr<PriceTrackingPageActionController> controller,
           ShoppingService* service) {
          if (!controller || !service) {
            return;
          }
          if (service->IsShoppingListEligible()) {
            // Fetching the image may have been blocked by the eligibility
            // check, retry.
            controller->MaybeDoProductImageFetch(
                controller->product_info_for_page_);
            controller->NotifyHost();
          }
        },
        weak_ptr_factory_.GetWeakPtr()));
  } else {
    CHECK_IS_TEST();
  }
}

PriceTrackingPageActionController::~PriceTrackingPageActionController() {
  // Recording this in the destructor corresponds to the tab being closed or the
  // browser shutting down.
  MaybeRecordPriceTrackingIconMetrics(/*from_icon_use=*/false);
}

std::optional<bool>
PriceTrackingPageActionController::ShouldShowForNavigation() {
  // If the user isn't eligible for the feature, don't block.
  if (!shopping_service_ || !shopping_service_->IsShoppingListEligible()) {
    return false;
  }

  // If we got a response from the shopping service but the response was empty,
  // we don't need to wait for the image or subscription status.
  if (got_product_response_for_page_ && !product_info_for_page_.has_value()) {
    return false;
  }

  // If the page is determined to be a product page, we're "undecided" until we
  // can show the current subscription state and have an image for the UI.
  if (!got_initial_subscription_status_for_page_ ||
      !got_image_response_for_page_) {
    return std::nullopt;
  }

  return !last_fetched_image_.IsEmpty();
}

bool PriceTrackingPageActionController::WantsExpandedUi() {
  if (!product_info_for_page_.has_value()) {
    return false;
  }
  CommerceSubscription sub(
      SubscriptionType::kPriceTrack, IdentifierType::kProductClusterId,
      base::NumberToString(product_info_for_page_->product_cluster_id.value()),
      ManagementType::kUserManaged);
  bool already_subscribed = shopping_service_->IsSubscribedFromCache(sub);

  // Don't expand the chip if the user is already subscribed to the product.
  if (!already_subscribed) {
    if (tracker_ &&
        tracker_->ShouldTriggerHelpUI(
            feature_engagement::kIPHPriceTrackingPageActionIconLabelFeature)) {
      tracker_->Dismissed(
          feature_engagement::kIPHPriceTrackingPageActionIconLabelFeature);
      expanded_ui_for_page_ = true;
      return true;
    }

    // If none of the above cases expanded a chip, expand the price tracking
    // chip if the product price is > $100.
    if (product_info_for_page_->amount_micros > kAlwaysExpandChipPriceMicros &&
        product_info_for_page_->product_cluster_id.has_value()) {
      expanded_ui_for_page_ = true;
      return true;
    }
  }

  return false;
}

void PriceTrackingPageActionController::ResetForNewNavigation(const GURL& url) {
  if (!shopping_service_->IsShoppingListEligible()) {
    return;
  }

  // The page action icon may not have been used for the last page load. If
  // that's the case, make sure we record it.
  MaybeRecordPriceTrackingIconMetrics(/*from_icon_use=*/false);

  // Cancel any pending callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  is_cluster_id_tracked_by_user_ = false;
  last_fetched_image_ = gfx::Image();
  last_fetched_image_url_ = GURL();
  current_url_ = url;
  got_product_response_for_page_ = false;
  got_image_response_for_page_ = false;
  got_initial_subscription_status_for_page_ = false;
  expanded_ui_for_page_ = false;
  icon_use_recorded_for_page_ = false;

  // Initiate an update for the icon on navigation since we may not have product
  // info.
  NotifyHost();

  shopping_service_->GetProductInfoForUrl(
      url, base::BindOnce(
               &PriceTrackingPageActionController::HandleProductInfoResponse,
               weak_ptr_factory_.GetWeakPtr()));
}

void PriceTrackingPageActionController::OnIconClicked() {
  MaybeRecordPriceTrackingIconMetrics(/*from_icon_use=*/true);
}

void PriceTrackingPageActionController::MaybeRecordPriceTrackingIconMetrics(
    bool from_icon_use) {
  // Ignore cases where these is no cluster ID or the metric was already
  // recorded for the page.
  if (!cluster_id_for_page_.has_value() || icon_use_recorded_for_page_) {
    return;
  }

  icon_use_recorded_for_page_ = true;

  // Ignore any instances where the product is already tracked. This will not
  // stop cases where the icon is being used to newly track a product since
  // this logic will run prior to subscriptions updating.
  if (is_cluster_id_tracked_by_user_) {
    return;
  }

  std::string histogram_name = "Commerce.PriceTracking.IconInteractionState";

  // Clicking the icon for a product that is already tracked does not
  // immediately untrack the product. If we made it this far, we can assume the
  // interaction was to track a product, otherwise we would have been blocked
  // above.
  if (from_icon_use) {
    if (expanded_ui_for_page_) {
      base::UmaHistogramEnumeration(
          histogram_name, PageActionIconInteractionState::kClickedExpanded);
    } else {
      base::UmaHistogramEnumeration(histogram_name,
                                    PageActionIconInteractionState::kClicked);
    }
  } else {
    if (expanded_ui_for_page_) {
      base::UmaHistogramEnumeration(
          histogram_name, PageActionIconInteractionState::kNotClickedExpanded);
    } else {
      base::UmaHistogramEnumeration(
          histogram_name, PageActionIconInteractionState::kNotClicked);
    }
  }
}

void PriceTrackingPageActionController::HandleProductInfoResponse(
    const GURL& url,
    const std::optional<const ProductInfo>& info) {
  if (url != current_url_ || !info.has_value()) {
    got_product_response_for_page_ = true;
    NotifyHost();
    return;
  }

  product_info_for_page_ = info;
  got_product_response_for_page_ = true;

  if (shopping_service_->IsShoppingListEligible() && CanTrackPrice(info) &&
      !info->image_url.is_empty()) {
    cluster_id_for_page_.emplace(info->product_cluster_id.value());
    UpdatePriceTrackingStateFromSubscriptions();

    MaybeDoProductImageFetch(info);
  }
}

void PriceTrackingPageActionController::MaybeDoProductImageFetch(
    const std::optional<ProductInfo>& info) {
  if (!shopping_service_->IsShoppingListEligible() || !CanTrackPrice(info) ||
      info->image_url.is_empty() || !this->last_fetched_image_.IsEmpty()) {
    return;
  }

  // TODO(crbug.com/40863328): Delay this fetch by possibly waiting until page
  // load has
  //                finished.
  image_fetcher_->FetchImage(
      info.value().image_url,
      base::BindOnce(
          &PriceTrackingPageActionController::HandleImageFetcherResponse,
          weak_ptr_factory_.GetWeakPtr(), info.value().image_url),
      image_fetcher::ImageFetcherParams(kShoppingListTrafficAnnotation,
                                        kImageFetcherUmaClient));
}

void PriceTrackingPageActionController::
    UpdatePriceTrackingStateFromSubscriptions() {
  if (!cluster_id_for_page_.has_value()) {
    return;
  }

  shopping_service_->IsSubscribed(
      BuildUserSubscriptionForClusterId(cluster_id_for_page_.value()),
      base::BindOnce(
          [](base::WeakPtr<PriceTrackingPageActionController> controller,
             bool is_tracked) {
            if (!controller) {
              return;
            }

            controller->is_cluster_id_tracked_by_user_ = is_tracked;
            controller->got_initial_subscription_status_for_page_ = true;
            controller->NotifyHost();
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void PriceTrackingPageActionController::OnSubscribe(
    const CommerceSubscription& subscription,
    bool succeeded) {
  HandleSubscriptionChange(subscription);
}

void PriceTrackingPageActionController::OnUnsubscribe(
    const CommerceSubscription& subscription,
    bool succeeded) {
  HandleSubscriptionChange(subscription);
}

void PriceTrackingPageActionController::HandleSubscriptionChange(
    const CommerceSubscription& sub) {
  if (sub.id_type == IdentifierType::kProductClusterId &&
      sub.id == base::NumberToString(
                    cluster_id_for_page_.value_or(kInvalidSubscriptionId))) {
    UpdatePriceTrackingStateFromSubscriptions();
    NotifyHost();
  }
}

void PriceTrackingPageActionController::HandleImageFetcherResponse(
    const GURL image_url,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& request_metadata) {
  got_image_response_for_page_ = true;

  if (!image.IsEmpty()) {
    last_fetched_image_url_ = image_url;
    last_fetched_image_ = image;
  }

  NotifyHost();
}

const gfx::Image& PriceTrackingPageActionController::GetLastFetchedImage() {
  return last_fetched_image_;
}

const GURL& PriceTrackingPageActionController::GetLastFetchedImageUrl() {
  return last_fetched_image_url_;
}

bool PriceTrackingPageActionController::IsPriceTrackingCurrentProduct() {
  return is_cluster_id_tracked_by_user_;
}

void PriceTrackingPageActionController::SetImageFetcherForTesting(
    image_fetcher::ImageFetcher* image_fetcher) {
  image_fetcher_ = image_fetcher;
}

}  // namespace commerce
