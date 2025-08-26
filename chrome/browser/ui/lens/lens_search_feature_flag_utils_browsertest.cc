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
                                    /*identity_manager=*/nullptr),
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

IN_PROC_BROWSER_TEST_F(LensSearchFeatureFlagsUtilsAimM3EnabledTest,
                       TestIsAimM3Enabled_WithIneligibleService_IsFalse) {
  SetUpAimEligibilityService(/*is_locally_eligible=*/false,
                             /*is_server_eligible=*/true,
                             /*server_eligibility_enabled=*/true);
  EXPECT_FALSE(lens::IsAimM3Enabled(browser()->profile()));

  SetUpAimEligibilityService(/*is_locally_eligible=*/true,
                             /*is_server_eligible=*/false,
                             /*server_eligibility_enabled=*/true);
  EXPECT_FALSE(lens::IsAimM3Enabled(browser()->profile()));
}

// Test fixture with kLensSearchAimM3 feature enabled and eligibility check
// disabled.
class LensSearchFeatureFlagsUtilsAimM3EnabledAndEligibilityCheckDisabledTest
    : public LensSearchFeatureFlagsUtilsBrowserTestBase {
 public:
  LensSearchFeatureFlagsUtilsAimM3EnabledAndEligibilityCheckDisabledTest() {
    // Initialize the feature list to enable kLensSearchAimM3
    // and set the 'use-aim-eligibility-service' param to false.
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensSearchAimM3,
        {{"use-aim-eligibility-service", "false"}});
  }
  ~LensSearchFeatureFlagsUtilsAimM3EnabledAndEligibilityCheckDisabledTest()
      override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    LensSearchFeatureFlagsUtilsAimM3EnabledAndEligibilityCheckDisabledTest,
    TestIsAimM3EnabledButEligibilityCheckDisabled_IsTrue) {
  // Returns true when the eligibility check is disabled.
  SetUpAimEligibilityService(/*is_locally_eligible=*/false,
                             /*is_server_eligible=*/false,
                             /*server_eligibility_enabled=*/false);
  EXPECT_TRUE(lens::IsAimM3Enabled(browser()->profile()));
}

// Test fixture with kLensSearchAimM3 feature disabled.
class LensSearchFeatureFlagsUtilsAimM3DisabledTest
    : public LensSearchFeatureFlagsUtilsBrowserTestBase {
 public:
  LensSearchFeatureFlagsUtilsAimM3DisabledTest() = default;
  ~LensSearchFeatureFlagsUtilsAimM3DisabledTest() override = default;
};

IN_PROC_BROWSER_TEST_F(LensSearchFeatureFlagsUtilsAimM3DisabledTest,
                       TestIsAimM3Enabled_WithFlagDisabled_IsFalse) {
  SetUpAimEligibilityService(/*is_locally_eligible=*/true,
                             /*is_server_eligible=*/true,
                             /*server_eligibility_enabled=*/true);
  EXPECT_FALSE(lens::IsAimM3Enabled(browser()->profile()));
}
