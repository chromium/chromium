// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic_configs/launched_configs.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/fake_common_dependencies.h"
#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/starter_heuristic_configs/launched_starter_heuristic_config.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace launched_configs {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

class LaunchedConfigsTest : public testing::Test {
 public:
  LaunchedConfigsTest() = default;
  ~LaunchedConfigsTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  FakeStarterPlatformDelegate fake_platform_delegate_ =
      FakeStarterPlatformDelegate(
          std::make_unique<FakeCommonDependencies>(nullptr));
};

TEST_F(LaunchedConfigsTest, ShoppingAndCouponsLaunchedForCct) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillAssistantInCCTTriggering);

  fake_platform_delegate_.is_custom_tab_ = true;
  fake_platform_delegate_.is_web_layer_ = false;
  fake_platform_delegate_.is_tab_created_by_gsa_ = true;
  fake_platform_delegate_.is_logged_in_ = false;
  fake_platform_delegate_.fake_common_dependencies_->msbb_enabled_ = true;

  EXPECT_THAT(GetOrCreateShoppingConfig()->GetDenylistedDomains(), SizeIs(29));
  EXPECT_THAT(GetOrCreateCouponsConfig()->GetDenylistedDomains(), SizeIs(29));

  EXPECT_THAT(GetOrCreateShoppingConfig()->GetIntent(),
              Eq("SHOPPING_ASSISTED_CHECKOUT"));
  EXPECT_THAT(GetOrCreateCouponsConfig()->GetIntent(), Eq("FIND_COUPONS"));

  fake_platform_delegate_.fake_common_dependencies_->country_code_ = "us";
  EXPECT_THAT(GetOrCreateShoppingConfig()->GetConditionSetsForClientState(
                  &fake_platform_delegate_, &context_),
              SizeIs(2));
  EXPECT_THAT(GetOrCreateCouponsConfig()->GetConditionSetsForClientState(
                  &fake_platform_delegate_, &context_),
              SizeIs(2));

  fake_platform_delegate_.fake_common_dependencies_->country_code_ = "gb";
  EXPECT_THAT(GetOrCreateShoppingConfig()->GetConditionSetsForClientState(
                  &fake_platform_delegate_, &context_),
              SizeIs(2));
  EXPECT_THAT(GetOrCreateCouponsConfig()->GetConditionSetsForClientState(
                  &fake_platform_delegate_, &context_),
              IsEmpty());

  fake_platform_delegate_.fake_common_dependencies_->country_code_ = "ch";
  EXPECT_THAT(GetOrCreateShoppingConfig()->GetConditionSetsForClientState(
                  &fake_platform_delegate_, &context_),
              IsEmpty());
  EXPECT_THAT(GetOrCreateCouponsConfig()->GetConditionSetsForClientState(
                  &fake_platform_delegate_, &context_),
              IsEmpty());
}

TEST_F(LaunchedConfigsTest, ShoppingAndCouponsCanBeDisabledWithFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillAssistantInCCTTriggering);

  fake_platform_delegate_.is_custom_tab_ = true;
  fake_platform_delegate_.is_web_layer_ = false;
  fake_platform_delegate_.is_tab_created_by_gsa_ = true;
  fake_platform_delegate_.is_logged_in_ = false;
  fake_platform_delegate_.fake_common_dependencies_->msbb_enabled_ = true;

  EXPECT_THAT(GetOrCreateShoppingConfig()->GetConditionSetsForClientState(
                  &fake_platform_delegate_, &context_),
              IsEmpty());
  EXPECT_THAT(GetOrCreateCouponsConfig()->GetConditionSetsForClientState(
                  &fake_platform_delegate_, &context_),
              IsEmpty());
}

}  // namespace
}  // namespace launched_configs
}  // namespace autofill_assistant
