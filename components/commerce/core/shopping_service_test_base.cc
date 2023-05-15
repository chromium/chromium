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
#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
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
    const std::string& currency_code) {
  OptimizationMetadata meta;

  PriceTrackingData price_tracking_data;
  BuyableProduct* buyable_product =
      price_tracking_data.mutable_buyable_product();

  if (!title.empty())
    buyable_product->set_title(title);

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

MockWebWrapper::MockWebWrapper(const GURL& last_committed_url,
                               bool is_off_the_record)
    : last_committed_url_(last_committed_url),
      is_off_the_record_(is_off_the_record) {}

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

void MockWebWrapper::SetMockJavaScriptResult(base::Value* result) {
  mock_js_result_ = result;
}

ShoppingServiceTestBase::ShoppingServiceTestBase()
    : bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()),
      opt_guide_(std::make_unique<MockOptGuideDecider>()),
      pref_service_(std::make_unique<TestingPrefServiceSimple>()),
      identity_test_env_(std::make_unique<signin::IdentityTestEnvironment>()),
      sync_service_(std::make_unique<syncer::TestSyncService>()),
      test_url_loader_factory_(
          std::make_unique<network::TestURLLoaderFactory>()) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  RegisterPrefs(pref_service_->registry());
  shopping_service_ = std::make_unique<ShoppingService>(
      "us", "en-us", bookmark_model_.get(), opt_guide_.get(),
      pref_service_.get(), identity_test_env_->identity_manager(),
      sync_service_.get(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          test_url_loader_factory_.get()),
      nullptr, nullptr);
}

ShoppingServiceTestBase::~ShoppingServiceTestBase() = default;

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
