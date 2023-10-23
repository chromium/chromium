// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/price_tracking/shopping_list_ui_tab_helper.h"

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/commerce/price_insights_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/commerce/shopping_insights_side_panel_ui.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
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

constexpr base::TimeDelta kDelayIconView = base::Seconds(1);

// price tracking chip (assuming price insights isn't expanded).
constexpr int64_t kAlwaysExpandChipPriceMicros = 100000000L;

bool ShouldDelayChipUpdate() {
  if (base::FeatureList::IsEnabled(commerce::kPriceInsights)) {
    return commerce::kPriceInsightsDelayChip.Get();
  }

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
}

void ShoppingListUiTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      ShouldIgnoreSameUrlNavigation() ||
      IsSameDocumentWithSameCommittedUrl(navigation_handle)) {
    is_initial_navigation_committed_ = true;
    return;
  }

  previous_main_frame_url_ = navigation_handle->GetURL();
  last_fetched_image_ = gfx::Image();
  last_fetched_image_url_ = GURL();
  is_cluster_id_tracked_by_user_ = false;
  cluster_id_for_page_.reset();
  product_info_for_page_.reset();
  is_page_action_expansion_computed_for_page_ = false;
  got_discounts_response_for_page_ = false;
  got_insights_response_for_page_ = false;
  got_product_response_for_page_ = false;
  got_initial_subscription_status_for_page_ = false;
  page_has_discounts_ = false;
  page_action_to_expand_ = absl::nullopt;
  pending_tracking_state_.reset();
  is_first_load_for_nav_finished_ = false;
  price_insights_info_.reset();

  MakeShoppingInsightsSidePanelUnavailable();

  if (!shopping_service_) {
    return;
  }

  // Cancel any pending callbacks by invalidating any weak pointers.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (shopping_service_->IsPriceInsightsEligible()) {
    UpdatePriceInsightsIconView();
  }
  if (shopping_service_->IsShoppingListEligible()) {
    UpdatePriceTrackingIconView();
  }

  shopping_service_->GetProductInfoForUrl(
      web_contents()->GetLastCommittedURL(),
      base::BindOnce(&ShoppingListUiTabHelper::HandleProductInfoResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  if (shopping_service_->IsDiscountEligibleToShowOnNavigation() ||
      base::FeatureList::IsEnabled(
          ntp_features::kNtpHistoryClustersModuleDiscounts)) {
    shopping_service_->GetDiscountInfoForUrls(
        {web_contents()->GetLastCommittedURL()},
        base::BindOnce(&ShoppingListUiTabHelper::HandleDiscountsResponse,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

bool ShoppingListUiTabHelper::ShouldIgnoreSameUrlNavigation() {
  return previous_main_frame_url_ == web_contents()->GetLastCommittedURL() &&
         is_initial_navigation_committed_;
}

bool ShoppingListUiTabHelper::IsSameDocumentWithSameCommittedUrl(
    content::NavigationHandle* navigation_handle) {
  return previous_main_frame_url_ == web_contents()->GetLastCommittedURL() &&
         navigation_handle->IsSameDocument();
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
    if (shopping_service_->IsPriceInsightsEligible()) {
      UpdatePriceInsightsIconView();
    }
    UpdatePriceTrackingIconView();
  } else {
    DelayUpdateForIconView();
  }
}

void ShoppingListUiTabHelper::DelayUpdateForIconView() {
  if (!is_first_load_for_nav_finished_) {
    return;
  }

  if (shopping_service_->IsPriceInsightsEligible()) {
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                &ShoppingListUiTabHelper::UpdatePriceInsightsIconView,
                weak_ptr_factory_.GetWeakPtr()),
            kDelayIconView);
  }

  if (!last_fetched_image_.IsEmpty()) {
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                &ShoppingListUiTabHelper::UpdatePriceTrackingIconView,
                weak_ptr_factory_.GetWeakPtr()),
            kDelayIconView);
  }
}

void ShoppingListUiTabHelper::UpdatePriceInsightsIconView() {
  DCHECK(web_contents());

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());

  if (!browser || !browser->window()) {
    return;
  }

  browser->window()->UpdatePageActionIcon(PageActionIconType::kPriceInsights);
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

void ShoppingListUiTabHelper::SetImageFetcherForTesting(
    image_fetcher::ImageFetcher* image_fetcher) {
  CHECK_IS_TEST();
  image_fetcher_ = image_fetcher;
}

