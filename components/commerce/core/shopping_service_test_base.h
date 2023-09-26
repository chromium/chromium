// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_TEST_BASE_H_
#define COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_TEST_BASE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/web_wrapper.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

using optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback;
using optimization_guide::OptimizationGuideDecision;
using optimization_guide::OptimizationGuideDecisionCallback;
using optimization_guide::OptimizationMetadata;
using optimization_guide::proto::OptimizationType;
using optimization_guide::proto::RequestContext;

class TestingPrefServiceSimple;

namespace bookmarks {
class BookmarkModel;
}

namespace network {
class TestURLLoaderFactory;
}

namespace signin {
class IdentityTestEnvironment;
}

namespace syncer {
class TestSyncService;
}

namespace commerce {

extern const uint64_t kInvalidDiscountId;

// A mock Optimization Guide decider that allows us to specify the response for
// a particular URL.
class MockOptGuideDecider
    : public optimization_guide::OptimizationGuideDecider {
 public:
  MockOptGuideDecider();
  MockOptGuideDecider(const MockOptGuideDecider&) = delete;
  MockOptGuideDecider operator=(const MockOptGuideDecider&) = delete;
  ~MockOptGuideDecider() override;

  void RegisterOptimizationTypes(
      const std::vector<OptimizationType>& optimization_types) override;

  void CanApplyOptimization(
      const GURL& url,
      OptimizationType optimization_type,
      OptimizationGuideDecisionCallback callback) override;

  OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      OptimizationType optimization_type,
      OptimizationMetadata* optimization_metadata) override;

  void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const base::flat_set<OptimizationType>& optimization_types,
      RequestContext request_context,
      OnDemandOptimizationGuideDecisionRepeatingCallback callback) override;

  void AddOnDemandShoppingResponse(const GURL& url,
                                   const OptimizationGuideDecision decision,
                                   const OptimizationMetadata& data);

  void SetResponse(const GURL& url,
                   const OptimizationType type,
                   const OptimizationGuideDecision decision,
                   const OptimizationMetadata& data);

  OptimizationMetadata BuildPriceTrackingResponse(
      const std::string& title,
      const std::string& image_url,
      const uint64_t offer_id,
      const uint64_t product_cluster_id,
      const std::string& country_code,
      const int64_t amount_micros = 0,
      const std::string& currency_code = "USD",
      const std::string& gpc_title = "example_gpc_title");

  void AddPriceUpdateToPriceTrackingResponse(OptimizationMetadata* out_meta,
                                             const std::string& currency_code,
                                             const int64_t current_price,
                                             const int64_t previous_price);

  OptimizationMetadata BuildMerchantTrustResponse(
      const float star_rating,
      const uint32_t count_rating,
      const std::string& details_page_url,
      const bool has_return_policy,
      const bool contains_sensitive_content);

  OptimizationMetadata BuildPriceInsightsResponse(
      const uint64_t product_cluster_id,
      const std::string& price_range_currency_code,
      const int64_t low_typical_price_micros,
      const int64_t high_typical_price_micros,
      const std::string& price_history_currency_code,
      const std::string& attributes,
      const std::vector<std::tuple<std::string, int64_t>>& history_prices,
      const std::string& jackpot_url,
      const PriceBucket& price_bucket,
      const bool has_multiple_catalogs);

  OptimizationMetadata BuildDiscountsResponse(
      const std::vector<DiscountInfo>& infos);

 private:
  absl::optional<GURL> response_url_;
  absl::optional<OptimizationType> optimization_type_;
  absl::optional<OptimizationGuideDecision> optimization_decision_;
  absl::optional<OptimizationMetadata> optimization_data_;

  // Shopping responses for the on-demand API.
  std::unordered_map<std::string,
                     optimization_guide::OptimizationGuideDecisionWithMetadata>
      on_demand_shopping_responses_;
};

// A mock WebWrapper where returned values can be manually set.
class MockWebWrapper : public WebWrapper {
 public:
  MockWebWrapper(const GURL& last_committed_url, bool is_off_the_record);

  // `result` specified the result of the subsequent javascript execution. This
  // object does not take ownership of the provided pointer.
  MockWebWrapper(const GURL& last_committed_url,
                 bool is_off_the_record,
                 base::Value* result);

  MockWebWrapper(const MockWebWrapper&) = delete;
  MockWebWrapper operator=(const MockWebWrapper&) = delete;

  ~MockWebWrapper() override;

  const GURL& GetLastCommittedURL() override;

  bool IsFirstLoadForNavigationFinished() override;
  void SetIsFirstLoadForNavigationFinished(bool finished);

  bool IsOffTheRecord() override;

  void RunJavascript(
      const std::u16string& script,
      base::OnceCallback<void(const base::Value)> callback) override;

 private:
  const GURL last_committed_url_;
  const bool is_off_the_record_;
  bool is_first_load_finished_{true};
  const raw_ptr<base::Value> mock_js_result_;
};

class ShoppingServiceTestBase : public testing::Test {
 public:
  ShoppingServiceTestBase();
  ShoppingServiceTestBase(const ShoppingServiceTestBase&) = delete;
  ShoppingServiceTestBase operator=(const ShoppingServiceTestBase&) = delete;
  ~ShoppingServiceTestBase() override;

  void SetUp() override;

  void TestBody() override;

  void TearDown() override;

  // A direct proxies to the same methods in the ShoppingService class.
  void DidNavigatePrimaryMainFrame(WebWrapper* web);
  void DidFinishLoad(WebWrapper* web);
  void DidNavigateAway(WebWrapper* web, const GURL& url);
  void WebWrapperDestroyed(WebWrapper* web);
  static void MergeProductInfoData(ProductInfo* info,
                                   const base::Value::Dict& on_page_data_map);

  // Skip the delay for running the on-page javascript for product info and
  // wait until the task completes.
  void SimulateProductInfoJsTaskFinished();

  // Get the count of the number of tabs a particular URL is open in from the
  // product info cache.
  int GetProductInfoCacheOpenURLCount(const GURL& url);

  // Get the item in the product info cache if it exists.
  const ProductInfo* GetFromProductInfoCache(const GURL& url);

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Used primarily for decoding JSON for the mock javascript execution.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;

  std::unique_ptr<MockOptGuideDecider> opt_guide_;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  base::test::ScopedFeatureList test_features_;

  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;

  std::unique_ptr<syncer::TestSyncService> sync_service_;

  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;

  std::unique_ptr<ShoppingService> shopping_service_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_TEST_BASE_H_
