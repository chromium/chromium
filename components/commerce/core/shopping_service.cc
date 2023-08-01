// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_service.h"

#include <vector>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/commerce/core/bookmark_update_manager.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/metrics/metrics_utils.h"
#include "components/commerce/core/metrics/scheduled_metrics_manager.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/commerce/core/proto/price_insights.pb.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/commerce/core/shopping_bookmark_model_observer.h"
#include "components/commerce/core/shopping_power_bookmark_data_provider.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_manager.h"
#include "components/commerce/core/subscriptions/subscriptions_observer.h"
#include "components/commerce/core/web_wrapper.h"
#include "components/grit/components_resources.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/search/ntp_features.h"
#include "components/session_proto_db/session_proto_storage.h"
#include "components/sync/service/sync_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/url_constants.h"

namespace commerce {

// Open graph keys.
const char kOgImage[] = "image";
const char kOgPriceAmount[] = "price:amount";
const char kOgPriceCurrency[] = "price:currency";
const char kOgProductLink[] = "product_link";
const char kOgTitle[] = "title";
const char kOgType[] = "type";

// Specific open graph values we're interested in.
const char kOgTypeOgProduct[] = "og:product";
const char kOgTypeProductItem[] = "product.item";

const long kToMicroCurrency = 1e6;

const char kImageAvailabilityHistogramName[] =
    "Commerce.ShoppingService.ProductInfo.ImageAvailability";
const char kProductInfoJavascriptTime[] =
    "Commerce.ShoppingService.ProductInfo.JavascriptExecutionTime";

const uint64_t kProductInfoJavascriptDelayMs = 2000;

ProductInfo::ProductInfo() = default;
ProductInfo::ProductInfo(const ProductInfo&) = default;
ProductInfo& ProductInfo::operator=(const ProductInfo&) = default;
ProductInfo::~ProductInfo() = default;

ProductInfoCacheEntry::ProductInfoCacheEntry() = default;
ProductInfoCacheEntry::~ProductInfoCacheEntry() = default;

MerchantInfo::MerchantInfo() = default;
MerchantInfo::MerchantInfo(const MerchantInfo&) = default;
MerchantInfo& MerchantInfo::operator=(const MerchantInfo&) = default;
MerchantInfo::MerchantInfo(MerchantInfo&&) = default;
MerchantInfo::~MerchantInfo() = default;

PriceInsightsInfo::PriceInsightsInfo() = default;
PriceInsightsInfo::PriceInsightsInfo(const PriceInsightsInfo&) = default;
PriceInsightsInfo& PriceInsightsInfo::operator=(const PriceInsightsInfo&) =
    default;
PriceInsightsInfo::~PriceInsightsInfo() = default;

ShoppingService::ShoppingService(
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
    power_bookmarks::PowerBookmarkService* power_bookmark_service)
    : country_on_startup_(country_on_startup),
      locale_on_startup_(locale_on_startup),
      opt_guide_(opt_guide),
      pref_service_(pref_service),
      sync_service_(sync_service),
      bookmark_model_(bookmark_model),
      power_bookmark_service_(power_bookmark_service),
      bookmark_consent_throttle_(
          unified_consent::UrlKeyedDataCollectionConsentHelper::
              NewPersonalizedBookmarksDataCollectionConsentHelper(
                  sync_service)),
      weak_ptr_factory_(this) {
  // Register for the types of information we're allowed to receive from
  // optimization guide.
  if (opt_guide_) {
    std::vector<optimization_guide::proto::OptimizationType> types;

    // Don't register for info unless we're allowed to by an experiment.
    if (IsProductInfoApiEnabled() || IsPDPMetricsRecordingEnabled()) {
      types.push_back(
          optimization_guide::proto::OptimizationType::PRICE_TRACKING);
    }
    if (IsMerchantViewerEnabled()) {
      types.push_back(optimization_guide::proto::OptimizationType::
                          MERCHANT_TRUST_SIGNALS_V2);
    }
    if (IsPriceInsightsInfoApiEnabled()) {
      types.push_back(
          optimization_guide::proto::OptimizationType::PRICE_INSIGHTS);
    }

    opt_guide_->RegisterOptimizationTypes(types);
  }

  if (identity_manager) {
    account_checker_ = base::WrapUnique(new AccountChecker(
        pref_service, identity_manager, sync_service, url_loader_factory));
  }

  if (IsProductInfoApiEnabled() && identity_manager && account_checker_ &&
      subscription_proto_db) {
    subscriptions_manager_ = std::make_unique<SubscriptionsManager>(
        identity_manager, url_loader_factory, subscription_proto_db,
        account_checker_.get());
  }

  if (bookmark_model_) {
    shopping_bookmark_observer_ =
        std::make_unique<ShoppingBookmarkModelObserver>(
            bookmark_model, this, subscriptions_manager_.get());

    if (power_bookmark_service_ && IsProductInfoApiEnabled()) {
      shopping_power_bookmark_data_provider_ =
          std::make_unique<ShoppingPowerBookmarkDataProvider>(
              bookmark_model_, power_bookmark_service_, this);
    }
  }

  bookmark_update_manager_ = std::make_unique<BookmarkUpdateManager>(
      this, bookmark_model_, pref_service_);

  // In testing, the objects required for metrics may be null.
  if (pref_service_ && bookmark_model_ && subscriptions_manager_) {
    scheduled_metrics_manager_ =
        std::make_unique<metrics::ScheduledMetricsManager>(pref_service_, this);
  }
}

void ShoppingService::WebWrapperCreated(WebWrapper* web) {}

void ShoppingService::DidNavigatePrimaryMainFrame(WebWrapper* web) {
  HandleDidNavigatePrimaryMainFrameForProductInfo(web);
}

void ShoppingService::HandleDidNavigatePrimaryMainFrameForProductInfo(
    WebWrapper* web) {
  // We need optimization guide and one of the features that depend on the
  // price tracking signal to be enabled to do any of this.
  if (!opt_guide_ ||
      (!IsProductInfoApiEnabled() && !IsPDPMetricsRecordingEnabled())) {
    return;
  }

  UpdateProductInfoCacheForInsertion(web->GetLastCommittedURL());

  opt_guide_->CanApplyOptimization(
      web->GetLastCommittedURL(),
      optimization_guide::proto::OptimizationType::PRICE_TRACKING,
      base::BindOnce(
          [](base::WeakPtr<ShoppingService> service, const GURL& url,
             base::WeakPtr<WebWrapper> web_wrapper,
             optimization_guide::OptimizationGuideDecision decision,
             const optimization_guide::OptimizationMetadata& metadata) {
            if (service.WasInvalidated() || web_wrapper.WasInvalidated()) {
              return;
            }

            service->HandleOptGuideProductInfoResponse(
                url, web_wrapper.get(),
                base::BindOnce(
                    [](const GURL&, const absl::optional<ProductInfo>&) {}),
                decision, metadata);

            service->PDPMetricsCallback(web_wrapper->IsOffTheRecord(), decision,
                                        metadata);
          },
          weak_ptr_factory_.GetWeakPtr(), web->GetLastCommittedURL(),
          web->GetWeakPtr()));
}

void ShoppingService::DidNavigateAway(WebWrapper* web, const GURL& from_url) {
  UpdateProductInfoCacheForRemoval(from_url);
}

void ShoppingService::DidStopLoading(WebWrapper* web) {
  ScheduleProductInfoJavascript(web);
}

void ShoppingService::ScheduleProductInfoJavascript(WebWrapper* web) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsProductInfoApiEnabled()) {
    return;
  }

  // If we weren't provided a web wrapper or the page hasn't finished loading
  // for the navigation, do nothing. This will be called when the page has a
  // document to run the script on.
  if (!web || !web->IsFirstLoadForNavigationFinished()) {
    return;
  }

  auto it = product_info_cache_.find(web->GetLastCommittedURL().spec());

  if (it == product_info_cache_.end() || !it->second->needs_javascript_run) {
    return;
  }

  ProductInfoCacheEntry* entry = it->second.get();

  // If there's already a task scheduled, cancel it.
  if (entry->run_javascript_task.get()) {
    entry->run_javascript_task->Cancel();
    entry->run_javascript_task.reset();
  }

  entry->run_javascript_task = std::make_unique<base::CancelableOnceClosure>(
      base::BindOnce(&ShoppingService::TryRunningJavascriptForProductInfo,
                     weak_ptr_factory_.GetWeakPtr(), web->GetWeakPtr()));

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, entry->run_javascript_task->callback(),
      base::Milliseconds(kProductInfoJavascriptDelayMs));
}

