// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"

#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/lens/lens_features.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// A test AimEligibilityService that returns a fixed eligibility value.
class TestingAimEligibilityService : public ChromeAimEligibilityService {
 public:
  explicit TestingAimEligibilityService(
      bool is_locally_eligible,
      bool is_server_eligible,
      bool server_eligibility_enabled,
      PrefService& pref_service,
      TemplateURLService* template_url_service)
      : ChromeAimEligibilityService(pref_service,
                                    template_url_service,
                                    /*url_loader_factory=*/nullptr,
                                    /*identity_manager=*/nullptr,
                                    /*is_off_the_record=*/false),
        is_locally_eligible_(is_locally_eligible),
        is_server_eligible_(is_server_eligible),
        server_eligibility_enabled_(server_eligibility_enabled) {}

  ~TestingAimEligibilityService() override = default;

  bool IsAimLocallyEligible() const override { return is_locally_eligible_; }
  bool IsServerEligibilityEnabled() const override {
    return server_eligibility_enabled_;
  }
  bool IsAimEligible() const override {
    if (!IsAimLocallyEligible()) {
      return false;
    }
    if (IsServerEligibilityEnabled()) {
      return is_server_eligible_;
    }
    return true;
  }

 private:
  bool is_locally_eligible_;
  bool is_server_eligible_;
  bool server_eligibility_enabled_;
};

class LensSearchFeatureFlagsUtilsBrowserTestBase : public InProcessBrowserTest {
 public:
  LensSearchFeatureFlagsUtilsBrowserTestBase() = default;
  ~LensSearchFeatureFlagsUtilsBrowserTestBase() override = default;

  // These tests are testing the default state of a feature, so disable the
  // field trial configs to ensure the default state is not changed.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }

 protected:
  void SetUpAimEligibilityService(bool is_locally_eligible,
                                  bool is_server_eligible,
                                  bool server_eligibility_enabled) {
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindLambdaForTesting(
            [is_locally_eligible, is_server_eligible,
             server_eligibility_enabled](content::BrowserContext* context) {
              Profile* profile = Profile::FromBrowserContext(context);
              return static_cast<std::unique_ptr<KeyedService>>(
                  std::make_unique<TestingAimEligibilityService>(
                      is_locally_eligible, is_server_eligible,
                      server_eligibility_enabled, *profile->GetPrefs(),
                      TemplateURLServiceFactory::GetForProfile(profile)));
            }));
  }
};

// Test fixture with kLensSearchAimM3 feature enabled.
class LensSearchFeatureFlagsUtilsAimM3EnabledTest
    : public LensSearchFeatureFlagsUtilsBrowserTestBase {
 public:
  LensSearchFeatureFlagsUtilsAimM3EnabledTest() = default;
  ~LensSearchFeatureFlagsUtilsAimM3EnabledTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_{lens::features::kLensSearchAimM3};
};

IN_PROC_BROWSER_TEST_F(LensSearchFeatureFlagsUtilsAimM3EnabledTest,
                       TestIsAimM3Enabled_IsTrue) {
  SetUpAimEligibilityService(/*is_locally_eligible=*/true,
                             /*is_server_eligible=*/true,
                             /*server_eligibility_enabled=*/true);
  EXPECT_TRUE(lens::IsAimM3Enabled(browser()->profile()));

  // Returns true when server eligibility checking is disabled as long as the
  // local eligibility check passes.
  SetUpAimEligibilityService(/*is_locally_eligible=*/true,
                             /*is_server_eligible=*/false,
                             /*server_eligibility_enabled=*/false);
  EXPECT_TRUE(lens::IsAimM3Enabled(browser()->profile()));
}


