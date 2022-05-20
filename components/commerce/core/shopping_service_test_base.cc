// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_service_test_base.h"

#include "base/notreached.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/testing_pref_service.h"

using optimization_guide::OptimizationGuideDecision;
using optimization_guide::OptimizationGuideDecisionCallback;
using optimization_guide::OptimizationMetadata;
using optimization_guide::proto::Any;
using optimization_guide::proto::OptimizationType;

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
    std::move(callback).Run(OptimizationGuideDecision::kUnknown,
                            OptimizationMetadata());
    return;
  }

  std::move(callback).Run(optimization_decision_.value(),
                          optimization_data_.value());
}

OptimizationGuideDecision MockOptGuideDecider::CanApplyOptimization(
    const GURL& url,
    OptimizationType optimization_type,
    OptimizationMetadata* optimization_metadata) {
  // We don't use the synchronous API in the shopping service.
  NOTREACHED();

  return OptimizationGuideDecision::kUnknown;
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
    const std::string& country_code) {
  OptimizationMetadata meta;

  PriceTrackingData price_tracking_data;
  BuyableProduct* buyable_product =
      price_tracking_data.mutable_buyable_product();
  buyable_product->set_title(title);
  buyable_product->set_image_url(image_url);
  buyable_product->set_offer_id(offer_id);
  buyable_product->set_product_cluster_id(product_cluster_id);
  buyable_product->set_country_code(country_code);

  Any any;
  any.set_type_url(price_tracking_data.GetTypeName());
  price_tracking_data.SerializeToString(any.mutable_value());
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

bool MockWebWrapper::IsOffTheRecord() {
  return is_off_the_record_;
}

void MockWebWrapper::RunJavascript(
    const std::u16string& script,
    base::OnceCallback<void(const base::Value)> callback) {
  if (!mock_js_result_) {
    std::move(callback).Run(base::Value());
    return;
  }

  std::move(callback).Run(mock_js_result_->Clone());
}

void MockWebWrapper::SetMockJavaScriptResult(base::Value* result) {
  mock_js_result_ = result;
}

ShoppingServiceTestBase::ShoppingServiceTestBase()
    : bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()),
      opt_guide_(std::make_unique<MockOptGuideDecider>()),
      pref_service_(std::make_unique<TestingPrefServiceSimple>()) {
  shopping_service_ = std::make_unique<ShoppingService>(
      bookmark_model_.get(), opt_guide_.get(), pref_service_.get());
}

ShoppingServiceTestBase::~ShoppingServiceTestBase() = default;

void ShoppingServiceTestBase::TestBody() {}

}  // namespace commerce