void ShoppingService::DidFinishLoad(WebWrapper* web) {
  ScheduleProductInfoJavascript(web);
}

void ShoppingService::TryRunningJavascriptForProductInfo(
    base::WeakPtr<WebWrapper> web) {
  if (web.WasInvalidated()) {
    return;
  }

  // Make sure we actually need the javascript to run based on the cache.
  auto it = product_info_cache_.find(web->GetLastCommittedURL().spec());
  if (it == product_info_cache_.end() || !it->second->needs_javascript_run) {
    return;
  }

  if (!IsProductInfoApiEnabled()) {
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If there is both an entry in the cache and the javascript fallback needs
  // to run, run it.
  if (it != product_info_cache_.end() && it->second->needs_javascript_run) {
    // Since we're about to run the JS, flip the flag in the cache.
    it->second->needs_javascript_run = false;

    std::string script =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_QUERY_SHOPPING_META_JS);

    it->second->javascript_execution_start_time = base::Time::Now();
    web->RunJavascript(
        base::UTF8ToUTF16(script),
        base::BindOnce(&ShoppingService::OnProductInfoJavascriptResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       GURL(web->GetLastCommittedURL())));
  }
}

void ShoppingService::OnProductInfoJavascriptResult(const GURL url,
                                                    base::Value result) {
  // We should only ever get a string result from the script execution.
  if (!result.is_string()) {
    return;
  }

  // Look up the entry again in case it was deleted (ex. by navigation).
  auto it = product_info_cache_.find(url.spec());
  if (it == product_info_cache_.end()) {
    return;
  }

  base::UmaHistogramTimes(
      kProductInfoJavascriptTime,
      base::Time::Now() - it->second->javascript_execution_start_time);

  data_decoder::DataDecoder::ParseJsonIsolated(
      result.GetString(),
      base::BindOnce(&ShoppingService::OnProductInfoJsonSanitizationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), url));
}