// Test fixture with kLensSearchAimM3 feature disabled.
class LensSearchFeatureFlagsUtilsAimM3DisabledTest
    : public LensSearchFeatureFlagsUtilsBrowserTestBase {
 public:
  LensSearchFeatureFlagsUtilsAimM3DisabledTest() {
    // Initialize the feature list to disable kLensSearchAimM3.
    feature_list_.InitWithFeatures(/*enabled_features=*/{},
                                   /*disabled_features=*/
                                   {lens::features::kLensSearchAimM3});
  }
  ~LensSearchFeatureFlagsUtilsAimM3DisabledTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LensSearchFeatureFlagsUtilsAimM3DisabledTest,
                       TestIsAimM3Enabled_WithFlagDisabled_IsFalse) {
  SetUpAimEligibilityService(/*is_locally_eligible=*/true,
                             /*is_server_eligible=*/true,
                             /*server_eligibility_enabled=*/true);
  EXPECT_FALSE(lens::IsAimM3Enabled(browser()->profile()));
}

// Test fixture with kLensSearchAimM3EnUs enabled and kLensSearchAimM3 default.
class LensSearchFeatureFlagsUtilsAimM3EnUsEnabledTest
    : public LensSearchFeatureFlagsUtilsBrowserTestBase {
 public:
  ~LensSearchFeatureFlagsUtilsAimM3EnUsEnabledTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_{
      lens::features::kLensSearchAimM3EnUs};
};

