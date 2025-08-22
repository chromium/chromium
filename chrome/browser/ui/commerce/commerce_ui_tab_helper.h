// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_COMMERCE_UI_TAB_HELPER_H_
#define CHROME_BROWSER_UI_COMMERCE_COMMERCE_UI_TAB_HELPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/commerce/price_tracking_page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/image/image.h"

// TODO(https://crbug.com/362675963): Once //c/b/ui/views/commerce/ gets
// modularized, the declaration of enum class PriceInsightsIconLabelType can
// move back into class PriceInsightsIconView, and we can include
// //c/b/ui/views/commerce/price_insights_icon_view.h directly, without
// circular dependencies.
enum class PriceInsightsIconLabelType;

class GURL;
class SidePanelEntryScope;
class SidePanelRegistry;
class SidePanelUI;
class DiscountsBubbleCoordinator;
class DiscountsIconViewBrowserTest;

namespace bookmarks {
class BookmarkModel;
}

namespace content {
class NavigationHandle;
}  // namespace content

namespace tabs {
class TabInterface;
class TabModel;
}

namespace image_fetcher {
class ImageFetcher;
}

namespace views {
class View;
}  // namespace views

namespace commerce {

class DiscountsPageActionController;
class ProductSpecificationsPageActionController;
class ShoppingService;

// This tab helper is used to update and maintain the state of UI for commerce
// features.
class CommerceUiTabHelper : public tabs::ContentsObservingTabFeature {
 public:
  CommerceUiTabHelper(tabs::TabInterface& tab_interface,
                      ShoppingService* shopping_service,
                      bookmarks::BookmarkModel* model,
                      image_fetcher::ImageFetcher* image_fetcher,
                      SidePanelRegistry* side_panel_registry);
  ~CommerceUiTabHelper() override;
  CommerceUiTabHelper(const CommerceUiTabHelper& other) = delete;
  CommerceUiTabHelper& operator=(const CommerceUiTabHelper& other) = delete;

  DECLARE_USER_DATA(CommerceUiTabHelper);
  static CommerceUiTabHelper* From(tabs::TabModel* tab);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Get the image for the last fetched product URL. A reference to this object
  // should not be kept directly, if one is needed, a copy should be made.
  virtual const gfx::Image& GetProductImage();
  // Return whether the DiscountsPageActionIconView is visible.
  virtual bool ShouldShowDiscountsIconView();
  // Return whether the PriceTrackingIconView is visible.
  virtual bool ShouldShowPriceTrackingIconView();
  // Return whether the PriceInsightsIconView is visible.
  virtual bool ShouldShowPriceInsightsIconView();
  // Return whether the ProductSpecificationsIconView is visible.
  virtual bool ShouldShowProductSpecificationsIconView();

  // Return the page action label. If no label should be shown, return
  // PriceInsightsIconLabelType::kNone.
  virtual PriceInsightsIconLabelType GetPriceInsightsIconLabelTypeForPage();

  // The URL for the last fetched product image. A reference to this object
  // should not be kept directly, if one is needed, a copy should be made.
  const GURL& GetProductImageURL();

  // Returns whether the current page has a product that is being price tracked.
  virtual bool IsPriceTracking();

  // Returns whether the product in the current page is in the recommended
  // product specifications set.
  virtual bool IsInRecommendedSet();

  // A notification that the open button in the added to compare set toast is
  // clicked. This method will open the compare page in a new tab if the compare
  // page is not already open in the current window, otherwise it will switch to
  // that compare page tab.
  virtual void OnOpenComparePageClicked();

  // Returns the name of the comparison set.
  virtual std::u16string GetComparisonSetName();

  // Returns the label to show on the product specifications icon.
  virtual std::u16string GetProductSpecificationsLabel(bool is_added);

  // Returns discounts for the last committed URL. A reference to this object
  // should not be kept directly, if one is needed, a copy should be made.
  virtual const std::vector<DiscountInfo>& GetDiscounts();

  // content::WebContentsObserver implementation
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  // Update this tab helper to use the specified image fetcher in tests.
  void SetImageFetcherForTesting(image_fetcher::ImageFetcher* image_fetcher);

  // Set the price tracking state for the product on the current page.
  virtual void SetPriceTrackingState(bool enable,
                                     bool is_new_bookmark,
                                     base::OnceCallback<void(bool)> callback);
  void OnPriceInsightsIconClicked();
  virtual void OnProductSpecificationsIconClicked();

  // Return the PriceInsightsInfo for the last fetched product URL. A reference
  // to this object should not be kept directly, if one is needed, a copy should
  // be made.
  virtual const std::optional<PriceInsightsInfo>& GetPriceInsightsInfo();

  // Gets whether the page action with the provided |type| should expand. This
  // method will change the internal state of this class if the ID provided
  // matches the icon that should expand -- the "true" response is only valid
  // once per page load to avoid having the icon expand multiple times.
  virtual bool ShouldExpandPageActionIcon(PageActionIconType type);
  // Return whether the page action with provided |type| has been expanded.
  bool IsPageActionIconExpanded(PageActionIconType type);

  // A notification that the price tracking icon was clicked.
  void OnPriceTrackingIconClicked();

  // TODO(b/355566609): Expose the DiscountsPageActionController getter instead
  // of the discount related methods below.
  //  A notification that the coupon code in the Discounts bubble is copied.
  void OnDiscountsCouponCodeCopied();
  // Return whether the coupon code is copied. This will reset the copied
  // status.
  virtual bool IsDiscountsCouponCodeCopied();
  // Return whether the Discounts bubble should show automatically for the given
  // |discount_id|.
  virtual bool ShouldAutoShowDiscountsBubble(uint64_t discount_id,
                                             bool is_merchant_wide);
  void DiscountsBubbleShown(uint64_t discount_id);