void ShoppingService::OnProductInfoJsonSanitizationCompleted(
    const GURL url,
    data_decoder::DataDecoder::ValueOrError result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value() || !result.value().is_dict())
    return;

  auto it = product_info_cache_.find(url.spec());

  // If there was no entry, do nothing. Most likely this means the page
  // navigated before the script finished running.
  if (it == product_info_cache_.end()) {
    return;
  }

  ProductInfo* cached_info = it->second->product_info.get();

  bool pdp_detected_by_client = false;
  bool pdp_detected_by_server = false;

  // If there wasn't cached info but the javascript still ran, it means that
  // the server didn't detect the page as a PDP, so we should try to determine
  // whether it is using the meta extracted from the page. This will only
  // happen if the |kCommerceLocalPDPDetection| flag is enabled.
  pdp_detected_by_client = CheckIsPDPFromMetaOnly(result.value().GetDict());
  if (cached_info) {
    pdp_detected_by_server = true;

    MergeProductInfoData(cached_info, result.value().GetDict());
  }

  if (base::FeatureList::IsEnabled(kCommerceLocalPDPDetection)) {
    metrics::RecordPDPStateWithLocalMeta(pdp_detected_by_server,
                                         pdp_detected_by_client);
  }
}

bool ShoppingService::CheckIsPDPFromMetaOnly(
    const base::Value::Dict& on_page_meta_map) {
  const std::string* type = on_page_meta_map.FindString(kOgType);

  // If the og:type meta is present and the value is either og:product or
  // product.item, we'll consider it a product regardless of the other data.
  if (type && (*type == kOgTypeProductItem || *type == kOgTypeOgProduct)) {
    return true;
  }

  // Similarly, if there's a product link the page is probably a product.
  if (on_page_meta_map.FindString(kOgProductLink)) {
    return true;
  }

  const std::string* currency = on_page_meta_map.FindString(kOgPriceCurrency);
  const std::string* amount = on_page_meta_map.FindString(kOgPriceAmount);
  const std::string* image = on_page_meta_map.FindString(kOgImage);

  if (currency && amount && image) {
    return true;
  }

  return false;
}

void ShoppingService::WebWrapperDestroyed(WebWrapper* web) {
  UpdateProductInfoCacheForRemoval(web->GetLastCommittedURL());
}

void ShoppingService::UpdateProductInfoCacheForInsertion(const GURL& url) {
  if (!IsProductInfoApiEnabled())
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = product_info_cache_.find(url.spec());
  if (it != product_info_cache_.end()) {
    it->second->pages_with_url_open++;
  } else {
    product_info_cache_.emplace(url.spec(),
                                std::make_unique<ProductInfoCacheEntry>());
    product_info_cache_[url.spec()]->pages_with_url_open++;
  }
}

void ShoppingService::UpdateProductInfoCache(
    const GURL& url,
    bool needs_js,
    std::unique_ptr<ProductInfo> info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = product_info_cache_.find(url.spec());
  if (it == product_info_cache_.end())
    return;

  it->second->needs_javascript_run = needs_js;
  it->second->product_info = std::move(info);
}

const ProductInfo* ShoppingService::GetFromProductInfoCache(const GURL& url) {
  auto it = product_info_cache_.find(url.spec());
  if (it == product_info_cache_.end())
    return nullptr;

  return it->second->product_info.get();
}

void ShoppingService::UpdateProductInfoCacheForRemoval(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if the previously navigated URL cache needs to be cleared. If more
  // than one tab was open with the same URL, keep it in the cache but decrement
  // the internal count.
  auto it = product_info_cache_.find(url.spec());
  if (it != product_info_cache_.end()) {
    ProductInfoCacheEntry* entry = it->second.get();
    if (entry->pages_with_url_open <= 1) {
      if (entry->run_javascript_task.get()) {
        entry->run_javascript_task->Cancel();
        entry->run_javascript_task.reset();
      }
      product_info_cache_.erase(it);
    } else {
      entry->pages_with_url_open--;
    }
  }
}

