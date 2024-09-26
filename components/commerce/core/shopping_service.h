// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "components/commerce/core/account_checker.h"
#include "components/commerce/core/commerce_info_cache.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/compare/cluster_manager.h"
#include "components/commerce/core/product_specifications/product_specifications_cache.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#include "components/commerce/core/proto/discounts_db_content.pb.h"
#include "components/commerce/core/proto/parcel_tracking_db_content.pb.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/web_extractor.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/unified_consent/consent_throttle.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;
class PrefService;

template <typename T>
class SessionProtoStorage;

namespace base {
class Value;
}

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace optimization_guide {
class OptimizationGuideDecider;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace power_bookmarks {
class PowerBookmarkService;
}  // namespace power_bookmarks

namespace sessions {
class TabRestoreService;
}  // namespace sessions

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace commerce {

extern const char kImageAvailabilityHistogramName[];
extern const char kProductInfoLocalExtractionTime[];

// The amount of time to wait after the last "stopped loading" event to run the
// on-page extraction for product info.
extern const uint64_t kProductInfoLocalExtractionDelayMs;

// The availability of the product image for an offer. This needs to be kept in
// sync with the ProductImageAvailability enum in enums.xml.
enum class ProductImageAvailability {
  kServerOnly = 0,
  kLocalOnly = 1,
  kBothAvailable = 2,
  kNeitherAvailable = 3,
  kMaxValue = kNeitherAvailable,
};

// The type of fallback data can be used when generating product info.
enum class ProductInfoFallback {
  kTitle = 0,
  kLeadImage = 1,
  kFallbackImage = 2,
  kPrice = 3,
  kMaxValue = kPrice,
};

namespace metrics {
class ScheduledMetricsManager;
}  // namespace metrics

class BookmarkUpdateManager;
class DiscountsStorage;
class ParcelsManager;
class ProductSpecificationsServerProxy;
class ProductSpecificationsService;
class ShoppingPowerBookmarkDataProvider;
class ShoppingBookmarkModelObserver;
class SubscriptionsManager;
class SubscriptionsObserver;
class WebWrapper;
enum class SubscriptionType;
struct CommerceSubscription;

// Types of shopping pages from backend.
enum class ShoppingPageType {
  kUnknown = 0,
  kShoppingPage = 1,
  kMerchantDomainPage = 2,
  kMultiProductPage = 3,
  kSingleProductPage = 4,
  kProductReviewPage = 5,
  kProductComparisonPage = 6,
  kProductTopNPage = 7,
  kSoldOutPage = 8,
  kBuyingGuidePage = 9,
  kMaxValue = kBuyingGuidePage,
};

using DiscountsPair = std::pair<GURL, std::vector<DiscountInfo>>;
using DiscountsOptGuideCallback = base::OnceCallback<void(DiscountsPair)>;
using RepeatingProductInfoCallback =
    base::RepeatingCallback<void(const GURL&,
                                 const std::optional<const ProductInfo>&)>;

// A callback for getting updated ProductInfo for a bookmark. This provides the
// bookmark ID being updated, the URL, and the product info.
using BookmarkProductInfoUpdatedCallback = base::RepeatingCallback<
    void(const int64_t, const GURL&, std::optional<ProductInfo>)>;

using UrlProductIdentifierTuple =
    std::tuple<const GURL, const std::optional<const uint64_t>>;

using UrlProductIdentifierTupleCallback =
    base::OnceCallback<void(const UrlProductIdentifierTuple&)>;

// Under Desktop browser test or interactive ui test, use
// ShoppingServiceFactory::SetTestingFactory to create a
// MockShoppingService for testing. The test should use
// BrowserContextDependencyManager to register a callback to create the
// MockShoppingService when the BrowserContext is created.
//
// Example of an InteractiveBrowserTest setup for using a MockShoppingService:
//
// clang-format off
// #include "components/commerce/core/mock_shopping_service.h"
// #include "components/keyed_service/content/browser_context_dependency_manager.h"
//
// class MyTest : public InteractiveBrowserTest {
//   void SetUpInProcessBrowserTestFixture() override {
//     create_services_subscription_ =
//         BrowserContextDependencyManager::GetInstance()
//             ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
//                 &MyTest::OnWillCreateBrowserContextServices,
//                 weak_ptr_factory_.GetWeakPtr()));
//   }
//
//   void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
//     commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
//         context, base::BindRepeating([](content::BrowserContext* context) {
//           return commerce::MockShoppingService::Build();
//         }));
//   }
//
//  private:
//   base::CallbackListSubscription create_services_subscription_;
//   base::WeakPtrFactory<MyTest> weak_ptr_factory_{this};
// };
//
// To get the MockShoppingService:
// auto* mock_shopping_service = static_cast<commerce::MockShoppingService*>(
//     commerce::ShoppingServiceFactory::GetForBrowserContext(
//         browser()->profile()));
// clang-format on

class ShoppingService : public KeyedService,
                        public base::SupportsUserData,
                        public history::HistoryServiceObserver,
                        public ProductSpecificationsSet::Observer {
 public:
  ShoppingService(
      const std::string& country_on_startup,
      const std::string& locale_on_startup,
      bookmarks::BookmarkModel* bookmark_model,
      optimization_guide::OptimizationGuideDecider* opt_guide,
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      SessionProtoStorage<
          commerce_subscription_db::CommerceSubscriptionContentProto>*
          subscription_proto_db,
      power_bookmarks::PowerBookmarkService* power_bookmark_service,
      ProductSpecificationsService* product_specifications_service,
      SessionProtoStorage<discounts_db::DiscountsContentProto>*
          discounts_proto_db,
      SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>*
          parcel_tracking_proto_db,
      history::HistoryService* history_service,
      std::unique_ptr<commerce::WebExtractor> web_extractor,
      sessions::TabRestoreService* tab_restore_service);
  ~ShoppingService() override;

  ShoppingService(const ShoppingService&) = delete;
  ShoppingService& operator=(const ShoppingService&) = delete;

  // Gets an AccountChecker instance to aid in determining feature eligibility.
  virtual AccountChecker* GetAccountChecker();

  // This API retrieves the product information for the provided |url| and
  // passes the payload back to the caller via |callback|. At minimum, this
  // API will wait for data from the backend but may provide a "partial" result
  // that doesn't include information from the page on-device.
  virtual void GetProductInfoForUrl(const GURL& url,
                                    ProductInfoCallback callback);

  // This API returns whatever product information is currently available for
  // the specified |url|. This method is less reliable than GetProductInfoForUrl
  // above as it may return an empty or partial result prior to the page being
  // processed or information being available from the backend.
  virtual std::optional<ProductInfo> GetAvailableProductInfoForUrl(
      const GURL& url);

  // Get updated product info (including price) for the provided list of
  // bookmark IDs. The information for each bookmark will be provided via a
  // repeating callback that provides the bookmark's ID, URL, and product info.
  // Currently this API should only be used in the BookmarkUpdateManager.
  virtual void GetUpdatedProductInfoForBookmarks(
      const std::vector<int64_t>& bookmark_ids,
      BookmarkProductInfoUpdatedCallback info_updated_callback);

  // Gets the maximum number of bookmarks that the backend will retrieve per
  // call to |GetUpdatedProductInfoForBookmarks|. This limit is imposed by our
  // backend rather than the shopping service itself.
  virtual size_t GetMaxProductBookmarkUpdatesPerBatch();

  // This API fetches information about a merchant for the provided |url| and
  // passes the payload back to the caller via |callback|. Call will run after
  // the fetch is completed. The merchant info object will be null if there is
  // none available.
  virtual void GetMerchantInfoForUrl(const GURL& url,
                                     MerchantInfoCallback callback);

  // This API fetches price insights information of the product on the provided
  // |url| and passes the payload back to the caller via |callback|. Call will
  // run after the fetch is completed. The price insights info object will be
  // null if there is none available.
  virtual void GetPriceInsightsInfoForUrl(const GURL& url,
                                          PriceInsightsInfoCallback callback);

  // This API fetches valid discounts information on the provided |url| and
  // passes the payload back to the caller via |callback|. Call will run after
  // the fetch is completed.
  virtual void GetDiscountInfoForUrl(const GURL& url,
                                     DiscountInfoCallback callback);

  virtual void GetProductSpecificationsForUrls(
      const std::vector<GURL>& urls,
      ProductSpecificationsCallback callback);

  // This API fetches whether the provided |url| is a shopping-related page and
  // passes the result back to the caller via |callback|. Call will run after
  // the fetch is completed.
  virtual void IsShoppingPage(const GURL& url, IsShoppingPageCallback callback);

  // Create new subscriptions in batch if needed, and will notify |callback| if
  // the operation completes successfully.
  virtual void Subscribe(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      base::OnceCallback<void(bool)> callback);

  // Delete existing subscriptions in batch if needed, and will notify
  // |callback| if the operation completes successfully.
  virtual void Unsubscribe(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      base::OnceCallback<void(bool)> callback);

  // Gets all subscriptions for the specified type. The list of subscriptions
  // will be provided as input to the |callback| passed to this function.
  virtual void GetAllSubscriptions(
      SubscriptionType type,
      base::OnceCallback<void(std::vector<CommerceSubscription>)> callback);

  // Methods to register or remove SubscriptionsObserver, which will be notified
  // when a (un)subscribe request has finished.
  void AddSubscriptionsObserver(SubscriptionsObserver* observer);
  void RemoveSubscriptionsObserver(SubscriptionsObserver* observer);

  // Check if the specified subscription exists.
  virtual void IsSubscribed(CommerceSubscription subscription,
                            base::OnceCallback<void(bool)> callback);

  // Checks if a subscription exists from the in-memory cache. Use of the the
  // callback-based version |IsSubscribed| is preferred. Information provided
  // by this API is not guaranteed to be correct.
  virtual bool IsSubscribedFromCache(const CommerceSubscription& subscription);

  // Gets all bookmarks that are price tracked. Internally this calls the
  // function by the same name in price_tracking_utils.h.
  virtual void GetAllPriceTrackedBookmarks(
      base::OnceCallback<void(std::vector<const bookmarks::BookmarkNode*>)>
          callback);

  // Gets all bookmarks that have shopping information associated with them.
  // Internally this calls the function by the same name in
  // price_tracking_utils.h.
  virtual std::vector<const bookmarks::BookmarkNode*> GetAllShoppingBookmarks();

  // Fetch users' pref from server on whether to receive price tracking emails.
  void FetchPriceEmailPref();

  // Schedule an update for saved product bookmarks using
  // |bookmark_update_manager_|.
  virtual void ScheduleSavedProductUpdate();

  // Returns whether a feature that is restricted to a specific region and
  // locale is enabled. This method is a proxy for the utility method by the
  // same name in commerce_feature_list but provides the country and locale as
  // determined by this service at startup.
  bool IsRegionLockedFeatureEnabled(
      const base::Feature& feature,
      const base::Feature& region_specific_feature);

  // This is a feature check for the "shopping list". This will only return true
  // if the user has the feature flag enabled, is signed-in, has MSBB enabled,
  // has webapp activity enabled, is allowed by enterprise policy, and (if
  // applicable) in an eligible country and locale. The value returned by this
  // method can change at runtime, so it should not be used when deciding
  // whether to create critical, feature-related infrastructure.
  virtual bool IsShoppingListEligible();

  // Wait for the shopping service and all of its dependent components to be
  // ready before attempting to access different features. This can be used for
  // UI that is available shortly after startup. If the dependencies time out or
  // the browser is being shut down, a null pointer to the shopping service will
  // be passed to the callback.
  virtual void WaitForReady(
      base::OnceCallback<void(ShoppingService*)> callback);

  // This is a feature check for the "merchant viewer", which will return true
  // if the user has the feature flag enabled or (if applicable) is in an
  // enabled country and locale.
  virtual bool IsMerchantViewerEnabled();

  // This is a feature check for the "price tracking", which will return true
  // if the user has the feature flag enabled or (if applicable) is in an
  // enabled country and locale.
  virtual bool IsCommercePriceTrackingEnabled();

  // This is a feature check for the "price insights", which will return true
  // if the user has the feature flag enabled, has MSBB enabled, and (if
  // applicable) is in an eligible country and locale. The value returned by
  // this method can change at runtime, so it should not be used when deciding
  // whether to create critical, feature-related infrastructure.
  virtual bool IsPriceInsightsEligible();

  // This is a feature check for "show discounts on navigation", which will
  // return true if the user has the feature flag enabled, is signed-in and
  // synced, has MSBB enabled, and (if applicable) is in an eligible country and
  // locale. The value returned by this method can change at runtime, so it
  // should not be used when deciding whether to create critical,
  // feature-related infrastructure.
  virtual bool IsDiscountEligibleToShowOnNavigation();

  // Check if parcel tracking is eligible for use. This not only checks the
  // feature flag, but also checks user's sign in state, country code, etc. The
  // value returned here can change during runtime so it should not be used
  // when deciding to build infrastructure.
  virtual bool IsParcelTrackingEligible();

  // Returns a list of URLs corresponding to active WebWrappers the shopping
  // service is keeping track of. This does not map to open tabs across all
  // platforms. Excludes non-HTTP/HTTPS URLs.
  virtual const std::vector<UrlInfo> GetUrlInfosForActiveWebWrappers();

  // Returns a list of URL info provided by |GetUrlInfosForActiveWebWrappers|
  // but filtered by URLs that are associated with products.
  virtual void GetUrlInfosForWebWrappersWithProducts(
      base::OnceCallback<void(const std::vector<UrlInfo>)> callback);

  // Gets a list of URLs from web wrappers that were recently viewed by the
  // user (ordered by most recent first). This generally aligns with recently
  // viewed tabs.
  virtual const std::vector<UrlInfo> GetUrlInfosForRecentlyViewedWebWrappers();

  // Starts tracking a list of parcels from a given page.
  void StartTrackingParcels(
      const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
          parcel_identifiers,
      const std::string& source_page_domain,
      GetParcelStatusCallback callback);

  // Gets the status of all parcel status stored in the db.
  virtual void GetAllParcelStatuses(GetParcelStatusCallback callback);

  // Called to stop tracking a given parcel.
  // DEPRECATED: use StopTrackingParcels() below()
  virtual void StopTrackingParcel(const std::string& tracking_id,
                                  base::OnceCallback<void(bool)> callback);

  // Called to stop tracking multiple parcels.
  void StopTrackingParcels(
      const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
          parcel_identifiers,
      base::OnceCallback<void(bool)> callback);

  // Called to stop tracking all parcels.
  virtual void StopTrackingAllParcels(base::OnceCallback<void(bool)> callback);

  virtual ProductSpecificationsService* GetProductSpecificationsService();

  virtual ClusterManager* GetClusterManager();

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  // ProductSpecificationsSet::Observer:
  void OnProductSpecificationsSetRemoved(
      const ProductSpecificationsSet& set) override;

  // Get a weak pointer for this service instance.
  base::WeakPtr<ShoppingService> AsWeakPtr();

  void Shutdown() override;

 private:
  // "CommerceTabHelper" encompases both the content/ and ios/ versions.
  friend class CommerceTabHelper;
  friend class CommerceInternalsHandler;
  // Test classes are also friends.
  friend class ShoppingServiceTestBase;
  friend class ShoppingServiceTest;
  // TODO(b/362316113): Pass HistoryService through handler constructor instead
  // of having the handler as a friend.
  friend class ShoppingServiceHandler;

  // A notification that a WebWrapper has been created. This typically
  // corresponds to a user creating a tab.
  void WebWrapperCreated(WebWrapper* web);

  // A notification that a WebWrapper has been destroyed. This signals that the
  // web page backing the provided WebWrapper is about to be destroyed.
  // Typically corresponds to a user closing a tab.
  void WebWrapperDestroyed(WebWrapper* web);

  // A notification that a web wrapper finished a navigation in the primary
  // main frame.
  void DidNavigatePrimaryMainFrame(WebWrapper* web);

  // Handle main frame navigation for the product info API.
  void HandleDidNavigatePrimaryMainFrameForProductInfo(WebWrapper* web);

  // A notification that the user navigated away from the |from_url|.
  void DidNavigateAway(WebWrapper* web, const GURL& from_url);

  // A notification that the provided web wrapper has stopped loading. This does
  // not necessarily correspond to the page being completely finished loading
  // and is a useful signal to help detect and deal with single-page web apps.
  void DidStopLoading(WebWrapper* web);

  // A notification that the provided web wrapper has finished loading its main
  // frame.
  void DidFinishLoad(WebWrapper* web);

  // A notification that the active web wrapper was switched. This signal is
  // analogous to switching tabs.
  void OnWebWrapperSwitched(WebWrapper* web);

  // Called to signal that a WebWrapper is viewed. This happens when a new
  // navigation is committed in a focused tab.
  void OnWebWrapperViewed(WebWrapper* web);

  // Schedule (or reschedule) the on-page local extraction execution. Calling
  // this sequentially for the same web wrapper with the same URL will cancel
  // the pending task and schedule a new one. The script will, at most, run once
  // per unique navigation.
  void ScheduleProductInfoLocalExtraction(WebWrapper* web);

  // Check conditions to decide if the on-page info extraction should be run and
  // trigger the run if needed.
  void TryRunningLocalExtractionForProductInfo(base::WeakPtr<WebWrapper> web);

  // Actually run the on-page info extraction if the page is shopping page based
  // on `is_shopping_page`.
  void RunLocalExtractionForProductInfoForShoppingPage(
      base::WeakPtr<WebWrapper> web,
      const GURL& url,
      std::optional<bool> is_shopping_page);

  // A callback for recording metrics after page navigation and having
  // determined the page is shopping related.
  void PDPMetricsCallback(
      bool is_off_the_record,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata,
      const GURL& url);

  void HandleOptGuideProductInfoResponse(
      const GURL& url,
      WebWrapper* web,
      ProductInfoCallback callback,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Handle a response from the optimization guide on-demand API for product
  // info, specifically dealing with batch updates for bookmarks.
  void HandleOnDemandProductInfoResponseForBookmarks(
      BookmarkProductInfoUpdatedCallback callback,
      std::unordered_map<std::string, int64_t> url_to_id_map,
      const GURL& url,
      const base::flat_map<
          optimization_guide::proto::OptimizationType,
          optimization_guide::OptimizationGuideDecisionWithMetadata>&
          decisions);

  // Handle a generic on-demand request for product info. While this method
  // accepts a repeating callback, it should only ever be called once.
  void HandleOnDemandProductInfoResponse(
      RepeatingProductInfoCallback callback,
      const GURL& url,
      const base::flat_map<
          optimization_guide::proto::OptimizationType,
          optimization_guide::OptimizationGuideDecisionWithMetadata>&
          decisions);

  // Produce a ProductInfo object given OptimizationGuideMeta. The returned
  // unique_ptr is owned by the caller and will be empty if conversion failed
  // or there was no info. The value returned here can change during runtime so
  // it should not be used when deciding to build infrastructure.
  std::unique_ptr<ProductInfo> OptGuideResultToProductInfo(
      const optimization_guide::OptimizationMetadata& metadata);

  // Handle the result of running the local extraction fallback for product
  // info.
  void OnProductInfoLocalExtractionResult(const GURL url,
                                          ukm::SourceId source_id,
                                          base::Value result);

  // Tries to determine whether a page is a PDP only from information in meta
  // tags extracted from the page. If enough information is present to call the
  // page a PDP, this function returns true.
  static bool CheckIsPDPFromMetaOnly(const base::Value::Dict& on_page_meta_map);

  // Merge shopping data from existing |info| and the result of on-page
  // heuristics -- a JSON object holding key -> value pairs (a map) stored in
  // |on_page_data_map|. The merged data is written to |info|.
  static void MergeProductInfoData(ProductInfo* info,
                                   const base::Value::Dict& on_page_data_map);

  void HandleOptGuideMerchantInfoResponse(
      const GURL& url,
      MerchantInfoCallback callback,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Update the data stored in the cache.
  void UpdateProductInfoCache(const GURL& url,
                              bool needs_js,
                              std::unique_ptr<ProductInfo> info);

  // Get the data stored in the cache or nullptr if none exists.
  const ProductInfo* GetFromProductInfoCache(const GURL& url);

  // Whether APIs like |GetPriceInsightsInfoForURL| are enabled and allowed to
  // be used.
  bool IsPriceInsightsInfoApiEnabled();

  // Whether APIs like |IsShoppingPage| are enabled and allowed to be used.
  bool IsShoppingPageTypesApiEnabled();

  void HandleOptGuidePriceInsightsInfoResponse(
      const GURL& url,
      PriceInsightsInfoCallback callback,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  std::unique_ptr<PriceInsightsInfo> OptGuideResultToPriceInsightsInfo(
      const optimization_guide::OptimizationMetadata& metadata);

  // Handle main frame navigation for the price insights info API.
  void HandleDidNavigatePrimaryMainFrameForPriceInsightsInfo(WebWrapper* web);

  void HandleOptGuideShoppingPageTypesResponse(
      const GURL& url,
      IsShoppingPageCallback callback,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Whether APIs like |GetDiscountInfoForUrls| are enabled and allowed to be
  // used.
  bool IsDiscountInfoApiEnabled();

  void GetDiscountInfoFromOptGuide(const GURL& url,
                                   DiscountInfoCallback callback);

  void HandleOptGuideDiscountInfoResponse(
      const GURL& url,
      DiscountInfoCallback callback,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  std::vector<DiscountInfo> OptGuideResultToDiscountInfos(
      const optimization_guide::OptimizationMetadata& metadata);

  void OnGetAllDiscountsFromOptGuide(const std::vector<GURL>& urls,
                                     DiscountInfoCallback callback,
                                     const std::vector<DiscountsPair>& results);

  void SetDiscountsStorageForTesting(std::unique_ptr<DiscountsStorage> storage);

  void GetProductIdentifierForUrl(const GURL& url,
                                  UrlProductIdentifierTupleCallback callback);

  void UpdateRecentlyViewedURL(WebWrapper* web);

  // Return all ProductSpecificationsSets from ProductSpecificationsService.
  virtual const std::vector<ProductSpecificationsSet>
  GetAllProductSpecificationSets();

  void OnGetOnDemandProductInfo(const GURL& url,
                                const std::optional<const ProductInfo>& info);

  // TODO(b/362316113): Remove once history service is passed through handler
  // constructor.
  virtual void QueryHistoryForUrl(
      const GURL& url,
      history::HistoryService::QueryURLCallback callback);

  // The two-letter country code as detected on startup.
  std::string country_on_startup_;

  // The locale as detected on startup.
  std::string locale_on_startup_;

  // A handle to optimization guide for information about URLs that have
  // recently been navigated to.
  raw_ptr<optimization_guide::OptimizationGuideDecider> opt_guide_;

  raw_ptr<PrefService> pref_service_;

  raw_ptr<syncer::SyncService> sync_service_;

  const raw_ptr<bookmarks::BookmarkModel> bookmark_model_;

  std::unique_ptr<AccountChecker> account_checker_;

  std::unique_ptr<SubscriptionsManager> subscriptions_manager_;

  raw_ptr<power_bookmarks::PowerBookmarkService> power_bookmark_service_;

  raw_ptr<ProductSpecificationsService> product_specifications_service_;

  // The service's means of observing the bookmark model which is automatically
  // removed from the model when destroyed. This will be null if no
  // BookmarkModel is provided to the service.
  std::unique_ptr<ShoppingBookmarkModelObserver> shopping_bookmark_observer_;

  // The service's means of providing data to power bookmarks.
  std::unique_ptr<ShoppingPowerBookmarkDataProvider>
      shopping_power_bookmark_data_provider_;

  // A cache that retains commerce information for a URL as long as at least one
  // instance of the URL is open in a tab or mainteined by some other subsystem.
  CommerceInfoCache commerce_info_cache_;

  ProductSpecificationsCache product_specifications_cache_;

  std::unique_ptr<ProductSpecificationsServerProxy> product_specs_server_proxy_;

  std::unique_ptr<BookmarkUpdateManager> bookmark_update_manager_;

  // The object tracking metrics that are recorded at specific intervals.
  std::unique_ptr<commerce::metrics::ScheduledMetricsManager>
      scheduled_metrics_manager_;

  // The object handling discounts storage.
  std::unique_ptr<DiscountsStorage> discounts_storage_;

  // Object for tracking parcel status.
  std::unique_ptr<ParcelsManager> parcels_manager_;

  // A consent throttle that will hold callbacks until the specific consent is
  // obtained.
  unified_consent::ConsentThrottle bookmark_consent_throttle_;

  // The object for local extractions of commerce information.
  std::unique_ptr<commerce::WebExtractor> web_extractor_;

  std::unordered_set<WebWrapper*> open_web_wrappers_;

  // A list of UrlInfo ordered by most recently viewed. This is based on
  // selected tab (not necessarily navigation).
  std::vector<UrlInfo> recently_visited_tabs_;

  // Class for clustering products.
  std::unique_ptr<ClusterManager> cluster_manager_;

  // An observer of the ProductSpecificationsService that keeps track of the
  // URLs contained within each ProductSpecificationsSet. This is used to keep
  // the commerce info cache up to date.
  std::unique_ptr<ProductSpecificationsSet::Observer>
      prod_spec_url_ref_observer_;

  // Map between URL and a list of callbacks that are waiting for product info.
  // This is used to avoid repeated calls to get product info for the same URL.
  std::map<GURL, std::vector<ProductInfoCallback>>
      on_demand_product_info_callbacks_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  const raw_ptr<history::HistoryService> history_service_;

  const raw_ptr<sessions::TabRestoreService> tab_restore_service_{nullptr};

  base::ScopedObservation<ProductSpecificationsService,
                          ProductSpecificationsSet::Observer>
      product_specifications_observation_{this};

  base::CancelableTaskTracker cancelable_task_tracker_;

  // Ensure certain functions are being executed on the same thread.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ShoppingService> weak_ptr_factory_;
};

}  // namespace commerce

namespace base {

template <>
struct ScopedObservationTraits<commerce::ShoppingService,
                               commerce::SubscriptionsObserver> {
  static void AddObserver(commerce::ShoppingService* source,
                          commerce::SubscriptionsObserver* observer) {
    source->AddSubscriptionsObserver(observer);
  }
  static void RemoveObserver(commerce::ShoppingService* source,
                             commerce::SubscriptionsObserver* observer) {
    source->RemoveSubscriptionsObserver(observer);
  }
};

}  // namespace base

#endif  // COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_