bool ShoppingListUiTabHelper::ShouldShowPriceTrackingIconView() {
  bool should_show = shopping_service_ &&
                     shopping_service_->IsShoppingListEligible() &&
                     !last_fetched_image_.IsEmpty() &&
                     got_initial_subscription_status_for_page_;

  return ShouldDelayChipUpdate()
             ? should_show && is_first_load_for_nav_finished_
             : should_show;
}

bool ShoppingListUiTabHelper::ShouldShowPriceInsightsIconView() {
  bool should_show = shopping_service_ &&
                     shopping_service_->IsPriceInsightsEligible() &&
                     price_insights_info_.has_value();

  return ShouldDelayChipUpdate()
             ? should_show && is_first_load_for_nav_finished_
             : should_show;
}

void ShoppingListUiTabHelper::HandleProductInfoResponse(
    const GURL& url,
    const absl::optional<const ProductInfo>& info) {
  if (url != web_contents()->GetLastCommittedURL() || !info.has_value()) {
    got_product_response_for_page_ = true;
    MaybeComputePageActionToExpand();
    return;
  }

  product_info_for_page_ = info;

  if (shopping_service_->IsShoppingListEligible() && CanTrackPrice(info) &&
      !info->image_url.is_empty()) {
    cluster_id_for_page_.emplace(info->product_cluster_id.value());
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

  if (shopping_service_->IsPriceInsightsEligible()) {
    if (!info->product_cluster_title.empty()) {
      shopping_service_->GetPriceInsightsInfoForUrl(
          url, base::BindOnce(
                   &ShoppingListUiTabHelper::HandlePriceInsightsInfoResponse,
                   weak_ptr_factory_.GetWeakPtr()));
    } else {
      // If we were blocked because of the title, consider it a response of
      // false.
      got_insights_response_for_page_ = true;
    }
  }
}

void ShoppingListUiTabHelper::HandlePriceInsightsInfoResponse(
    const GURL& url,
    const absl::optional<PriceInsightsInfo>& info) {
  got_insights_response_for_page_ = true;

  if (url != web_contents()->GetLastCommittedURL() || !info.has_value()) {
    MaybeComputePageActionToExpand();
    return;
  }

  price_insights_info_.emplace(info.value());
  MaybeComputePageActionToExpand();
  MakeShoppingInsightsSidePanelAvailable();
  TriggerUpdateForIconView();
}

void ShoppingListUiTabHelper::HandleDiscountsResponse(const DiscountsMap& map) {
  bool response_has_discounts = false;
  if (!map.empty()) {
    for (auto it = map.begin(); it == map.end(); ++it) {
      if (!it->second.empty()) {
        response_has_discounts = true;
        break;
      }
    }
  }

  page_has_discounts_ =
      response_has_discounts
          ? shopping_service_->IsDiscountEligibleToShowOnNavigation() ||
                (base::FeatureList::IsEnabled(
                     ntp_features::kNtpHistoryClustersModuleDiscounts) &&
                 commerce::UrlContainsDiscountUtmTag(
                     web_contents()->GetLastCommittedURL()))
          : false;

  got_discounts_response_for_page_ = true;
  MaybeComputePageActionToExpand();
}

void ShoppingListUiTabHelper::MaybeComputePageActionToExpand() {
  if (!shopping_service_) {
    return;
  }

  // Make sure we have responses from all the relevant features first.
  if ((shopping_service_->IsDiscountEligibleToShowOnNavigation() ||
       base::FeatureList::IsEnabled(
           ntp_features::kNtpHistoryClustersModuleDiscounts)) &&
      !got_discounts_response_for_page_) {
    return;
  }
  if (shopping_service_->IsPriceInsightsEligible() &&
      !got_insights_response_for_page_) {
    return;
  }
  if (shopping_service_->IsShoppingListEligible() &&
      (!got_product_response_for_page_ ||
       !got_initial_subscription_status_for_page_)) {
    return;
  }

  if (is_page_action_expansion_computed_for_page_) {
    return;
  }

  ComputePageActionToExpand();

  is_page_action_expansion_computed_for_page_ = true;

  UpdatePriceTrackingIconView();
  UpdatePriceInsightsIconView();
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
         base::OnceCallback<void(bool)> callback, bool is_tracked,
         bool success) {
        if (helper) {
          if (success) {
            helper->is_cluster_id_tracked_by_user_ = is_tracked;
          }
          helper->pending_tracking_state_.reset();
        }

        std::move(callback).Run(success);
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), enable);

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
          shopping_service_.get(), bookmark_model_,
          info->product_cluster_id.value(), enable,
          std::move(wrapped_callback));
    }
  }
}

