// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
    content::BrowserContext* context) {
  auto* profile = Profile::FromBrowserContext(context);
  return std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
      *profile->GetPrefs(),
      TemplateURLServiceFactory::GetForProfile(profile),
      /*url_loader_factory=*/nullptr,
      /*identity_manager=*/nullptr);
}

// Shared test fixture for IsNtpComposeboxEnabled and IsNtpRealboxNextEnabled.
class NtpFieldTrialEnabledTest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    builder.AddTestingFactory(
        AimEligibilityServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockAimEligibilityService));
    profile_ = builder.Build();

    mock_service_ =
        static_cast<testing::NiceMock<MockAimEligibilityService>*>(
            AimEligibilityServiceFactory::GetForProfile(profile_.get()));
  }

 protected:
  void SetAllEligible(bool eligible) {
    ON_CALL(*mock_service_, IsAimLocallyEligible())
        .WillByDefault(testing::Return(eligible));
    ON_CALL(*mock_service_, IsAimEligible())
        .WillByDefault(testing::Return(eligible));
    ON_CALL(*mock_service_, IsFuseboxEligible())
        .WillByDefault(testing::Return(eligible));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<testing::NiceMock<MockAimEligibilityService>> mock_service_ =
      nullptr;
};

// --- IsNtpComposeboxEnabled tests ---

using IsNtpComposeboxEnabledTest = NtpFieldTrialEnabledTest;

TEST_F(IsNtpComposeboxEnabledTest, ReturnsFalseForNullProfile) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(ntp_composebox::kNtpComposebox);
  EXPECT_FALSE(ntp_composebox::IsNtpComposeboxEnabled(nullptr));
}

TEST_F(IsNtpComposeboxEnabledTest, ReturnsFalseWhenFeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(ntp_composebox::kNtpComposebox);
  SetAllEligible(true);
  EXPECT_FALSE(ntp_composebox::IsNtpComposeboxEnabled(profile_.get()));
}

TEST_F(IsNtpComposeboxEnabledTest, ReturnsFalseWhenNotEligible) {
  base::test::ScopedFeatureList features;
  SetAllEligible(false);
  EXPECT_FALSE(ntp_composebox::IsNtpComposeboxEnabled(profile_.get()));
}

TEST_F(IsNtpComposeboxEnabledTest, ReturnsTrueWhenEligible) {
  base::test::ScopedFeatureList features;
  SetAllEligible(true);
  EXPECT_TRUE(ntp_composebox::IsNtpComposeboxEnabled(profile_.get()));
}

TEST_F(IsNtpComposeboxEnabledTest,
       ReturnsFalseWhenAimEligibleButNotFuseboxEligible) {
  base::test::ScopedFeatureList features;
  ON_CALL(*mock_service_, IsAimLocallyEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_service_, IsAimEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_service_, IsFuseboxEligible())
      .WillByDefault(testing::Return(false));
  EXPECT_FALSE(ntp_composebox::IsNtpComposeboxEnabled(profile_.get()));
}

// --- IsNtpRealboxNextEnabled tests ---

using IsNtpRealboxNextEnabledTest = NtpFieldTrialEnabledTest;

TEST_F(IsNtpRealboxNextEnabledTest, ReturnsFalseForNullProfile) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {}, {ntp_composebox::kNtpComposebox, ntp_realbox::kNtpRealboxNext});
  EXPECT_FALSE(ntp_realbox::IsNtpRealboxNextEnabled(nullptr));
}

TEST_F(IsNtpRealboxNextEnabledTest, ReturnsFalseWhenComposeboxDisabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {}, {ntp_composebox::kNtpComposebox, ntp_realbox::kNtpRealboxNext});
  SetAllEligible(true);
  EXPECT_FALSE(ntp_realbox::IsNtpRealboxNextEnabled(profile_.get()));
}

TEST_F(IsNtpRealboxNextEnabledTest,
       ReturnsFalseWhenOnlyRealboxNextEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(ntp_composebox::kNtpComposebox);
  SetAllEligible(true);
  EXPECT_FALSE(ntp_realbox::IsNtpRealboxNextEnabled(profile_.get()));
}

TEST_F(IsNtpRealboxNextEnabledTest, ReturnsFalseWhenNotEligible) {
  SetAllEligible(false);
  EXPECT_FALSE(ntp_realbox::IsNtpRealboxNextEnabled(profile_.get()));
}

TEST_F(IsNtpRealboxNextEnabledTest, ReturnsTrueWhenEligible) {
  SetAllEligible(true);
  EXPECT_TRUE(ntp_realbox::IsNtpRealboxNextEnabled(profile_.get()));
}

TEST_F(IsNtpRealboxNextEnabledTest,
       ReturnsFalseWhenAimEligibleButNotFuseboxEligible) {
  ON_CALL(*mock_service_, IsAimLocallyEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_service_, IsAimEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_service_, IsFuseboxEligible())
      .WillByDefault(testing::Return(false));
  EXPECT_FALSE(ntp_realbox::IsNtpRealboxNextEnabled(profile_.get()));
}

}  // namespace
