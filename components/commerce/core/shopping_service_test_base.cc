// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_service_test_base.h"

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/proto/discounts.pb.h"
#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/commerce/core/proto/price_insights.pb.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

using optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback;
using optimization_guide::OptimizationGuideDecision;
using optimization_guide::OptimizationGuideDecisionCallback;
using optimization_guide::OptimizationGuideDecisionWithMetadata;
using optimization_guide::OptimizationMetadata;
using optimization_guide::proto::Any;
using optimization_guide::proto::OptimizationType;
using optimization_guide::proto::RequestContext;

namespace commerce {

const uint64_t kInvalidDiscountId = 0;

MockOptGuideDecider::MockOptGuideDecider() = default;
MockOptGuideDecider::~MockOptGuideDecider() = default;

void MockOptGuideDecider::RegisterOptimizationTypes(
    const std::vector<OptimizationType>& optimization_types) {}

void MockOptGuideDecider::CanApplyOptimization(
    const GURL& url,
    OptimizationType optimization_type,
    OptimizationGuideDecisionCallback callback) {
  bool type_matches = optimization_type_.has_value() &&
                      optimization_type_.value() == optimization_type;
  bool url_matches = response_url_.has_value() && url == response_url_.value();

  if (!type_matches || !url_matches) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), OptimizationGuideDecision::kUnknown,
                       OptimizationMetadata()));
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), optimization_decision_.value(),
                     optimization_data_.value()));
}

OptimizationGuideDecision MockOptGuideDecider::CanApplyOptimization(
    const GURL& url,
    OptimizationType optimization_type,
    OptimizationMetadata* optimization_metadata) {
  // We don't use the synchronous API in the shopping service.
  NOTREACHED();

  return OptimizationGuideDecision::kUnknown;
}

void MockOptGuideDecider::CanApplyOptimizationOnDemand(
    const std::vector<GURL>& urls,
    const base::flat_set<OptimizationType>& optimization_types,
    RequestContext request_context,
    OnDemandOptimizationGuideDecisionRepeatingCallback callback) {
  if (optimization_types.contains(OptimizationType::PRICE_TRACKING)) {
    for (const GURL& url : urls) {
      if (on_demand_shopping_responses_.find(url.spec()) ==
          on_demand_shopping_responses_.end()) {
        continue;
      }

      base::flat_map<OptimizationType, OptimizationGuideDecisionWithMetadata>
          decision_map;
      decision_map[OptimizationType::PRICE_TRACKING] =
          on_demand_shopping_responses_[url.spec()];
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(callback, url, std::move(decision_map)));
    }
  }
}

void MockOptGuideDecider::AddOnDemandShoppingResponse(
    const GURL& url,
    const OptimizationGuideDecision decision,
    const OptimizationMetadata& data) {
  optimization_guide::OptimizationGuideDecisionWithMetadata response;
  response.decision = decision;
  response.metadata = data;

  on_demand_shopping_responses_[url.spec()] = response;
}

void MockOptGuideDecider::SetResponse(const GURL& url,
                                      const OptimizationType type,
                                      const OptimizationGuideDecision decision,
                                      const OptimizationMetadata& data) {
  response_url_ = url;
  optimization_type_ = type;
  optimization_decision_ = decision;
  optimization_data_ = data;
}

OptimizationMetadata MockOptGuideDecider::BuildPriceTrackingResponse(
    const std::string& title,
    const std::string& image_url,
    const uint64_t offer_id,
    const uint64_t product_cluster_id,
    const std::string& country_code,
    const int64_t amount_micros,
    const std::string& currency_code,
    const std::string& gpc_title) {
  OptimizationMetadata meta;

  PriceTrackingData price_tracking_data;
  BuyableProduct* buyable_product =
      price_tracking_data.mutable_buyable_product();

  if (!title.empty())
    buyable_product->set_title(title);

  if (!gpc_title.empty()) {
    buyable_product->set_gpc_title(gpc_title);
  }

  if (!image_url.empty())
    buyable_product->set_image_url(image_url);

  buyable_product->set_offer_id(offer_id);
  buyable_product->set_product_cluster_id(product_cluster_id);

  if (!country_code.empty())
    buyable_product->set_country_code(country_code);

  ProductPrice* price = buyable_product->mutable_current_price();
  price->set_currency_code(currency_code);
  price->set_amount_micros(amount_micros);

  Any any;
  any.set_type_url(price_tracking_data.GetTypeName());
  price_tracking_data.SerializeToString(any.mutable_value());
  meta.set_any_metadata(any);

  return meta;
}

void MockOptGuideDecider::AddPriceUpdateToPriceTrackingResponse(
    OptimizationMetadata* out_meta,
    const std::string& currency_code,
    const int64_t current_price,
    const int64_t previous_price) {
  PriceTrackingData price_tracking_data =
      optimization_guide::ParsedAnyMetadata<PriceTrackingData>(
          out_meta->any_metadata().value())
          .value();

  ProductPriceUpdate* price_update =
      price_tracking_data.mutable_product_update();
  price_update->mutable_new_price()->set_amount_micros(current_price);
  price_update->mutable_new_price()->set_currency_code(currency_code);
  price_update->mutable_old_price()->set_amount_micros(previous_price);
  price_update->mutable_old_price()->set_currency_code(currency_code);

  Any any;
  any.set_type_url(price_tracking_data.GetTypeName());
  price_tracking_data.SerializeToString(any.mutable_value());
  out_meta->set_any_metadata(any);
}

