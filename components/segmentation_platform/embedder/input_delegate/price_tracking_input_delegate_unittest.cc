// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/input_delegate/price_tracking_input_delegate.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/shopping_service_test_base.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/public/input_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::processing {

namespace {

commerce::ShoppingService* TestShoppingServiceGetter(
    commerce::ShoppingService* service) {
  return service;
}

bookmarks::BookmarkModel* TestBookmarkModelGetter(
    bookmarks::BookmarkModel* model) {
  return model;
}

}  // namespace

class PriceTrackingInputDelegateTest : public testing::Test {
 public:
  PriceTrackingInputDelegateTest() {
    scoped_feature_list_.InitAndEnableFeature(commerce::kShoppingList);
  }
  ~PriceTrackingInputDelegateTest() override = default;

  void SetUp() override {
    Test::SetUp();

    shopping_service_ = std::make_unique<commerce::MockShoppingService>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    input_delegate_ = std::make_unique<PriceTrackingInputDelegate>(
        base::BindRepeating(&TestShoppingServiceGetter,
                            base::Unretained(shopping_service_.get())),
        base::BindRepeating(&TestBookmarkModelGetter,
                            base::Unretained(bookmark_model_.get())));
    feature_processor_state_ = std::make_unique<FeatureProcessorState>();
  }

  void ExpectProcessResult(const proto::CustomInput& input,
                           bool expected_error,
                           const Tensor& expected_tensor) {
    base::RunLoop wait_for_process;
    input_delegate_->Process(
        input, *feature_processor_state_,
        base::BindOnce(
            [](base::OnceClosure quit, bool expected_error,
               const Tensor& expected_tensor, bool error, Tensor tensor) {
              EXPECT_EQ(expected_error, error);
              EXPECT_EQ(expected_tensor, tensor);
              std::move(quit).Run();
            },
            wait_for_process.QuitClosure(), expected_error, expected_tensor));
    wait_for_process.Run();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<commerce::MockShoppingService> shopping_service_;
  std::unique_ptr<PriceTrackingInputDelegate> input_delegate_;
  std::unique_ptr<FeatureProcessorState> feature_processor_state_;
};

TEST_F(PriceTrackingInputDelegateTest, NoInputContext) {
  proto::CustomInput input_proto;
  input_proto.set_name("test");
  input_proto.set_fill_policy(proto::CustomInput::PRICE_TRACKING_HINTS);
  input_proto.set_tensor_length(1);

  ExpectProcessResult(input_proto, /*expected_error=*/true, {});
}

TEST_F(PriceTrackingInputDelegateTest, InputContextDoesntHaveUrl) {
  proto::CustomInput input_proto;
  input_proto.set_name("test");
  input_proto.set_fill_policy(proto::CustomInput::PRICE_TRACKING_HINTS);
  input_proto.set_tensor_length(1);

  auto input_context = base::MakeRefCounted<InputContext>();
  feature_processor_state_->set_input_context_for_testing(input_context);

  ExpectProcessResult(input_proto, /*expected_error=*/true, {});
}

TEST_F(PriceTrackingInputDelegateTest, NoPriceTracking) {
  const GURL kTestUrl("https://www.example.com/");
  proto::CustomInput input_proto;
  input_proto.set_name("test");
  input_proto.set_fill_policy(proto::CustomInput::PRICE_TRACKING_HINTS);
  input_proto.set_tensor_length(1);

  processing::ProcessedValue url_value(kTestUrl);
  auto input_context = base::MakeRefCounted<InputContext>();
  input_context->metadata_args.insert(
      std::make_pair("url", std::move(url_value)));
  feature_processor_state_->set_input_context_for_testing(input_context);

  shopping_service_->SetResponseForGetProductInfoForUrl(absl::nullopt);

  ExpectProcessResult(input_proto, /*expected_error=*/false,
                      {ProcessedValue(0.0f)});
}

TEST_F(PriceTrackingInputDelegateTest, PriceTracking) {
  const GURL kTestUrl("https://www.example.com/");
  proto::CustomInput input_proto;
  input_proto.set_name("test");
  input_proto.set_fill_policy(proto::CustomInput::PRICE_TRACKING_HINTS);
  input_proto.set_tensor_length(1);

  processing::ProcessedValue url_value(kTestUrl);
  auto input_context = base::MakeRefCounted<InputContext>();
  input_context->metadata_args.insert(
      std::make_pair("url", std::move(url_value)));
  feature_processor_state_->set_input_context_for_testing(input_context);

  shopping_service_->SetResponseForGetProductInfoForUrl(
      commerce::ProductInfo());

  ExpectProcessResult(input_proto, /*expected_error=*/false,
                      {ProcessedValue(1.0f)});
}

TEST_F(PriceTrackingInputDelegateTest,
       PriceTracking_WhenUrlBookmarkedButNotTracked) {
  const GURL kTestUrl("https://www.example.com/");
  proto::CustomInput input_proto;
  input_proto.set_name("test");
  input_proto.set_fill_policy(proto::CustomInput::PRICE_TRACKING_HINTS);
  input_proto.set_tensor_length(1);

  const bookmarks::BookmarkNode* bb_node = bookmark_model_->bookmark_bar_node();
  bookmark_model_->AddNewURL(bb_node, 0, u"shopping site", kTestUrl);

  processing::ProcessedValue url_value(kTestUrl);
  auto input_context = base::MakeRefCounted<InputContext>();
  input_context->metadata_args.insert(
      std::make_pair("url", std::move(url_value)));
  feature_processor_state_->set_input_context_for_testing(input_context);

  shopping_service_->SetResponseForGetProductInfoForUrl(
      commerce::ProductInfo());

  ExpectProcessResult(input_proto, /*expected_error=*/false,
                      {ProcessedValue(1.0f)});
}

TEST_F(PriceTrackingInputDelegateTest, NoPriceTracking_WhenUrlTracked) {
  const GURL kTestUrl("https://www.example.com/");
  proto::CustomInput input_proto;
  input_proto.set_name("test");
  input_proto.set_fill_policy(proto::CustomInput::PRICE_TRACKING_HINTS);
  input_proto.set_tensor_length(1);

  const bookmarks::BookmarkNode* bb_node = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* test_url_node =
      bookmark_model_->AddNewURL(bb_node, 0, u"shopping site", kTestUrl);

  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> price_tracking_meta =
      std::make_unique<power_bookmarks::PowerBookmarkMeta>();
  price_tracking_meta->mutable_shopping_specifics()->set_is_price_tracked(true);
  power_bookmarks::SetNodePowerBookmarkMeta(
      bookmark_model_.get(), test_url_node, std::move(price_tracking_meta));

  processing::ProcessedValue url_value(kTestUrl);
  auto input_context = base::MakeRefCounted<InputContext>();
  input_context->metadata_args.insert(
      std::make_pair("url", std::move(url_value)));
  feature_processor_state_->set_input_context_for_testing(input_context);

  shopping_service_->SetResponseForGetProductInfoForUrl(
      commerce::ProductInfo());

  ExpectProcessResult(input_proto, /*expected_error=*/false,
                      {ProcessedValue(0.0f)});
}

}  // namespace segmentation_platform::processing