void ShoppingListUiTabHelper::OnPriceInsightsIconClicked() {
  auto* side_panel_ui = GetSidePanelUI();
  auto* registry = SidePanelRegistry::Get(web_contents());
  DCHECK(side_panel_ui && registry->GetEntryForKey(SidePanelEntry::Key(
                              SidePanelEntry::Id::kShoppingInsights)));

  if (side_panel_ui->IsSidePanelShowing() &&
      side_panel_ui->GetCurrentEntryId() ==
          SidePanelEntry::Id::kShoppingInsights) {
    side_panel_ui->Close();
  } else {
    side_panel_ui->Show(SidePanelEntryId::kShoppingInsights);
    if (price_insights_info_.has_value()) {
      base::UmaHistogramBoolean(
          "Commerce.PriceInsights.SidePanelOpenWithMultipleCatalogs",
          price_insights_info_->has_multiple_catalogs);
    }
  }
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
            helper->got_initial_subscription_status_for_page_ = true;
            helper->UpdatePriceTrackingIconView();
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void ShoppingListUiTabHelper::HandleImageFetcherResponse(
    const GURL image_url,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& request_metadata) {
  if (image.IsEmpty()) {
    return;
  }

  last_fetched_image_url_ = image_url;
  last_fetched_image_ = image;

  got_product_response_for_page_ = true;
  MaybeComputePageActionToExpand();

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

void ShoppingListUiTabHelper::MakeShoppingInsightsSidePanelAvailable() {
  auto* registry = SidePanelRegistry::Get(web_contents());
  if (!registry) {
    return;
  }

  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kShoppingInsights,
      l10n_util::GetStringUTF16(IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TITLE),
      ui::ImageModel::FromVectorIcon(vector_icons::kShoppingBagIcon,
                                     ui::kColorIcon, /*icon_size=*/16),
      base::BindRepeating(
          &ShoppingListUiTabHelper::CreateShoppingInsightsWebView,
          base::Unretained(this)));
  registry->Register(std::move(entry));
}

void ShoppingListUiTabHelper::MakeShoppingInsightsSidePanelUnavailable() {
  auto* side_panel_ui = GetSidePanelUI();
  if (side_panel_ui && side_panel_ui->IsSidePanelShowing() &&
      side_panel_ui->GetCurrentEntryId() ==
          SidePanelEntry::Id::kShoppingInsights) {
    side_panel_ui->Close();
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceInsights.NavigationClosedSidePanel"));
  }

  auto* registry = SidePanelRegistry::Get(web_contents());
  if (!registry) {
    return;
  }
  registry->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kShoppingInsights));
}

std::unique_ptr<views::View>
ShoppingListUiTabHelper::CreateShoppingInsightsWebView() {
  auto shopping_insights_web_view =
      std::make_unique<SidePanelWebUIViewT<ShoppingInsightsSidePanelUI>>(
          base::RepeatingClosure(), base::RepeatingClosure(),
          std::make_unique<BubbleContentsWrapperT<ShoppingInsightsSidePanelUI>>(
              GURL(kChromeUIShoppingInsightsSidePanelUrl),
              Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
              IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TITLE,
              /*webui_resizes_host=*/false,
              /*esc_closes_ui=*/false));
  // Call ShowUI() to make the UI ready, this doesn't really open/switch the
  // side panel.
  shopping_insights_web_view->ShowUI();

  return shopping_insights_web_view;
}

SidePanelUI* ShoppingListUiTabHelper::GetSidePanelUI() const {
  auto* browser = chrome::FindBrowserWithWebContents(web_contents());
  return browser ? SidePanelUI::GetSidePanelUIForBrowser(browser) : nullptr;
}

const absl::optional<bool>&
ShoppingListUiTabHelper::GetPendingTrackingStateForTesting() {
  return pending_tracking_state_;
}

const absl::optional<PriceInsightsInfo>&
ShoppingListUiTabHelper::GetPriceInsightsInfo() {
  return price_insights_info_;
}

bool ShoppingListUiTabHelper::IsShowingDiscountsIcon() {
  auto* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser) {
    return false;
  }
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return false;
  }

  auto* toolbar_button_provider = browser_view->toolbar_button_provider();
  if (!toolbar_button_provider) {
    return false;
  }

  auto* icon = toolbar_button_provider->GetPageActionIconView(
      PageActionIconType::kPaymentsOfferNotification);

  return page_has_discounts_ || (icon ? icon->GetVisible() : false);
  ;
}

