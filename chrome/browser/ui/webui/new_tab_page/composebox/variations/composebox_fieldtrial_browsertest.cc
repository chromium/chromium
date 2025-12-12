// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"

#include <memory>
#include <optional>
#include <string>
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
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"

// A test AimEligibilityService that returns fixed eligibility values.
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

class NtpFieldTrialBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, std::string, bool, bool, bool, bool, bool>> {
 public:
  NtpFieldTrialBrowserTest() = default;
  ~NtpFieldTrialBrowserTest() override = default;

 protected:
  virtual bool IsFeatureEnabledByDefault() const = 0;

  // This test is testing the default state of a feature, so we need to ensure
  // that field trial configs don't change the feature state.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }

  void SetUpOnMainThread() override {
    scoped_browser_locale_ =
        std::make_unique<ScopedBrowserLocale>(std::get<0>(GetParam()));
    g_browser_process->variations_service()->OverrideStoredPermanentCountry(
        std::get<1>(GetParam()));
    auto is_locally_eligible = std::get<2>(GetParam());
    auto is_server_eligible = std::get<3>(GetParam());
    auto server_eligibility_enabled = std::get<4>(GetParam());

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

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    scoped_browser_locale_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

  bool GetExpectedEnabled() {
    auto [locale, country, is_locally_eligible, is_server_eligible,
          server_eligibility_enabled, override_feature, feature] = GetParam();

    bool expected_enabled = false;

    // Mirrored implementation logic for Is...Enabled methods:
    // Get the service to check server eligibility (this is now handled by the
    // mock).
    auto* service =
        AimEligibilityServiceFactory::GetForProfile(browser()->profile());

    // If service or local eligibility check fails, return false.
    if (!service || !service->IsAimLocallyEligible()) {
      return false;
    }

    // If the generic feature is overridden, it takes precedence.
    if (override_feature) {
      return feature;
    }

    // If the server eligibility is enabled, return overall eligibility alone.
    // The service will control locale rollout so there's no need to check
    // locale or the state of kMyFeature below.
    if (service->IsServerEligibilityEnabled()) {
      expected_enabled = service->IsAimEligible();
    } else {
      // Otherwise, check the generic entrypoint feature default value.
      expected_enabled = IsFeatureEnabledByDefault();
    }

    return expected_enabled;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ScopedBrowserLocale> scoped_browser_locale_;
};

class NtpComposeboxFieldTrialBrowserTest : public NtpFieldTrialBrowserTest {
 public:
  NtpComposeboxFieldTrialBrowserTest() = default;
  ~NtpComposeboxFieldTrialBrowserTest() override = default;
  NtpComposeboxFieldTrialBrowserTest(
      const NtpComposeboxFieldTrialBrowserTest&) = delete;
  NtpComposeboxFieldTrialBrowserTest& operator=(
      const NtpComposeboxFieldTrialBrowserTest&) = delete;

 protected:
  bool IsFeatureEnabledByDefault() const override {
    return ntp_composebox::kNtpComposebox.default_state ==
           base::FEATURE_ENABLED_BY_DEFAULT;
  }

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    const auto override_feature = std::get<5>(GetParam());
    const auto composebox_feature = std::get<6>(GetParam());
    if (override_feature) {
      if (composebox_feature) {
        enabled_features.push_back(ntp_composebox::kNtpComposebox);
      } else {
        disabled_features.push_back(ntp_composebox::kNtpComposebox);
      }
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);

    InProcessBrowserTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         NtpComposeboxFieldTrialBrowserTest,
                         ::testing::Combine(
                             // Values for the locale.
                             ::testing::Values("en-US", "es-MX"),
                             // Values for the country.
                             ::testing::Values("us", "ca"),
                             // Values for local eligibility.
                             ::testing::Values(true, false),
                             // Values for server eligibility.
                             ::testing::Values(true, false),
                             // Values for server eligibility enabled.
                             ::testing::Values(true, false),
                             // Values for whether to override the generic
                             // composebox feature.
                             ::testing::Values(true, false),
                             // Values for the generic composebox feature.
                             ::testing::Values(true, false)));

IN_PROC_BROWSER_TEST_P(NtpComposeboxFieldTrialBrowserTest, Test) {
  bool expected = GetExpectedEnabled();
  if (!base::FeatureList::IsEnabled(ntp_composebox::kNtpComposebox)) {
    expected = false;
  }
  EXPECT_EQ(ntp_composebox::IsNtpComposeboxEnabled(browser()->profile()),
            expected);
}

class NtpRealboxNextFieldTrialBrowserTest : public NtpFieldTrialBrowserTest {
 public:
  NtpRealboxNextFieldTrialBrowserTest() = default;
  ~NtpRealboxNextFieldTrialBrowserTest() override = default;
  NtpRealboxNextFieldTrialBrowserTest(
      const NtpRealboxNextFieldTrialBrowserTest&) = delete;
  NtpRealboxNextFieldTrialBrowserTest& operator=(
      const NtpRealboxNextFieldTrialBrowserTest&) = delete;

 protected:
  bool IsFeatureEnabledByDefault() const override {
    return ntp_composebox::kNtpComposebox.default_state ==
               base::FEATURE_ENABLED_BY_DEFAULT &&
           ntp_realbox::kNtpRealboxNext.default_state ==
               base::FEATURE_ENABLED_BY_DEFAULT;
  }

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    const auto override_feature = std::get<5>(GetParam());
    const auto feature = std::get<6>(GetParam());
    if (override_feature) {
      if (feature) {
        enabled_features.push_back(ntp_composebox::kNtpComposebox);
        enabled_features.push_back(ntp_realbox::kNtpRealboxNext);
      } else {
        disabled_features.push_back(ntp_realbox::kNtpRealboxNext);
      }
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);

    InProcessBrowserTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         NtpRealboxNextFieldTrialBrowserTest,
                         ::testing::Combine(
                             // Values for the locale.
                             ::testing::Values("en-US", "es-MX"),
                             // Values for the country.
                             ::testing::Values("us", "ca"),
                             // Values for local eligibility.
                             ::testing::Values(true, false),
                             // Values for server eligibility.
                             ::testing::Values(true, false),
                             // Values for server eligibility enabled.
                             ::testing::Values(true, false),
                             // Values for whether to override the generic
                             // realbox next feature.
                             ::testing::Values(true, false),
                             // Values for the generic realbox next feature.
                             ::testing::Values(true, false)));

IN_PROC_BROWSER_TEST_P(NtpRealboxNextFieldTrialBrowserTest, Test) {
  bool expected = GetExpectedEnabled();
  if (!base::FeatureList::IsEnabled(ntp_realbox::kNtpRealboxNext)) {
    expected = false;
  }
  EXPECT_EQ(ntp_realbox::IsNtpRealboxNextEnabled(browser()->profile()),
            expected);
}
