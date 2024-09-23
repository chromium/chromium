// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_PRICE_TRACKING_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_COMMERCE_PRICE_TRACKING_PAGE_ACTION_CONTROLLER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/commerce/commerce_page_action_controller.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/subscriptions/subscriptions_observer.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "ui/gfx/image/image.h"

class GURL;

namespace feature_engagement {
class Tracker;
}

namespace image_fetcher {
class ImageFetcher;
}  // namespace image_fetcher

namespace commerce {

struct CommerceSubscription;
class ShoppingService;

class PriceTrackingPageActionController : public CommercePageActionController,
                                          public SubscriptionsObserver {
 public:
  PriceTrackingPageActionController(
      base::RepeatingCallback<void()> notify_callback,
      ShoppingService* shopping_service,
      image_fetcher::ImageFetcher* image_fetcher,
      feature_engagement::Tracker* tracker);
  PriceTrackingPageActionController(const PriceTrackingPageActionController&) =
      delete;
  PriceTrackingPageActionController& operator=(
      const PriceTrackingPageActionController&) = delete;
  ~PriceTrackingPageActionController() override;

  // CommercePageActionController impl:
  std::optional<bool> ShouldShowForNavigation() override;
  bool WantsExpandedUi() override;
  void ResetForNewNavigation(const GURL& url) override;

  void OnIconClicked();

  // SubscriptionsObserver impl:
  void OnSubscribe(const CommerceSubscription& subscription,
                   bool succeeded) override;
  void OnUnsubscribe(const CommerceSubscription& subscription,
                     bool succeeded) override;

  // Accessors for the UI.
  const gfx::Image& GetLastFetchedImage();
  const GURL& GetLastFetchedImageUrl();
  bool IsPriceTrackingCurrentProduct();
  void SetImageFetcherForTesting(image_fetcher::ImageFetcher* image_fetcher);

 private:
  void HandleProductInfoResponse(const GURL& url,
                                 const std::optional<const ProductInfo>& info);

  void MaybeDoProductImageFetch(const std::optional<ProductInfo>& info);

  // Update the flag tracking the price tracking state of the product from
  // subscriptions.
  void UpdatePriceTrackingStateFromSubscriptions();

  void HandleSubscriptionChange(const CommerceSubscription& sub);

  void HandleImageFetcherResponse(
      const GURL image_url,
      const gfx::Image& image,
      const image_fetcher::RequestMetadata& request_metadata);

  void MaybeRecordPriceTrackingIconMetrics(bool from_icon_use);

  // The URL for the most recent navigation.
  GURL current_url_;

  bool got_product_response_for_page_{false};
  bool got_image_response_for_page_{false};
  bool got_initial_subscription_status_for_page_{false};

  // Whether the product shown on the current page is tracked by the user.
  bool is_cluster_id_tracked_by_user_{false};

  // The cluster ID for the current page, if applicable.
  std::optional<uint64_t> cluster_id_for_page_;

  // The product info available for the current page if available.
  std::optional<ProductInfo> product_info_for_page_;

  // The shopping service is tied to the lifetime of the browser context
  // which will always outlive this tab helper.
  raw_ptr<ShoppingService> shopping_service_;

  raw_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  raw_ptr<feature_engagement::Tracker> tracker_;

  // The URL of the last product image that was fetched.
  GURL last_fetched_image_url_;

  // The last image that was fetched. See |last_image_fetched_url_| for the
  // URL that was used.
  gfx::Image last_fetched_image_;

  // Whether the page action icon was expanded for the current page load.
  bool expanded_ui_for_page_{false};

  // Whether the price tracking icon was recorded for the current page. This
  // will only record "track" events.
  bool icon_use_recorded_for_page_{false};

  base::ScopedObservation<ShoppingService, SubscriptionsObserver>
      scoped_observation_{this};

  base::WeakPtrFactory<PriceTrackingPageActionController> weak_ptr_factory_{
      this};
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_COMMERCE_PRICE_TRACKING_PAGE_ACTION_CONTROLLER_H_
