// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/product_specifications_page_action_controller.h"

#include <memory>
#include <optional>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "chrome/test/base/testing_profile.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {

namespace {
const char kTestUrl[] = "https://www.example.com";
const int64_t kClusterId = 12345L;

// Build a basic ProductInfo object.
std::optional<ProductInfo> CreateProductInfo(uint64_t cluster_id) {
  std::optional<ProductInfo> info;
  info.emplace();
  info->product_cluster_id = cluster_id;
  return info;
}

std::optional<ProductGroup> CreateProductGroup() {
  return ProductGroup(base::Uuid::GenerateRandomV4(), "test", {GURL()},
                      base::Time());
}
}  // namespace

class ProductSpecificationsPageActionControllerUnittest : public testing::Test {
 public:
  ProductSpecificationsPageActionControllerUnittest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(commerce::kProductSpecifications);
    shopping_service_ = std::make_unique<MockShoppingService>();
    base::RepeatingCallback<void()> callback = notify_host_callback_.Get();
    account_checker_ = std::make_unique<MockAccountChecker>();
    shopping_service_->SetAccountChecker(account_checker_.get());
    controller_ = std::make_unique<ProductSpecificationsPageActionController>(
        std::move(callback), shopping_service_.get());
  }

  ProductSpecificationsPageActionControllerUnittest(
      const ProductSpecificationsPageActionControllerUnittest&) = delete;
  ProductSpecificationsPageActionControllerUnittest operator=(
      const ProductSpecificationsPageActionControllerUnittest&) = delete;
  ~ProductSpecificationsPageActionControllerUnittest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockAccountChecker> account_checker_;
  base::MockRepeatingCallback<void()> notify_host_callback_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<ProductSpecificationsPageActionController> controller_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ProductSpecificationsPageActionControllerUnittest, IconShow) {
  EXPECT_CALL(*shopping_service_, GetProductGroupForCandidateProduct).Times(1);
  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl).Times(1);
  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->WantsExpandedUi());

  shopping_service_->SetResponseForGetProductGroupForCandidateProduct(
      CreateProductGroup());
  shopping_service_->SetResponseForGetProductInfoForUrl(
      CreateProductInfo(kClusterId));

  controller_->ResetForNewNavigation(GURL(kTestUrl));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_TRUE(controller_->ShouldShowForNavigation().value());
  ASSERT_TRUE(controller_->WantsExpandedUi());
}

TEST_F(ProductSpecificationsPageActionControllerUnittest,
       IconNotwShow_NoProductGroup) {
  EXPECT_CALL(*shopping_service_, GetProductGroupForCandidateProduct).Times(1);
  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl).Times(1);
  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->WantsExpandedUi());

  // Mock no product group for current URL.
  shopping_service_->SetResponseForGetProductGroupForCandidateProduct(
      std::nullopt);
  shopping_service_->SetResponseForGetProductInfoForUrl(
      CreateProductInfo(kClusterId));

  controller_->ResetForNewNavigation(GURL(kTestUrl));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->WantsExpandedUi());
}

TEST_F(ProductSpecificationsPageActionControllerUnittest,
       IconNotwShow_NoProductInfo) {
  EXPECT_CALL(*shopping_service_, GetProductGroupForCandidateProduct).Times(0);
  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl).Times(1);
  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(1));

  // Before a navigation, the controller should be in an "undecided" state.
  ASSERT_FALSE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->WantsExpandedUi());

  // Mock no product info for current URL.
  shopping_service_->SetResponseForGetProductInfoForUrl(std::nullopt);

  controller_->ResetForNewNavigation(GURL(kTestUrl));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->WantsExpandedUi());
}

TEST_F(ProductSpecificationsPageActionControllerUnittest,
       IconNotwShow_FeatureIneligible) {
  // Mock Ineligible for the feature.
  account_checker_->SetSignedIn(false);

  EXPECT_CALL(*shopping_service_, GetProductGroupForCandidateProduct).Times(0);
  EXPECT_CALL(*shopping_service_, GetProductInfoForUrl).Times(0);
  EXPECT_CALL(notify_host_callback_, Run()).Times(testing::AtLeast(0));

  // Before a navigation, the controller already rejects.
  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->WantsExpandedUi());

  controller_->ResetForNewNavigation(GURL(kTestUrl));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(controller_->ShouldShowForNavigation().has_value());
  ASSERT_FALSE(controller_->ShouldShowForNavigation().value());
  ASSERT_FALSE(controller_->WantsExpandedUi());
}

}  // namespace commerce