IN_PROC_BROWSER_TEST_F(LensSearchFeatureFlagsUtilsAimM3EnUsEnabledTest,
                       TestIsAimM3Enabled_IsTrueForEnUs) {
  // Set locale to en-US.
  ScopedBrowserLocale scoped_locale{"en-US"};
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");

  // Returns true when server eligibility checking is disabled, the flag is
  // enabled, and locale is en-US.
  SetUpAimEligibilityService(/*is_locally_eligible=*/true,
                             /*is_server_eligible=*/false,
                             /*server_eligibility_enabled=*/false);
  EXPECT_TRUE(lens::IsAimM3Enabled(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(LensSearchFeatureFlagsUtilsAimM3EnUsEnabledTest,
                       TestIsAimM3Enabled_IsFalse) {
  // Set locale to en-US.
  ScopedBrowserLocale scoped_locale{"en-US"};
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");

  // Returns false when locally ineligible.
  SetUpAimEligibilityService(/*is_locally_eligible=*/false,
                             /*is_server_eligible=*/false,
                             /*server_eligibility_enabled=*/false);
  EXPECT_FALSE(lens::IsAimM3Enabled(browser()->profile()));

  // Returns false when server eligibility checking is enabled and the server
  // returns ineligible.
  SetUpAimEligibilityService(/*is_locally_eligible=*/true,
                             /*is_server_eligible=*/false,
                             /*server_eligibility_enabled=*/true);
  EXPECT_FALSE(lens::IsAimM3Enabled(browser()->profile()));

  // Country is not US.
  {
    ScopedBrowserLocale scoped_locale_ca{"en-US"};
    g_browser_process->variations_service()->OverrideStoredPermanentCountry(
        "ca");
    SetUpAimEligibilityService(/*is_locally_eligible=*/true,
                               /*is_server_eligible=*/false,
                               /*server_eligibility_enabled=*/false);
    EXPECT_FALSE(lens::IsAimM3Enabled(browser()->profile()));
  }

  // Locale is not en.
  {
    ScopedBrowserLocale scoped_locale_fr{"fr-US"};
    g_browser_process->variations_service()->OverrideStoredPermanentCountry(
        "us");
    SetUpAimEligibilityService(/*is_locally_eligible=*/true,
                               /*is_server_eligible=*/false,
                               /*server_eligibility_enabled=*/false);
    EXPECT_FALSE(lens::IsAimM3Enabled(browser()->profile()));
  }
}

// Test fixture for verifying en-US users are routed to the eligibility service
// when kLensSearchAimM3EnUs is enabled.
class LensSearchFeatureFlagsUtilsAimM3EnUsUsesEligibilityTest
    : public LensSearchFeatureFlagsUtilsBrowserTestBase {
 public:
  LensSearchFeatureFlagsUtilsAimM3EnUsUsesEligibilityTest() = default;
  ~LensSearchFeatureFlagsUtilsAimM3EnUsUsesEligibilityTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_{
      lens::features::kLensSearchAimM3EnUs};
};

IN_PROC_BROWSER_TEST_F(LensSearchFeatureFlagsUtilsAimM3EnUsUsesEligibilityTest,
                       TestIsAimM3Enabled_EnUsUsesAimEligibilityService) {
  // Set locale to en-US.
  ScopedBrowserLocale scoped_locale{"en-US"};
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");

  // When the eligibility service returns eligible, IsAimM3Enabled should be
  // true.
  SetUpAimEligibilityService(/*is_locally_eligible=*/true,
                             /*is_server_eligible=*/true,
                             /*server_eligibility_enabled=*/true);
  EXPECT_TRUE(lens::IsAimM3Enabled(browser()->profile()));

  // When the eligibility service returns ineligible, IsAimM3Enabled should be
  // false.
  SetUpAimEligibilityService(/*is_locally_eligible=*/false,
                             /*is_server_eligible=*/false,
                             /*server_eligibility_enabled=*/false);
  EXPECT_FALSE(lens::IsAimM3Enabled(browser()->profile()));
}

// Test fixture for verifying that other users follow the
// kLensSearchAimM3UseAimEligibility flag. This is parameterized on whether the
// flag is enabled.
class LensSearchFeatureFlagsUtilsAimM3FollowsUseEligibilityFlagTest
    : public LensSearchFeatureFlagsUtilsBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  LensSearchFeatureFlagsUtilsAimM3FollowsUseEligibilityFlagTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    if (ShouldUseAimEligibility()) {
      enabled_features.push_back(
          lens::features::kLensSearchAimM3UseAimEligibility);
    }
    // Also enable the main M3 feature, since that would be the state for users
    // who would be using the eligibility service.
    enabled_features.push_back(lens::features::kLensSearchAimM3);
    feature_list_.InitWithFeatures(enabled_features, {});
  }
  ~LensSearchFeatureFlagsUtilsAimM3FollowsUseEligibilityFlagTest() override =
      default;

  bool ShouldUseAimEligibility() const { return GetParam(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    LensSearchFeatureFlagsUtilsAimM3FollowsUseEligibilityFlagTest,
    TestIsAimM3Enabled_OtherUsersFollowUseAimEligibilityFlag) {
  // Set locale to something other than en-US.
  ScopedBrowserLocale scoped_locale{"en-GB"};
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("gb");

  if (ShouldUseAimEligibility()) {
    // When the eligibility service returns eligible, IsAimM3Enabled should be
    // true.
    SetUpAimEligibilityService(/*is_locally_eligible=*/true,
                               /*is_server_eligible=*/true,
                               /*server_eligibility_enabled=*/true);
    EXPECT_TRUE(lens::IsAimM3Enabled(browser()->profile()));

    // When the eligibility service returns ineligible, IsAimM3Enabled should be
    // false.
    SetUpAimEligibilityService(/*is_locally_eligible=*/false,
                               /*is_server_eligible=*/false,
                               /*server_eligibility_enabled=*/false);
    EXPECT_FALSE(lens::IsAimM3Enabled(browser()->profile()));
  } else {
    // If not using the AIM service, the result depends on kLensSearchAimM3. In
    // this test fixture, we have it enabled.
    SetUpAimEligibilityService(/*is_locally_eligible=*/false,
                               /*is_server_eligible=*/false,
                               /*server_eligibility_enabled=*/false);
    EXPECT_TRUE(lens::IsAimM3Enabled(browser()->profile()));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    LensSearchFeatureFlagsUtilsAimM3FollowsUseEligibilityFlagTest,
    testing::Bool());