void ShoppingListUiTabHelper::ComputePageActionToExpand() {
  page_action_to_expand_ = absl::nullopt;

  if (!web_contents() || !web_contents()->GetBrowserContext()) {
    page_action_to_expand_ = absl::nullopt;
    return;
  }

  auto* tracker = feature_engagement::TrackerFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());

  // TODO(b:301440117): Splitting the triggering logic for each icon into
  //                    delegates would make this much easier to test.

  // We don't have full control over the discounts icon, so if we detect
  // that it is showing at all, block the others from expanding.
  if (IsShowingDiscountsIcon()) {
    return;
  }

  // Prioritize the price insights icon.
  if (ShouldShowPriceInsightsIconView()) {
    PriceInsightsIconView::PriceInsightsIconLabelType label_type =
        GetPriceInsightsIconLabelTypeForPage();
    bool icon_has_label =
        label_type != PriceInsightsIconView::PriceInsightsIconLabelType::kNone;

    if (icon_has_label && tracker &&
        tracker->ShouldTriggerHelpUI(
            feature_engagement::kIPHPriceInsightsPageActionIconLabelFeature)) {
      // Note that `Dismiss()` in these cases does not dismiss the UI. It's
      // telling the FE backend that the promo is done so that other promos can
      // run. Showing the label should not block other promos from displaying.
      tracker->Dismissed(
          feature_engagement::kIPHPriceInsightsPageActionIconLabelFeature);
      page_action_to_expand_ = PageActionIconType::kPriceInsights;
      base::UmaHistogramEnumeration(
          "Commerce.PriceInsights.OmniboxIconShownLabel", label_type);
      return;
    } else {
      base::UmaHistogramEnumeration(
          "Commerce.PriceInsights.OmniboxIconShownLabel",
          PriceInsightsIconView::PriceInsightsIconLabelType::kNone);
    }
  }

  if (ShouldShowPriceTrackingIconView()) {
    bool already_subscribed = false;
    if (product_info_for_page_.has_value()) {
      CommerceSubscription sub(
          SubscriptionType::kPriceTrack, IdentifierType::kProductClusterId,
          base::NumberToString(
              product_info_for_page_->product_cluster_id.value()),
          ManagementType::kUserManaged);
      already_subscribed = shopping_service_->IsSubscribedFromCache(sub);
    }

    // Don't expand the chip if the user is already subscribed to the product.
    if (!already_subscribed) {
      if (tracker && tracker->ShouldTriggerHelpUI(
                         feature_engagement::
                             kIPHPriceTrackingPageActionIconLabelFeature)) {
        tracker->Dismissed(
            feature_engagement::kIPHPriceTrackingPageActionIconLabelFeature);
        page_action_to_expand_ = PageActionIconType::kPriceTracking;
        return;
      }

      // If none of the above cases expanded a chip, expand the price tracking
      // chip if the product price is > $100.
      if (product_info_for_page_->amount_micros >
              kAlwaysExpandChipPriceMicros &&
          product_info_for_page_->product_cluster_id.has_value()) {
        page_action_to_expand_ = PageActionIconType::kPriceTracking;
        return;
      }
    }
  }
}

PriceInsightsIconView::PriceInsightsIconLabelType
ShoppingListUiTabHelper::GetPriceInsightsIconLabelTypeForPage() {
  auto& price_insights_info = GetPriceInsightsInfo();

  if (!price_insights_info.has_value() ||
      !price_insights_info->typical_low_price_micros.has_value() ||
      !price_insights_info->typical_high_price_micros.has_value() ||
      price_insights_info->catalog_history_prices.empty()) {
    return PriceInsightsIconView::PriceInsightsIconLabelType::kNone;
  } else if (price_insights_info->price_bucket ==
             commerce::PriceBucket::kLowPrice) {
    return PriceInsightsIconView::PriceInsightsIconLabelType::kPriceIsLow;
  } else if (price_insights_info->price_bucket ==
                 commerce::PriceBucket::kHighPrice &&
             commerce::kPriceInsightsChipLabelExpandOnHighPrice.Get()) {
    return PriceInsightsIconView::PriceInsightsIconLabelType::kPriceIsHigh;
  } else {
    return PriceInsightsIconView::PriceInsightsIconLabelType::kNone;
  }
}

bool ShoppingListUiTabHelper::ShouldExpandPageActionIcon(
    PageActionIconType type) {
  // Only allow the requesting icon to expand once. This prevents the icon from
  // expanding multiple times per page load.
  if (page_action_to_expand_.has_value() &&
      type == page_action_to_expand_.value()) {
    page_action_to_expand_ = absl::nullopt;
    return true;
  }
  return false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ShoppingListUiTabHelper);

}  // namespace commerce
