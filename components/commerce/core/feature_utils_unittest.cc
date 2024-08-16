// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/feature_utils.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/product_specifications/mock_product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/sync/base/data_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace commerce {
namespace {

class FeatureUtilsTest : public testing::Test {
 public:
  FeatureUtilsTest()
      : account_checker_(std::make_unique<MockAccountChecker>()),
        specifications_service_(
            std::make_unique<MockProductSpecificationsService>()) {}
  ~FeatureUtilsTest() override = default;

 protected:
  base::test::ScopedFeatureList test_features_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockAccountChecker> account_checker_;
  std::unique_ptr<MockProductSpecificationsService> specifications_service_;
};

TEST_F(FeatureUtilsTest, CanLoadProductSpecificationsFullPageUi_HasFlag) {
  test_features_.InitAndEnableFeature(kProductSpecifications);

  ASSERT_TRUE(CanLoadProductSpecificationsFullPageUi(account_checker_.get()));
}

TEST_F(FeatureUtilsTest, CanLoadProductSpecificationsFullPageUi_NoFlag) {
  ASSERT_FALSE(CanLoadProductSpecificationsFullPageUi(account_checker_.get()));
}

TEST_F(FeatureUtilsTest, CanManageProductSpecificationsSets_NullParams) {
  ASSERT_FALSE(CanManageProductSpecificationsSets(nullptr, nullptr));
}

TEST_F(FeatureUtilsTest, CanManageProductSpecificationsSets_HasSets_NoFlag) {
  std::vector<ProductSpecificationsSet> sets;
  sets.push_back(ProductSpecificationsSet(
      "10000000-0000-0000-0000-000000000000", 0, 0, std::vector<GURL>{}, ""));
  ON_CALL(*specifications_service_, GetAllProductSpecifications())
      .WillByDefault(testing::Return(sets));

  ON_CALL(*account_checker_, IsSyncTypeEnabled)
      .WillByDefault(testing::Return(true));

  ASSERT_FALSE(CanManageProductSpecificationsSets(
      account_checker_.get(), specifications_service_.get()));
}

TEST_F(FeatureUtilsTest,
       CanManageProductSpecificationsSets_HasSets_HasFlag_NoFetch) {
  test_features_.InitAndEnableFeature(kProductSpecifications);
  std::vector<ProductSpecificationsSet> sets;
  sets.push_back(ProductSpecificationsSet(
      "10000000-0000-0000-0000-000000000000", 0, 0, std::vector<GURL>{}, ""));
  ON_CALL(*specifications_service_, GetAllProductSpecifications())
      .WillByDefault(testing::Return(sets));

  ON_CALL(*account_checker_, IsSyncTypeEnabled)
      .WillByDefault(testing::Return(true));

  // Turning off MSBB should block fetches for new data from happening.
  account_checker_->SetAnonymizedUrlDataCollectionEnabled(false);

  ASSERT_FALSE(CanFetchProductSpecificationsData(account_checker_.get()));
  ASSERT_TRUE(CanManageProductSpecificationsSets(
      account_checker_.get(), specifications_service_.get()));
}

TEST_F(FeatureUtilsTest, CanManageProductSpecificationsSets_NoSets_HasFlag) {
  test_features_.InitAndEnableFeature(kProductSpecifications);
  std::vector<ProductSpecificationsSet> sets;
  ON_CALL(*specifications_service_, GetAllProductSpecifications())
      .WillByDefault(testing::Return(sets));

  ON_CALL(*account_checker_, IsSyncTypeEnabled)
      .WillByDefault(testing::Return(true));

  ASSERT_TRUE(CanManageProductSpecificationsSets(
      account_checker_.get(), specifications_service_.get()));
}

TEST_F(FeatureUtilsTest, CanManageProductSpecificationsSets_NoSets_NoFlag) {
  std::vector<ProductSpecificationsSet> sets;
  ON_CALL(*specifications_service_, GetAllProductSpecifications())
      .WillByDefault(testing::Return(sets));

  ON_CALL(*account_checker_, IsSyncTypeEnabled)
      .WillByDefault(testing::Return(true));

  ASSERT_FALSE(CanManageProductSpecificationsSets(
      account_checker_.get(), specifications_service_.get()));
}

TEST_F(FeatureUtilsTest, CanFetchProductSpecificationsData_AllRequirements) {
  test_features_.InitAndEnableFeature(kProductSpecifications);
  std::vector<ProductSpecificationsSet> sets;
  ON_CALL(*specifications_service_, GetAllProductSpecifications())
      .WillByDefault(testing::Return(sets));

  // TODO(356845106): Integrate country and locale checks.
  account_checker_->SetAnonymizedUrlDataCollectionEnabled(true);
  ON_CALL(*account_checker_, IsSyncTypeEnabled)
      .WillByDefault(testing::Return(true));

  ASSERT_TRUE(CanFetchProductSpecificationsData(account_checker_.get()));
}

TEST_F(FeatureUtilsTest, CanFetchProductSpecificationsData_NoMSBB) {
  test_features_.InitAndEnableFeature(kProductSpecifications);
  std::vector<ProductSpecificationsSet> sets;
  ON_CALL(*specifications_service_, GetAllProductSpecifications())
      .WillByDefault(testing::Return(sets));

  // TODO(356845106): Integrate country and locale checks.
  account_checker_->SetAnonymizedUrlDataCollectionEnabled(false);
  ON_CALL(*account_checker_, IsSyncTypeEnabled)
      .WillByDefault(testing::Return(true));

  ASSERT_FALSE(CanFetchProductSpecificationsData(account_checker_.get()));
}

TEST_F(FeatureUtilsTest, CanFetchProductSpecificationsData_NoSync) {
  test_features_.InitAndEnableFeature(kProductSpecifications);
  std::vector<ProductSpecificationsSet> sets;
  ON_CALL(*specifications_service_, GetAllProductSpecifications())
      .WillByDefault(testing::Return(sets));

  // TODO(356845106): Integrate country and locale checks.
  account_checker_->SetAnonymizedUrlDataCollectionEnabled(true);
  ON_CALL(*account_checker_, IsSyncTypeEnabled)
      .WillByDefault(testing::Return(false));

  ASSERT_FALSE(CanFetchProductSpecificationsData(account_checker_.get()));
}

}  // namespace
}  // namespace commerce