OptimizationMetadata MockOptGuideDecider::BuildMerchantTrustResponse(
    const float star_rating,
    const uint32_t count_rating,
    const std::string& details_page_url,
    const bool has_return_policy,
    const bool contains_sensitive_content) {
  OptimizationMetadata meta;

  MerchantTrustSignalsV2 merchant_trust_data;
  merchant_trust_data.set_merchant_star_rating(star_rating);
  merchant_trust_data.set_merchant_count_rating(count_rating);
  merchant_trust_data.set_merchant_details_page_url(details_page_url);
  merchant_trust_data.set_has_return_policy(has_return_policy);
  merchant_trust_data.set_contains_sensitive_content(
      contains_sensitive_content);

  Any any;
  any.set_type_url(merchant_trust_data.GetTypeName());
  merchant_trust_data.SerializeToString(any.mutable_value());
  meta.set_any_metadata(any);

  return meta;
}

OptimizationMetadata MockOptGuideDecider::BuildPriceInsightsResponse(
    const uint64_t product_cluster_id,
    const std::string& price_range_currency_code,
    const int64_t low_typical_price_micros,
    const int64_t high_typical_price_micros,
    const std::string& price_history_currency_code,
    const std::string& attributes,
    const std::vector<std::tuple<std::string, int64_t>>& history_prices,
    const std::string& jackpot_url,
    const PriceBucket& price_bucket,
    const bool has_multiple_catalogs) {
  OptimizationMetadata meta;

  PriceInsightsData price_insights_data;

  price_insights_data.set_product_cluster_id(product_cluster_id);

  PriceRange* price_range = price_insights_data.mutable_price_range();
  price_range->set_currency_code(price_range_currency_code);
  price_range->set_lowest_typical_price_micros(low_typical_price_micros);
  price_range->set_highest_typical_price_micros(high_typical_price_micros);

  PriceHistory* price_history = price_insights_data.mutable_price_history();
  price_history->set_currency_code(price_history_currency_code);
  price_history->set_attributes(attributes);
  for (auto price : history_prices) {
    PricePoint* price_point = price_history->add_price_points();
    price_point->set_date(std::get<0>(price));
    price_point->set_min_price_micros(std::get<1>(price));
  }
  price_history->set_jackpot_url(jackpot_url);

  PriceInsightsData_PriceBucket bucket = PriceInsightsData_PriceBucket_UNKNOWN;
  if (price_bucket == PriceBucket::kLowPrice) {
    bucket = PriceInsightsData_PriceBucket_LOW_PRICE;
  } else if (price_bucket == PriceBucket::kTypicalPrice) {
    bucket = PriceInsightsData_PriceBucket_TYPICAL_PRICE;
  } else if (price_bucket == PriceBucket::kHighPrice) {
    bucket = PriceInsightsData_PriceBucket_HIGH_PRICE;
  }

  price_insights_data.set_price_bucket(bucket);
  price_insights_data.set_has_multiple_catalogs(has_multiple_catalogs);

  Any any;
  any.set_type_url(price_insights_data.GetTypeName());
  price_insights_data.SerializeToString(any.mutable_value());
  meta.set_any_metadata(any);

  return meta;
}

OptimizationMetadata MockOptGuideDecider::BuildDiscountsResponse(
    const std::vector<DiscountInfo>& infos) {
  OptimizationMetadata meta;

  DiscountsData discounts_data;

  std::vector<DiscountClusterType> checked_cluster_types;
  for (const auto& info_to_check : infos) {
    if (std::find(checked_cluster_types.begin(), checked_cluster_types.end(),
                  info_to_check.cluster_type) != checked_cluster_types.end()) {
      continue;
    }
    checked_cluster_types.push_back(info_to_check.cluster_type);

    DiscountCluster* cluster = discounts_data.add_discount_clusters();

    DiscountCluster_ClusterType cluster_type =
        DiscountCluster_ClusterType_TYPE_UNSPECIFIED;
    if (info_to_check.cluster_type == DiscountClusterType::kOfferLevel) {
      cluster_type = DiscountCluster_ClusterType_OFFER_LEVEL;
    }
    cluster->set_cluster_type(cluster_type);

    for (const auto& info : infos) {
      if (info.cluster_type != info_to_check.cluster_type) {
        continue;
      }

      Discount* discount = cluster->add_discounts();
      if (info.id != kInvalidDiscountId) {
        discount->set_id(info.id);
      }

      Discount_Type type = Discount_Type_TYPE_UNSPECIFIED;
      if (info.type == DiscountType::kFreeListingWithCode) {
        type = Discount_Type_FREE_LISTING_WITH_CODE;
      }
      discount->set_type(type);

      Discount_Description* description = discount->mutable_description();
      description->set_language_code(info.language_code);
      description->set_detail(info.description_detail);
      if (info.terms_and_conditions.has_value()) {
        description->set_terms_and_conditions(
            info.terms_and_conditions.value());
      }
      description->set_value_text(info.value_in_text);
      discount->set_expiry_time_sec(info.expiry_time_sec);
      discount->set_is_merchant_wide(info.is_merchant_wide);
      if (info.discount_code.has_value()) {
        discount->set_discount_code(info.discount_code.value());
      }
      discount->set_offer_id(info.offer_id);
    }
  }

  Any any;
  any.set_type_url(discounts_data.GetTypeName());
  discounts_data.SerializeToString(any.mutable_value());
  meta.set_any_metadata(any);

  return meta;
}

