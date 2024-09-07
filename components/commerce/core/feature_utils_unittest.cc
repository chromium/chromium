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
#include "components/commerce/core/test_utils.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/data_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace commerce {
namespace {

class FeatureUtilsTest : public testing::Test {
 public:
  FeatureUtilsTest()
      : prefs_(std::make_unique<TestingPrefServiceSimple>()),
        account_checker_(std::make_unique<MockAccountChecker>()),
        specifications_service_(
            std::make_unique<MockProductSpecificationsService>()) {
    RegisterCommercePrefs(prefs_->registry());
    account_checker_->SetPrefs(prefs_.get());
  }
  ~FeatureUtilsTest() override = default;

 protected:
  // Set up so that all product specs checks pass by default.
  void SetupProductSpecificationsEnabled() {
    ON_CALL(*account_checker_, IsSyncTypeEnabled)
        .WillByDefault(testing::Return(true));
    account_checker_->SetAnonymizedUrlDataCollectionEnabled(true);
    account_checker_->SetSignedIn(true);
    account_checker_->SetIsSubjectToParentalControls(false);
    account_checker_->SetCanUseModelExecutionFeatures(true);

    // 0 is the enabled enterprise state for the feature.
    SetTabCompareEnterprisePolicyPref(prefs_.get(), 0);

    // Default to having no sets.
    ON_CALL(*specifications_service_, GetAllProductSpecifications())
        .WillByDefault(
            testing::Return(std::vector<ProductSpecificationsSet>()));

    // TODO(356845106): Integrate country and locale checks.
  }

  base::test::ScopedFeatureList test_features_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
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
  SetupProductSpecificationsEnabled();

  std::vector<ProductSpecificationsSet> sets;
  sets.push_back(ProductSpecificationsSet(
      "10000000-0000-0000-0000-000000000000", 0, 0, std::vector<GURL>{}, ""));
  ON_CALL(*specifications_service_, GetAllProductSpecifications())
      .WillByDefault(testing::Return(sets));

