// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"

#include <gmock/gmock.h>

#include <string>

#include "base/test/scoped_feature_list.h"
#include "components/fingerprinting_protection_filter/browser/test_support.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "content/public/test/test_renderer_host.h"
#include "fingerprinting_protection_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fingerprinting_protection_filter {
namespace {

// TODO(https://crbug.com/366515692): Add unit tests for other functions, i.e.
// GetThrottleManager, DidFinishNavigation, etc.

struct CreateForWebContentsTestCase {
  std::string test_name;
  bool is_regular_feature_enabled = false;
  bool is_incognito_feature_enabled = false;
  bool is_incognito_profile = false;
  bool nullptr_expected;
};

class CreateForWebContentsHelperTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<CreateForWebContentsTestCase> {
 public:
  CreateForWebContentsHelperTest() = default;

  GURL GetTestUrl() { return GURL("http://cool.things.com"); }

  void SetUp() override { content::RenderViewHostTestHarness::SetUp(); }

  void TearDown() override {
    RenderViewHostTestHarness::TearDown();
    scoped_feature_list_.Reset();
  }

  void SetFeatureFlags(bool is_regular_feature_enabled,
                       bool is_incognito_feature_enabled) {
    if (is_regular_feature_enabled && is_incognito_feature_enabled) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {features::kEnableFingerprintingProtectionFilter,
           features::kEnableFingerprintingProtectionFilterInIncognito},
          /*disabled_features=*/{});
    } else if (!is_regular_feature_enabled && !is_incognito_feature_enabled) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              features::kEnableFingerprintingProtectionFilter,
              features::kEnableFingerprintingProtectionFilterInIncognito});
    } else if (is_regular_feature_enabled) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::
                                    kEnableFingerprintingProtectionFilter},
          /*disabled_features=*/{
              features::kEnableFingerprintingProtectionFilterInIncognito});
    } else if (is_incognito_feature_enabled) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {features::kEnableFingerprintingProtectionFilterInIncognito},
          /*disabled_features=*/{
              features::kEnableFingerprintingProtectionFilter});
    }
  }

  void ExpectNullptr(
      bool expect_nullptr,
      FingerprintingProtectionWebContentsHelper* web_contents_helper) {
    if (expect_nullptr) {
      EXPECT_EQ(nullptr, web_contents_helper);
    } else {
      EXPECT_NE(nullptr, web_contents_helper);
    }
  }

 protected:
  TestSupport test_support_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

const CreateForWebContentsTestCase kTestCases[] = {
    {
        .test_name = "Created_FeaturesOn_RegularProfile",
        .is_regular_feature_enabled = true,
        .is_incognito_feature_enabled = true,
        .nullptr_expected = false,
    },
    {
        .test_name = "Created_FeaturesOn_IncognitoProfile",
        .is_regular_feature_enabled = true,
        .is_incognito_feature_enabled = true,
        .is_incognito_profile = true,
        .nullptr_expected = false,
    },
    {
        .test_name = "Created_RegularFeatureEnabled_RegularProfile",
        .is_regular_feature_enabled = true,
        .nullptr_expected = false,
    },
    {
        .test_name = "Created_IncognitoFeatureEnabled_IncognitoProfile",
        .is_incognito_feature_enabled = true,
        .is_incognito_profile = true,
        .nullptr_expected = false,
    },
    {
        .test_name = "NotCreated_FeaturesOff",
        .nullptr_expected = true,
    },
    {
        .test_name = "NotCreated_RegularFeatureEnabled_IncognitoProfile",
        .is_regular_feature_enabled = true,
        .is_incognito_profile = true,
        .nullptr_expected = true,
    },
    {
        .test_name = "NotCreated_RegularFeatureDisabled_RegularProfile",
        .nullptr_expected = true,
    },
    {
        .test_name = "NotCreated_RegularFeatureDisabled_IncognitoProfile",
        .is_incognito_profile = true,
        .nullptr_expected = true,
    },
    {
        .test_name = "NotCreated_IncognitoFeatureEnabled_RegularProfile",
        .is_incognito_feature_enabled = true,
        .nullptr_expected = true,
    },
    {
        .test_name = "NotCreated_IncognitoFeatureDisabled_IncognitoProfile",
        .is_incognito_profile = true,
        .nullptr_expected = true,
    },
    {
        .test_name = "NotCreated_IncognitoFeatureDisabled_RegularProfile",
        .nullptr_expected = true,
    }};