MockWebWrapper::MockWebWrapper(const GURL& last_committed_url,
                               bool is_off_the_record)
    : MockWebWrapper(last_committed_url, is_off_the_record, nullptr) {}

MockWebWrapper::MockWebWrapper(const GURL& last_committed_url,
                               bool is_off_the_record,
                               base::Value* result)
    : last_committed_url_(last_committed_url),
      is_off_the_record_(is_off_the_record),
      mock_js_result_(result) {}

MockWebWrapper::~MockWebWrapper() = default;

const GURL& MockWebWrapper::GetLastCommittedURL() {
  return last_committed_url_;
}

bool MockWebWrapper::IsFirstLoadForNavigationFinished() {
  return is_first_load_finished_;
}

void MockWebWrapper::SetIsFirstLoadForNavigationFinished(bool finished) {
  is_first_load_finished_ = finished;
}

bool MockWebWrapper::IsOffTheRecord() {
  return is_off_the_record_;
}

void MockWebWrapper::RunJavascript(
    const std::u16string& script,
    base::OnceCallback<void(const base::Value)> callback) {
  if (!mock_js_result_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::Value()));
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), mock_js_result_->Clone()));
}

ShoppingServiceTestBase::ShoppingServiceTestBase()
    : local_or_syncable_bookmark_model_(
          bookmarks::TestBookmarkClient::CreateModel()),
      account_bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()),
      opt_guide_(std::make_unique<MockOptGuideDecider>()),
      pref_service_(std::make_unique<TestingPrefServiceSimple>()),
      identity_test_env_(std::make_unique<signin::IdentityTestEnvironment>()),
      sync_service_(std::make_unique<syncer::TestSyncService>()),
      test_url_loader_factory_(
          std::make_unique<network::TestURLLoaderFactory>()) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  RegisterPrefs(pref_service_->registry());
  pref_service_->registry()->RegisterBooleanPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
}

ShoppingServiceTestBase::~ShoppingServiceTestBase() = default;

void ShoppingServiceTestBase::SetUp() {
  shopping_service_ = std::make_unique<ShoppingService>(
      "us", "en-us", local_or_syncable_bookmark_model_.get(),
      account_bookmark_model_.get(), opt_guide_.get(), pref_service_.get(),
      identity_test_env_->identity_manager(), sync_service_.get(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          test_url_loader_factory_.get()),
      nullptr, nullptr, nullptr, nullptr, nullptr);
}

void ShoppingServiceTestBase::TestBody() {}

void ShoppingServiceTestBase::TearDown() {
  // Reset the enabled/disabled features after each test.
  test_features_.Reset();
}

void ShoppingServiceTestBase::DidNavigatePrimaryMainFrame(WebWrapper* web) {
  shopping_service_->DidNavigatePrimaryMainFrame(web);
  base::RunLoop().RunUntilIdle();
}

void ShoppingServiceTestBase::DidFinishLoad(WebWrapper* web) {
  shopping_service_->DidFinishLoad(web);
  base::RunLoop().RunUntilIdle();
}

void ShoppingServiceTestBase::SimulateProductInfoJsTaskFinished() {
  task_environment_.FastForwardBy(
      base::Milliseconds(kProductInfoJavascriptDelayMs));
  base::RunLoop().RunUntilIdle();
}

void ShoppingServiceTestBase::DidNavigateAway(WebWrapper* web,
                                              const GURL& url) {
  shopping_service_->DidNavigateAway(web, url);
  base::RunLoop().RunUntilIdle();
}

void ShoppingServiceTestBase::WebWrapperDestroyed(WebWrapper* web) {
  shopping_service_->WebWrapperDestroyed(web);
  base::RunLoop().RunUntilIdle();
}

void ShoppingServiceTestBase::MergeProductInfoData(
    ProductInfo* info,
    const base::Value::Dict& on_page_data_map) {
  ShoppingService::MergeProductInfoData(info, on_page_data_map);
}

int ShoppingServiceTestBase::GetProductInfoCacheOpenURLCount(const GURL& url) {
  auto it = shopping_service_->product_info_cache_.find(url.spec());
  if (it == shopping_service_->product_info_cache_.end())
    return 0;

  return it->second->pages_with_url_open;
}

const ProductInfo* ShoppingServiceTestBase::GetFromProductInfoCache(
    const GURL& url) {
  return shopping_service_->GetFromProductInfoCache(url);
}

}  // namespace commerce