void ShoppingService::PDPMetricsCallback(
    bool is_off_the_record,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (!IsPDPMetricsRecordingEnabled())
    return;

  metrics::RecordPDPMetrics(decision, metadata, pref_service_,
                            is_off_the_record, IsShoppingListEligible());

  bool supported_country =
      IsRegionLockedFeatureEnabled(kShoppingList, kShoppingListRegionLaunched,
                                   country_on_startup_, locale_on_startup_);
  metrics::RecordShoppingListIneligibilityReasons(
      pref_service_, account_checker_.get(), is_off_the_record,
      supported_country);
}

void ShoppingService::GetProductInfoForUrl(const GURL& url,
                                           ProductInfoCallback callback) {
  if (!opt_guide_ || !IsProductInfoApiEnabled()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), url, absl::nullopt));
    return;
  }

  const ProductInfo* cached_info = GetFromProductInfoCache(url);
  if (cached_info) {
    absl::optional<ProductInfo> info;
    // Make a copy based on the cached value.
    info.emplace(*cached_info);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), url, info));
    return;
  }

  opt_guide_->CanApplyOptimization(
      url, optimization_guide::proto::OptimizationType::PRICE_TRACKING,
      base::BindOnce(&ShoppingService::HandleOptGuideProductInfoResponse,
                     weak_ptr_factory_.GetWeakPtr(), url, nullptr,
                     std::move(callback)));
}

absl::optional<ProductInfo> ShoppingService::GetAvailableProductInfoForUrl(
    const GURL& url) {
  absl::optional<ProductInfo> optional_info;

  const ProductInfo* cached_info = GetFromProductInfoCache(url);
  if (cached_info)
    optional_info.emplace(*cached_info);

  return optional_info;
}

void ShoppingService::GetUpdatedProductInfoForBookmarks(
    const std::vector<base::Uuid>& bookmark_uuids,
    BookmarkProductInfoUpdatedCallback info_updated_callback) {
  std::vector<GURL> urls;
  std::unordered_map<std::string, base::Uuid> url_to_uuid_map;
  for (const base::Uuid& uuid : bookmark_uuids) {
    const bookmarks::BookmarkNode* bookmark =
        bookmarks::GetBookmarkNodeByUuid(bookmark_model_, uuid);

    std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
        power_bookmarks::GetNodePowerBookmarkMeta(bookmark_model_, bookmark);

    if (!meta || !meta->has_shopping_specifics())
      continue;

    if (!bookmark)
      continue;

    urls.push_back(bookmark->url());
    url_to_uuid_map[bookmark->url().spec()] = uuid;
  }

  opt_guide_->CanApplyOptimizationOnDemand(
      urls, {optimization_guide::proto::OptimizationType::PRICE_TRACKING},
      optimization_guide::proto::RequestContext::CONTEXT_BOOKMARKS,
      base::BindRepeating(&ShoppingService::OnProductInfoUpdatedOnDemand,
                          weak_ptr_factory_.GetWeakPtr(),
                          std::move(info_updated_callback),
                          std::move(url_to_uuid_map)));
}

size_t ShoppingService::GetMaxProductBookmarkUpdatesPerBatch() {
  return optimization_guide::features::
      MaxUrlsForOptimizationGuideServiceHintsFetch();
}

void ShoppingService::GetAllPriceTrackedBookmarks(
    base::OnceCallback<void(std::vector<const bookmarks::BookmarkNode*>)>
        callback) {
  commerce::GetAllPriceTrackedBookmarks(this, bookmark_model_,
                                        std::move(callback));
}

std::vector<const bookmarks::BookmarkNode*>
ShoppingService::GetAllShoppingBookmarks() {
  return commerce::GetAllShoppingBookmarks(bookmark_model_);
}