  // Trigger the discount bubble show for the provided `discount` data.
  void ShowDiscountBubble(const DiscountInfo& discount,
                          base::OnceClosure one_bubble_closing_callback);

  const DiscountsBubbleCoordinator& GetDiscountsBubbleCoordinator() const;

  PriceTrackingPageActionController* GetPriceTrackingControllerForTesting();

  void SetPriceTrackingControllerForTesting(
      std::unique_ptr<PriceTrackingPageActionController> controller);

 protected:
  const std::optional<bool>& GetPendingTrackingStateForTesting();

  virtual std::unique_ptr<views::View> CreateShoppingInsightsWebView(
      SidePanelEntryScope& scope);

  virtual GURL GetComparisonTableURL();

 private:
  friend class CommerceUiTabHelperTest;
  friend class ::DiscountsIconViewBrowserTest;

  void UpdateUiForShoppingServiceReady(ShoppingService* service);

  void HandleProductInfoResponse(const GURL& url,
                                 const std::optional<const ProductInfo>& info);

  void HandlePriceInsightsInfoResponse(
      const GURL& url,
      const std::optional<PriceInsightsInfo>& info);

  void UpdateDiscountsIconView();

  // Returns the discounts page action view. It's used by the discount bubble
  // coordinator.
  views::View* GetDiscountsIconView();

  void UpdatePriceTrackingIconView();

  void UpdatePriceInsightsIconView();

  void UpdateProductSpecificationsIconView();

  void TriggerUpdateForIconView();

  bool ShouldIgnoreSameUrlNavigation();

  bool IsSameDocumentWithSameCommittedUrl(
      content::NavigationHandle* navigation_handle);

  // Make the ShoppingInsights entry available in the side panel.
  void MakeShoppingInsightsSidePanelAvailable();

  // Make the ShoppingInsights entry unavailable in the side panel. If the
  // ShoppingInsights side panel is currently showing, close the side panel
  // first.
  void MakeShoppingInsightsSidePanelUnavailable();

  SidePanelUI* GetSidePanelUI();

  void MaybeComputePageActionToExpand();

  void ComputePageActionToExpand();

  void RecordIconMetrics(PageActionIconType page_action, bool from_icon_use);

  void RecordPriceInsightsIconMetrics(bool from_icon_use);

  void MaybeRecordShoppingInformationUKM(
      std::optional<PageActionIconType> page_action_type);

  void OnPageActionControllerNotification(
      base::RepeatingClosure page_action_icon_update_callback);

  base::RepeatingClosure GetPageActionControllerNotificationCallback(
      base::RepeatingClosure page_action_icon_update_callback);

  // This helper is for the legacy page actions. It will be removed after the
  // migration to the new framework.
  void UpdatePageActionIconView(PageActionIconType type);

  // The shopping service is tied to the lifetime of the browser context
  // which will always outlive this tab helper.
  raw_ptr<ShoppingService, DanglingUntriaged> shopping_service_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<image_fetcher::ImageFetcher> image_fetcher_;
  raw_ptr<SidePanelRegistry> side_panel_registry_;

  std::unique_ptr<PriceTrackingPageActionController> price_tracking_controller_;
  std::unique_ptr<ProductSpecificationsPageActionController>
      product_specifications_controller_;
  std::unique_ptr<DiscountsPageActionController>
      discounts_page_action_controller_;

  // The product info available for the current page if available.
  std::optional<ProductInfo> product_info_for_page_;

  // Whether the chip that should expand for the current page has been computed.
  bool is_page_action_expansion_computed_for_page_{false};

  // Whether we have received responses for the various commerce features for
  // the current page load.
  bool got_discounts_response_for_page_{false};
  bool got_insights_response_for_page_{false};
  bool page_has_discounts_{false};

  // Page action icon uses that have already been recorded for the current page.
  // For Price Tracking, this will only record "track" events.
  std::set<PageActionIconType> icon_use_recorded_for_page_;

  // A flag indicating whether the initial navigation has committed for the web
  // contents. This is used to ensure product info is fetched when a tab is
  // being restored.
  bool is_initial_navigation_committed_{false};

  // This represents the desired state of the tracking icon prior to getting the
  // callback from the (un)subscribe event. If no value, there is no pending
  // state, otherwise true means "tracking" and false means "not tracking".
  std::optional<bool> pending_tracking_state_;

  // The url from the previous successful main frame navigation. This will be
  // empty if this is the first navigation for this tab or post-restart.
  GURL previous_main_frame_url_;

  // The PriceInsightsInfo associated with the last committed URL.
  std::optional<PriceInsightsInfo> price_insights_info_;

  // The page action that should expand for the current page. This optional will
  // be reset once the value is read by the UI.
  std::optional<PageActionIconType> page_action_to_expand_;

  // The page action that was expanded for the current page load, if any. This
  // indicates that |page_action_to_expand_| was read by the UI and lets us keep
  // track of which page action actually expanded.
  std::optional<PageActionIconType> page_action_expanded_;

  base::TimeTicks page_action_icon_compute_start_time_;

  // The price insights icon label type for the current page load.
  PriceInsightsIconLabelType price_insights_label_type_;

  // Coordinates the creation and the display of the discounts bubble view.
  std::unique_ptr<DiscountsBubbleCoordinator> discounts_bubble_coordinator_;

  ui::ScopedUnownedUserData<CommerceUiTabHelper> scoped_unowned_user_data_;
  base::WeakPtrFactory<CommerceUiTabHelper> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_COMMERCE_COMMERCE_UI_TAB_HELPER_H_
