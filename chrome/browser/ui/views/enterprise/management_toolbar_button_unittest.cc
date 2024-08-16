// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/enterprise/management_toolbar_button.h"

#include <tuple>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "testing/gtest/include/gtest/gtest.h"

class ManagementToolbarButtonUnitTest : public TestWithBrowserView {};

TEST_F(ManagementToolbarButtonUnitTest, VisibilityWithPefs) {
  auto* management_toolbar_button =
      browser_view()->toolbar()->management_toolbar_button();
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(nullptr, management_toolbar_button);
#else
  policy::ScopedManagementServiceOverrideForTesting profile_management(
      policy::ManagementServiceFactory::GetForProfile(GetProfile()),
      policy::EnterpriseManagementAuthority::NONE);
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::NONE);
  management_toolbar_button->UpdateManagementInfo();

  EXPECT_FALSE(management_toolbar_button->GetVisible());
  EXPECT_TRUE(management_toolbar_button->GetText().empty());

  GetProfile()->GetPrefs()->SetString(prefs::kEnterpriseCustomLabel, "value");
  EXPECT_TRUE(management_toolbar_button->GetVisible());
  EXPECT_EQ(u"value", management_toolbar_button->GetText());

  GetProfile()->GetPrefs()->ClearPref(prefs::kEnterpriseCustomLabel);
  EXPECT_FALSE(management_toolbar_button->GetVisible());
  EXPECT_TRUE(management_toolbar_button->GetText().empty());

  GetProfile()->GetPrefs()->SetString(prefs::kEnterpriseLogoUrl, "value");
  EXPECT_TRUE(management_toolbar_button->GetVisible());
  EXPECT_TRUE(management_toolbar_button->GetText().empty());

  GetProfile()->GetPrefs()->ClearPref(prefs::kEnterpriseLogoUrl);
  EXPECT_FALSE(management_toolbar_button->GetVisible());
#endif
}

class ManagementToolbarButtonFeatureFlagUnitTest
    : public ::testing::WithParamInterface<
          std::tuple<bool,
                     bool,
                     policy::EnterpriseManagementAuthority,
                     policy::EnterpriseManagementAuthority>>,
      public TestWithBrowserView {
 public:
  ManagementToolbarButtonFeatureFlagUnitTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (enable_feature()) {
      enabled_features.push_back(features::kManagementToolbarButton);
    } else {
      disabled_features.push_back(features::kManagementToolbarButton);
    }
    if (enable_feature_for_trusted_management()) {
      enabled_features.push_back(
          features::kManagementToolbarButtonForTrustedManagementSources);
    } else {
      disabled_features.push_back(
          features::kManagementToolbarButtonForTrustedManagementSources);
    }
    features_.InitWithFeatures(enabled_features, disabled_features);
  }

  policy::ManagementService* GetProfileManagementService() {
    return policy::ManagementServiceFactory::GetForProfile(GetProfile());
  }

  bool enable_feature() { return std::get<0>(GetParam()); }
  bool enable_feature_for_trusted_management() {
    return std::get<1>(GetParam());
  }
  bool platform_authority() { return std::get<2>(GetParam()); }
  bool profile_authority() { return std::get<3>(GetParam()); }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_P(ManagementToolbarButtonFeatureFlagUnitTest, Visibility) {
  auto* management_toolbar_button =
      browser_view()->toolbar()->management_toolbar_button();
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(nullptr, management_toolbar_button);
#else
  ASSERT_NE(nullptr, management_toolbar_button);
  policy::ScopedManagementServiceOverrideForTesting profile_management(
      GetProfileManagementService(), profile_authority());
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(), platform_authority());

  management_toolbar_button->UpdateManagementInfo();
  if (enable_feature()) {
    EXPECT_EQ(management_toolbar_button->GetVisible(),
              GetProfileManagementService()->IsManaged());
  } else if (enable_feature_for_trusted_management()) {
    bool profile_managed = GetProfileManagementService()->IsManaged();
    bool profile_management_trusted =
        GetProfileManagementService()
            ->GetManagementAuthorityTrustworthiness() >=
        policy::ManagementAuthorityTrustworthiness::TRUSTED;
    bool device_management_trusted =
        policy::ManagementServiceFactory::GetForPlatform()
            ->GetManagementAuthorityTrustworthiness() >=
        policy::ManagementAuthorityTrustworthiness::TRUSTED;
    EXPECT_EQ(management_toolbar_button->GetVisible(),
              profile_managed &&
                  (profile_management_trusted || device_management_trusted));

  } else {
    EXPECT_FALSE(management_toolbar_button->GetVisible());
  }
  EXPECT_TRUE(management_toolbar_button->GetText().empty());
#endif
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ManagementToolbarButtonFeatureFlagUnitTest,
    ::testing::Combine(
        /*enable_feature*/ ::testing::Bool(),
        /*enable_feature_for_trusted_management*/ ::testing::Bool(),
        /*platform_authority*/
        ::testing::Values(policy::EnterpriseManagementAuthority::NONE,
                          policy::EnterpriseManagementAuthority::COMPUTER_LOCAL,
                          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL,
                          policy::EnterpriseManagementAuthority::CLOUD,
                          policy::EnterpriseManagementAuthority::CLOUD_DOMAIN),
        /*profile_authority*/
        ::testing::Values(
            policy::EnterpriseManagementAuthority::NONE,
            policy::EnterpriseManagementAuthority::COMPUTER_LOCAL,
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL,
            policy::EnterpriseManagementAuthority::CLOUD,
            policy::EnterpriseManagementAuthority::CLOUD_DOMAIN)));