void ShoppingService::GetMerchantInfoForUrl(const GURL& url,
                                            MerchantInfoCallback callback) {
  if (!opt_guide_ || !IsMerchantViewerEnabled()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), url, absl::nullopt));
    return;
  }

  opt_guide_->CanApplyOptimization(
      url,
      optimization_guide::proto::OptimizationType::MERCHANT_TRUST_SIGNALS_V2,
      base::BindOnce(&ShoppingService::HandleOptGuideMerchantInfoResponse,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

void ShoppingService::GetPriceInsightsInfoForUrl(
    const GURL& url,
    PriceInsightsInfoCallback callback) {
  if (!opt_guide_ || !IsPriceInsightsInfoApiEnabled()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), url, absl::nullopt));
    return;
  }

  opt_guide_->CanApplyOptimization(
      url, optimization_guide::proto::OptimizationType::PRICE_INSIGHTS,
      base::BindOnce(&ShoppingService::HandleOptGuidePriceInsightsInfoResponse,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

bool ShoppingService::IsProductInfoApiEnabled() {
  return IsRegionLockedFeatureEnabled(
             kShoppingList, kShoppingListRegionLaunched, country_on_startup_,
             locale_on_startup_) ||
         (base::FeatureList::IsEnabled(ntp_features::kNtpChromeCartModule) &&
          IsEnabledForCountryAndLocale(ntp_features::kNtpChromeCartModule,
                                       country_on_startup_,
                                       locale_on_startup_));
}

bool ShoppingService::IsPDPMetricsRecordingEnabled() {
  return IsRegionLockedFeatureEnabled(kShoppingPDPMetrics,
                                      kShoppingPDPMetricsRegionLaunched,
                                      country_on_startup_, locale_on_startup_);
}

bool ShoppingService::IsMerchantViewerEnabled() {
  return IsRegionLockedFeatureEnabled(kCommerceMerchantViewer,
                                      kCommerceMerchantViewerRegionLaunched,
                                      country_on_startup_, locale_on_startup_);
}

bool ShoppingService::IsCommercePriceTrackingEnabled() {
  return IsRegionLockedFeatureEnabled(kCommercePriceTracking,
                                      kCommercePriceTrackingRegionLaunched,
                                      country_on_startup_, locale_on_startup_);
}

bool ShoppingService::IsPriceInsightsEligible() {
  if (!IsRegionLockedFeatureEnabled(kPriceInsights,
                                    kPriceInsightsRegionLaunched,
                                    country_on_startup_, locale_on_startup_)) {
    return false;
  }
  return account_checker_ &&
         account_checker_->IsAnonymizedUrlDataCollectionEnabled();
}

bool ShoppingService::IsPriceInsightsInfoApiEnabled() {
  return IsRegionLockedFeatureEnabled(kPriceInsights,
                                      kPriceInsightsRegionLaunched,
                                      country_on_startup_, locale_on_startup_);
}

void ShoppingService::HandleOptGuideProductInfoResponse(
    const GURL& url,
    WebWrapper* web,
    ProductInfoCallback callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // If optimization guide returns negative, return a negative signal with an
  // empty data object.
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    std::move(callback).Run(url, absl::nullopt);

    // If doing local PDP detection, we might still want to run this.
    if (base::FeatureList::IsEnabled(kCommerceLocalPDPDetection) &&
        url.SchemeIsHTTPOrHTTPS()) {
      UpdateProductInfoCache(url, true, nullptr);
      if (web) {
        ScheduleProductInfoJavascript(web);
      }
    }

    return;
  }

  std::unique_ptr<ProductInfo> info = OptGuideResultToProductInfo(metadata);

  absl::optional<ProductInfo> optional_info;
  // The product info is considered valid only if it has a country code.
  if (info && !info->country_code.empty()) {
    optional_info.emplace(*info);
    UpdateProductInfoCache(url, true, std::move(info));
  }

  // TODO(mdjones): Longer-term it might make sense to wait until the
  // javascript has run to execute this. The problem is that optimization guide
  // is fast and the javascript is slow.
  std::move(callback).Run(url, optional_info);

  // Check if we still need to run the javascript. This could happen if page
  // load happened prior to optimization guide responding.
  if (web) {
    ScheduleProductInfoJavascript(web);
  }
}

std::unique_ptr<ProductInfo> ShoppingService::OptGuideResultToProductInfo(
    const optimization_guide::OptimizationMetadata& metadata) {
  if (!metadata.any_metadata().has_value())
    return nullptr;

  absl::optional<commerce::PriceTrackingData> parsed_any =
      optimization_guide::ParsedAnyMetadata<commerce::PriceTrackingData>(
          metadata.any_metadata().value());
  commerce::PriceTrackingData price_data = parsed_any.value();

  if (!parsed_any.has_value() || !price_data.IsInitialized())
    return nullptr;

  const commerce::BuyableProduct buyable_product = price_data.buyable_product();

  std::unique_ptr<ProductInfo> info = std::make_unique<ProductInfo>();

  if (buyable_product.has_title())
    info->title = buyable_product.title();

  if (buyable_product.has_gpc_title()) {
    info->product_cluster_title = buyable_product.gpc_title();
  }

  if (buyable_product.has_image_url()) {
    info->server_image_available = true;

    // Only keep the server-provided image if we're allowed to.
    if (base::FeatureList::IsEnabled(commerce::kCommerceAllowServerImages)) {
      info->image_url = GURL(buyable_product.image_url());
    }
  } else {
    info->server_image_available = false;
  }

  if (buyable_product.has_offer_id())
    info->offer_id = buyable_product.offer_id();

  if (buyable_product.has_product_cluster_id())
    info->product_cluster_id = buyable_product.product_cluster_id();

  if (buyable_product.has_current_price()) {
    info->currency_code = buyable_product.current_price().currency_code();
    info->amount_micros = buyable_product.current_price().amount_micros();
  }

  if (buyable_product.has_country_code())
    info->country_code = buyable_product.country_code();

  // Check to see if there was a price drop associated with this product. Those
  // prices take priority over what BuyableProduct has.
  if (price_data.has_product_update()) {
    const commerce::ProductPriceUpdate price_update =
        price_data.product_update();

    // Both new and old price should exist and have the same currency code.
    bool currency_codes_match = price_update.new_price().currency_code() ==
                                price_update.old_price().currency_code();

    if (price_update.has_new_price() &&
        info->currency_code == price_update.new_price().currency_code() &&
        currency_codes_match) {
      info->amount_micros = price_update.new_price().amount_micros();
    }
    if (price_update.has_old_price() &&
        info->currency_code == price_update.old_price().currency_code() &&
        currency_codes_match) {
      info->previous_amount_micros.emplace(
          price_update.old_price().amount_micros());
    }
  }

  return info;
}

void ShoppingService::OnProductInfoUpdatedOnDemand(
    BookmarkProductInfoUpdatedCallback callback,
    std::unordered_map<std::string, base::Uuid> url_to_uuid_map,
    const GURL& url,
    const base::flat_map<
        optimization_guide::proto::OptimizationType,
        optimization_guide::OptimizationGuideDecisionWithMetadata>& decisions) {
  auto iter = decisions.find(
      optimization_guide::proto::OptimizationType::PRICE_TRACKING);

  if (iter == decisions.cend())
    return;

  optimization_guide::OptimizationGuideDecisionWithMetadata decision =
      iter->second;

  // Only fire the callback for price tracking info if successful.
  if (decision.decision !=
      optimization_guide::OptimizationGuideDecision::kTrue) {
    return;
  }

  std::unique_ptr<ProductInfo> info =
      OptGuideResultToProductInfo(decision.metadata);

  absl::optional<ProductInfo> optional_info;
  if (info) {
    optional_info.emplace(*info);
    UpdateProductInfoCache(url, false, std::move(info));

    std::move(callback).Run(url_to_uuid_map[url.spec()], url, optional_info);
  }
}

void ShoppingService::MergeProductInfoData(
    ProductInfo* info,
    const base::Value::Dict& on_page_data_map) {
  if (!info)
    return;

  // This will be true if any of the data found in |on_page_data_map| is used to
  // populate fields in |meta|.
  bool data_was_merged = false;

  bool had_fallback_image = false;

  for (const auto [key, value] : on_page_data_map) {
    if (base::CompareCaseInsensitiveASCII(key, kOgTitle) == 0) {
      if (info->title.empty()) {
        info->title = value.GetString();
        base::UmaHistogramEnumeration(
            "Commerce.ShoppingService.ProductInfo.FallbackDataContent",
            ProductInfoFallback::kTitle);
        data_was_merged = true;
      }
    } else if (base::CompareCaseInsensitiveASCII(key, kOgImage) == 0) {
      had_fallback_image = true;

      // If the product already has an image, add the one found on the page as
      // a fallback. The original image, if it exists, should have been
      // retrieved from the proto received from optimization guide before this
      // callback runs.
      if (info->image_url.is_empty()) {
        GURL og_url(value.GetString());

        // Only keep the local image if we're allowed to.
        if (base::FeatureList::IsEnabled(commerce::kCommerceAllowLocalImages))
          info->image_url.Swap(&og_url);

        base::UmaHistogramEnumeration(
            "Commerce.ShoppingService.ProductInfo.FallbackDataContent",
            ProductInfoFallback::kLeadImage);
        data_was_merged = true;
      }
      // TODO(mdjones): Add capacity for fallback images when necessary.

    } else if (base::CompareCaseInsensitiveASCII(key, kOgPriceCurrency) == 0) {
      if (info->amount_micros <= 0) {
        double amount = 0;
        if (base::StringToDouble(*on_page_data_map.FindString(kOgPriceAmount),
                                 &amount)) {
          // Currency is stored in micro-units rather than standard units, so we
          // need to convert (open graph provides standard units).
          info->amount_micros = amount * kToMicroCurrency;
          info->currency_code = value.GetString();
          base::UmaHistogramEnumeration(
              "Commerce.ShoppingService.ProductInfo.FallbackDataContent",
              ProductInfoFallback::kPrice);
          data_was_merged = true;
        }
      }
    }
  }

  ProductImageAvailability image_availability =
      ProductImageAvailability::kNeitherAvailable;
  if (info->server_image_available && had_fallback_image) {
    image_availability = ProductImageAvailability::kBothAvailable;
  } else if (had_fallback_image) {
    image_availability = ProductImageAvailability::kLocalOnly;
  } else if (info->server_image_available) {
    image_availability = ProductImageAvailability::kServerOnly;
  }
  base::UmaHistogramEnumeration(kImageAvailabilityHistogramName,
                                image_availability);

  base::UmaHistogramBoolean(
      "Commerce.ShoppingService.ProductInfo.FallbackDataUsed", data_was_merged);
}

void ShoppingService::HandleOptGuideMerchantInfoResponse(
    const GURL& url,
    MerchantInfoCallback callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // If optimization guide returns negative, return a negative signal with an
  // empty data object.
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    std::move(callback).Run(url, absl::nullopt);
    return;
  }

  absl::optional<MerchantInfo> info;

  if (metadata.any_metadata().has_value()) {
    absl::optional<commerce::MerchantTrustSignalsV2> parsed_any =
        optimization_guide::ParsedAnyMetadata<commerce::MerchantTrustSignalsV2>(
            metadata.any_metadata().value());
    commerce::MerchantTrustSignalsV2 merchant_data = parsed_any.value();
    if (parsed_any.has_value() && merchant_data.IsInitialized()) {
      info.emplace();

      if (merchant_data.has_merchant_star_rating()) {
        info->star_rating = merchant_data.merchant_star_rating();
      }

      if (merchant_data.has_merchant_count_rating()) {
        info->count_rating = merchant_data.merchant_count_rating();
      }

      if (merchant_data.has_merchant_details_page_url()) {
        GURL details_page_url = GURL(merchant_data.merchant_details_page_url());
        if (details_page_url.is_valid()) {
          info->details_page_url = details_page_url;
        }
      }

      if (merchant_data.has_has_return_policy()) {
        info->has_return_policy = merchant_data.has_return_policy();
      }

      if (merchant_data.has_non_personalized_familiarity_score()) {
        info->non_personalized_familiarity_score =
            merchant_data.non_personalized_familiarity_score();
      }

      if (merchant_data.has_contains_sensitive_content()) {
        info->contains_sensitive_content =
            merchant_data.contains_sensitive_content();
      }

      if (merchant_data.has_proactive_message_disabled()) {
        info->proactive_message_disabled =
            merchant_data.proactive_message_disabled();
      }
    }
  }

  std::move(callback).Run(url, std::move(info));
}