  ASSERT_FALSE(CanManageProductSpecificationsSets(
      account_checker_.get(), specifications_service_.get()));
}

TEST_F(FeatureUtilsTest,
       CanManageProductSpecificationsSets_HasSets_HasFlag_NoFetch) {
  test_features_.InitAndEnableFeature(kProductSpecifications);
  SetupProductSpecificationsEnabled();

  std::vector<ProductSpecificationsSet> sets;
  sets.push_back(ProductSpecificationsSet(
      "10000000-0000-0000-0000-000000000000", 0, 0, std::vector<GURL>{}, ""));
  ON_CALL(*specifications_service_, GetAllProductSpecifications())
      .WillByDefault(testing::Return(sets));

  // Before turning off MSBB, we should be allowed to fetch.
  ASSERT_TRUE(CanFetchProductSpecificationsData(account_checker_.get()));

  // Turning off MSBB should block fetches for new data from happening.
  account_checker_->SetAnonymizedUrlDataCollectionEnabled(false);

  ASSERT_FALSE(CanFetchProductSpecificationsData(account_checker_.get()));
  ASSERT_TRUE(CanManageProductSpecificationsSets(
      account_checker_.get(), specifications_service_.get()));
}

TEST_F(FeatureUtilsTest, CanManageProductSpecificationsSets_NoSets_HasFlag) {
  test_features_.InitAndEnableFeature(kProductSpecifications);
  SetupProductSpecificationsEnabled();

  ASSERT_EQ(0u, specifications_service_->GetAllProductSpecifications().size());
  ASSERT_TRUE(CanManageProductSpecificationsSets(
      account_checker_.get(), specifications_service_.get()));
}

TEST_F(FeatureUtilsTest, CanManageProductSpecificationsSets_NoSets_NoFlag) {
  SetupProductSpecificationsEnabled();

  ASSERT_EQ(0u, specifications_service_->GetAllProductSpecifications().size());
  ASSERT_FALSE(CanManageProductSpecificationsSets(
      account_checker_.get(), specifications_service_.get()));
}

TEST_F(FeatureUtilsTest, CanFetchProductSpecificationsData_AllRequirements) {
  test_features_.InitAndEnableFeature(kProductSpecifications);
  SetupProductSpecificationsEnabled();

  ASSERT_TRUE(CanFetchProductSpecificationsData(account_checker_.get()));
}

TEST_F(FeatureUtilsTest, CanFetchProductSpecificationsData_NoMSBB) {
  test_features_.InitAndEnableFeature(kProductSpecifications);
  SetupProductSpecificationsEnabled();

  // We should be able to fetch data before turning off msbb.
  ASSERT_TRUE(CanFetchProductSpecificationsData(account_checker_.get()));

  account_checker_->SetAnonymizedUrlDataCollectionEnabled(false);

  ASSERT_FALSE(CanFetchProductSpecificationsData(account_checker_.get()));
}

TEST_F(FeatureUtilsTest, CanFetchProductSpecificationsData_NoSync) {
  test_features_.InitAndEnableFeature(kProductSpecifications);
  SetupProductSpecificationsEnabled();

  // We should be able to fetch data before turning off sync.
  ASSERT_TRUE(CanFetchProductSpecificationsData(account_checker_.get()));

  ON_CALL(*account_checker_, IsSyncTypeEnabled)
      .WillByDefault(testing::Return(false));

  ASSERT_FALSE(CanFetchProductSpecificationsData(account_checker_.get()));
}

TEST_F(FeatureUtilsTest, CanFetchProductSpecificationsData_NoEnterprise) {
  test_features_.InitAndEnableFeature(kProductSpecifications);
  SetupProductSpecificationsEnabled();

  // We should be able to fetch data before turning off enterprise.
  ASSERT_TRUE(CanFetchProductSpecificationsData(account_checker_.get()));

  // 1 is enabled but without logging.
  SetTabCompareEnterprisePolicyPref(prefs_.get(), 1);

  ASSERT_TRUE(CanFetchProductSpecificationsData(account_checker_.get()));

  // 2 is the disabled enterprise state for the feature.
  SetTabCompareEnterprisePolicyPref(prefs_.get(), 2);

  ASSERT_FALSE(CanFetchProductSpecificationsData(account_checker_.get()));
}

TEST_F(FeatureUtilsTest,
       CanFetchProductSpecificationsData_EnterpriseQualityLogging) {
  test_features_.InitAndEnableFeature(kProductSpecifications);
  SetupProductSpecificationsEnabled();

  // We should be able to fetch data before turning off enterprise.
  ASSERT_TRUE(IsProductSpecificationsQualityLoggingAllowed(prefs_.get()));

  // 1 is enabled but without logging.
  SetTabCompareEnterprisePolicyPref(prefs_.get(), 1);

  ASSERT_FALSE(IsProductSpecificationsQualityLoggingAllowed(prefs_.get()));

  // 2 is the disabled enterprise state for the feature.
  SetTabCompareEnterprisePolicyPref(prefs_.get(), 2);

  ASSERT_FALSE(IsProductSpecificationsQualityLoggingAllowed(prefs_.get()));
}

TEST_F(FeatureUtilsTest,
       IsProductSpecificationsAllowedForEnterprise_EnterpriseNotSet) {
  // Without any other setup, enterprise should be allowed.
  ASSERT_TRUE(IsProductSpecificationsAllowedForEnterprise(prefs_.get()));

  // 2 is the disabled enterprise state for the feature.
  SetTabCompareEnterprisePolicyPref(prefs_.get(), 2);

  ASSERT_FALSE(IsProductSpecificationsAllowedForEnterprise(prefs_.get()));
}

TEST_F(FeatureUtilsTest,
       IsProductSpecificationsAllowedForEnterprise_EnterpriseNotManaged) {
  TestingPrefServiceSimple prefs;

  // If the preference is local, we shouldn't be respecting it.
  RegisterCommercePrefs(prefs.registry());
  prefs.SetInteger(
      optimization_guide::prefs::kProductSpecificationsEnterprisePolicyAllowed,
      2);

  ASSERT_TRUE(IsProductSpecificationsAllowedForEnterprise(&prefs));
}

TEST_F(FeatureUtilsTest,
       CanFetchProductSpecificationsData_BlockedByParentalControls) {
  test_features_.InitAndEnableFeature(kProductSpecifications);
  SetupProductSpecificationsEnabled();

  // We should be able to fetch data before turning off enterprise.
  ASSERT_TRUE(CanFetchProductSpecificationsData(account_checker_.get()));

  account_checker_->SetIsSubjectToParentalControls(true);

  ASSERT_FALSE(CanFetchProductSpecificationsData(account_checker_.get()));
}

TEST_F(FeatureUtilsTest,
       CanFetchProductSpecificationsData_ModelExecutionNotAllowed) {
  test_features_.InitAndEnableFeature(kProductSpecifications);
  SetupProductSpecificationsEnabled();

  // We should be able to fetch data before turning off enterprise.
  ASSERT_TRUE(CanFetchProductSpecificationsData(account_checker_.get()));

  account_checker_->SetCanUseModelExecutionFeatures(false);

  ASSERT_FALSE(CanFetchProductSpecificationsData(account_checker_.get()));
}

}  // namespace
}  // namespace commerce