INSTANTIATE_TEST_SUITE_P(
    CreateForWebContentsHelperTestSuiteInstantiation,
    CreateForWebContentsHelperTest,
    testing::ValuesIn<CreateForWebContentsTestCase>(kTestCases),
    [](const testing::TestParamInfo<CreateForWebContentsHelperTest::ParamType>&
           info) { return info.param.test_name; });

TEST_P(CreateForWebContentsHelperTest, CreateForWebContents) {
  const CreateForWebContentsTestCase& test_case = GetParam();

  SetFeatureFlags(test_case.is_regular_feature_enabled,
                  test_case.is_incognito_feature_enabled);

  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      RenderViewHostTestHarness::web_contents(), test_support_.prefs(),
      test_support_.tracking_protection_settings(),
      /*dealer=*/nullptr,
      /*is_incognito=*/test_case.is_incognito_profile);

  ExpectNullptr(test_case.nullptr_expected,
                FingerprintingProtectionWebContentsHelper::FromWebContents(
                    RenderViewHostTestHarness::web_contents()));
}

class MockFingerprintingProtectionObserver
    : public FingerprintingProtectionObserver {
 public:
  MOCK_METHOD(void, OnSubresourceBlocked, (), (override));
};

class FingerprintingProtectionNotifyOnBlockedResourcesTest
    : public content::RenderViewHostTestHarness {
 public:
  FingerprintingProtectionNotifyOnBlockedResourcesTest() = default;

  void SetUp() override { content::RenderViewHostTestHarness::SetUp(); }

  void TearDown() override {
    RenderViewHostTestHarness::TearDown();
    scoped_feature_list_.Reset();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestSupport test_support_;
};

TEST_F(FingerprintingProtectionNotifyOnBlockedResourcesTest,
       OnSubresourceBlockedCalled_NotifyOnBlockedResources) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableFingerprintingProtectionFilter);
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      RenderViewHostTestHarness::web_contents(), test_support_.prefs(),
      test_support_.tracking_protection_settings(),
      /*dealer=*/nullptr,
      /*is_incognito=*/false);

  auto* test_web_contents_helper =
      FingerprintingProtectionWebContentsHelper::FromWebContents(
          RenderViewHostTestHarness::web_contents());
  MockFingerprintingProtectionObserver observer;

  test_web_contents_helper->AddObserver(&observer);

  EXPECT_CALL(observer, OnSubresourceBlocked());
  test_web_contents_helper->NotifyOnBlockedResources();
}

TEST_F(FingerprintingProtectionNotifyOnBlockedResourcesTest,
       OnSubresourceBlockedNotCalled_WithoutNotifyOnBlockedResources) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableFingerprintingProtectionFilter);
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      RenderViewHostTestHarness::web_contents(), test_support_.prefs(),
      test_support_.tracking_protection_settings(),
      /*dealer=*/nullptr,
      /*is_incognito=*/false);

  auto* test_web_contents_helper =
      FingerprintingProtectionWebContentsHelper::FromWebContents(
          RenderViewHostTestHarness::web_contents());
  MockFingerprintingProtectionObserver observer;

  test_web_contents_helper->AddObserver(&observer);

  // Expect OnSubresourceBlocked is not called without NotifyOnBlockedResources
  // called.
  EXPECT_CALL(observer, OnSubresourceBlocked()).Times(0);
}

}  // namespace
}  // namespace fingerprinting_protection_filter