void ShoppingService::HandleOptGuidePriceInsightsInfoResponse(
    const GURL& url,
    PriceInsightsInfoCallback callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue ||
      !metadata.any_metadata().has_value()) {
    std::move(callback).Run(url, absl::nullopt);
    return;
  }

  absl::optional<commerce::PriceInsightsData> parsed_any =
      optimization_guide::ParsedAnyMetadata<commerce::PriceInsightsData>(
          metadata.any_metadata().value());
  commerce::PriceInsightsData insights_data = parsed_any.value();

  if (!parsed_any.has_value() || !insights_data.IsInitialized() ||
      !insights_data.has_product_cluster_id() ||
      insights_data.product_cluster_id() == 0) {
    std::move(callback).Run(url, absl::nullopt);
    return;
  }

  absl::optional<PriceInsightsInfo> info;
  info.emplace();

  info->product_cluster_id = insights_data.product_cluster_id();

  bool has_range = insights_data.has_price_range() &&
                   !insights_data.price_range().currency_code().empty();
  if (has_range) {
    info->currency_code = insights_data.price_range().currency_code();
    info->typical_low_price_micros =
        insights_data.price_range().lowest_typical_price_micros();
    info->typical_high_price_micros =
        insights_data.price_range().highest_typical_price_micros();
  }

  if (insights_data.has_price_history() &&
      !insights_data.price_history().currency_code().empty()) {
    bool currency_code_match =
        has_range ? insights_data.price_history().currency_code() ==
                        insights_data.price_range().currency_code()
                  : true;
    if (currency_code_match) {
      const commerce::PriceHistory history = insights_data.price_history();

      info->currency_code = history.currency_code();

      if (history.has_attributes()) {
        info->catalog_attributes = history.attributes();
      }

      for (int i = 0; i < history.price_points_size(); i++) {
        info->catalog_history_prices.emplace_back(
            history.price_points(i).date(),
            history.price_points(i).min_price_micros());
      }

      if (history.has_jackpot_url() && !history.jackpot_url().empty()) {
        info->jackpot_url = GURL(history.jackpot_url());
      }
    }
  }

  if (insights_data.has_price_bucket()) {
    info->price_bucket = PriceBucket(insights_data.price_bucket());
  }

  if (insights_data.has_has_multiple_catalogs()) {
    info->has_multiple_catalogs = insights_data.has_multiple_catalogs();
  }

  std::move(callback).Run(url, std::move(info));
}

void ShoppingService::Subscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/1377515): When calling this api, we should always have a
  // valid subscriptions_manager_ and there is no need to do the null check. We
  // can build an internal system for error logging.
  if (subscriptions_manager_) {
    subscriptions_manager_->Subscribe(std::move(subscriptions),
                                      std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void ShoppingService::Unsubscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/1377515): When calling this api, we should always have a
  // valid subscriptions_manager_ and there is no need to do the null check. We
  // can build an internal system for error logging.
  if (subscriptions_manager_) {
    subscriptions_manager_->Unsubscribe(std::move(subscriptions),
                                        std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void ShoppingService::AddSubscriptionsObserver(
    SubscriptionsObserver* observer) {
  if (subscriptions_manager_) {
    subscriptions_manager_->AddObserver(observer);
  }
}

void ShoppingService::RemoveSubscriptionsObserver(
    SubscriptionsObserver* observer) {
  if (subscriptions_manager_) {
    subscriptions_manager_->RemoveObserver(observer);
  }
}

void ShoppingService::GetAllSubscriptions(
    SubscriptionType type,
    base::OnceCallback<void(std::vector<CommerceSubscription>)> callback) {
  if (subscriptions_manager_) {
    subscriptions_manager_->GetAllSubscriptions(type, std::move(callback));
  } else {
    CHECK_IS_TEST();
  }
}

void ShoppingService::IsSubscribed(CommerceSubscription subscription,
                                   base::OnceCallback<void(bool)> callback) {
  if (subscriptions_manager_) {
    subscriptions_manager_->IsSubscribed(std::move(subscription),
                                         std::move(callback));
  } else {
    CHECK_IS_TEST();
  }
}

bool ShoppingService::IsSubscribedFromCache(
    const CommerceSubscription& subscription) {
  if (subscriptions_manager_) {
    return subscriptions_manager_->IsSubscribedFromCache(subscription);
  } else {
    CHECK_IS_TEST();
  }
  return false;
}

void ShoppingService::FetchPriceEmailPref() {
  if (account_checker_) {
    account_checker_->FetchPriceEmailPref();
  }
}

void ShoppingService::ScheduleSavedProductUpdate() {
  bookmark_update_manager_->ScheduleUpdate();
}

bool ShoppingService::IsShoppingListEligible() {
  return IsShoppingListEligible(account_checker_.get(), pref_service_,
                                country_on_startup_, locale_on_startup_);
}

void ShoppingService::WaitForReady(
    base::OnceCallback<void(ShoppingService*)> callback) {
  bookmark_consent_throttle_.EnqueueRequest(base::BindOnce(
      [](base::WeakPtr<ShoppingService> service,
         syncer::SyncService* sync_service,
         base::OnceCallback<void(ShoppingService*)> callback,
         bool has_consent) {
        if (service.WasInvalidated() || !sync_service ||
            sync_service->GetTransportState() !=
                syncer::SyncService::TransportState::ACTIVE) {
          std::move(callback).Run(nullptr);
          return;
        }
        std::move(callback).Run(service.get());
      },
      AsWeakPtr(), sync_service_, std::move(callback)));
}

bool ShoppingService::IsShoppingListEligible(AccountChecker* account_checker,
                                             PrefService* prefs,
                                             const std::string& country_code,
                                             const std::string& locale) {
  if (!IsRegionLockedFeatureEnabled(kShoppingList, kShoppingListRegionLaunched,
                                    country_code, locale)) {
    return false;
  }

  if (!prefs || !IsShoppingListAllowedForEnterprise(prefs))
    return false;

  // Make sure the user allows subscriptions to be made and that we can fetch
  // store data.
  if (!account_checker || !account_checker->IsSignedIn() ||
      !account_checker->IsSyncingBookmarks() ||
      !account_checker->IsAnonymizedUrlDataCollectionEnabled() ||
      !account_checker->IsWebAndAppActivityEnabled() ||
      account_checker->IsSubjectToParentalControls()) {
    return false;
  }

  return true;
}

void ShoppingService::IsClusterIdTrackedByUser(
    uint64_t cluster_id,
    base::OnceCallback<void(bool)> callback) {
  if (!subscriptions_manager_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  CommerceSubscription sub(
      SubscriptionType::kPriceTrack, IdentifierType::kProductClusterId,
      base::NumberToString(cluster_id), ManagementType::kUserManaged);
  subscriptions_manager_->IsSubscribed(std::move(sub), std::move(callback));
}

base::WeakPtr<ShoppingService> ShoppingService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ShoppingService::Shutdown() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ShoppingService::~ShoppingService() = default;

}  // namespace commerce
