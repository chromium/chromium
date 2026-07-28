// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/gemini_step_eligibility_checker.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/test_variations_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class GeminiStepEligibilityCheckerTest : public testing::Test {
 public:
  GeminiStepEligibilityCheckerTest() {
    TestingProfile::Builder builder;
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        TestingBrowserProcess::GetGlobal()->local_state(),
        &metrics_enabled_state_provider_, std::wstring(), base::FilePath(),
        metrics::StartupVisibility::kUnknown);

    variations_service_ = std::make_unique<variations::TestVariationsService>(
        TestingBrowserProcess::GetGlobal()->local_state(),
        metrics_state_manager_.get());

    TestingBrowserProcess::GetGlobal()->SetVariationsService(
        variations_service_.get());
  }

  ~GeminiStepEligibilityCheckerTest() override {
    TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
  }

  Profile& profile() { return *profile_; }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }
  variations::TestVariationsService* variations_service() {
    return variations_service_.get();
  }

  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void SetCountry(const std::string& country) {
    variations_service_->OverrideStoredPermanentCountry(country);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        variations::switches::kVariationsOverrideCountry, country);
  }

  AccountInfo MakePrimaryAccountAvailable(const std::string& email) {
    return identity_test_env()->MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
  }

  void UpdateAccountCapabilities(const AccountInfo& account_info) {
    AccountInfo updated_info = account_info;
    AccountCapabilitiesTestMutator mutator(&updated_info);
    mutator.SetAllSupportedCapabilities(true);
    identity_test_env()->UpdateAccountInfoForAccount(updated_info);
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedCommandLine scoped_command_line_;
  metrics::TestEnabledStateProvider metrics_enabled_state_provider_{false,
                                                                    false};
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
};

TEST_F(GeminiStepEligibilityCheckerTest, FailsImmediatelyIfSignedOut) {
  GeminiStepEligibilityChecker checker;
  base::test::TestFuture<bool> future;

  checker.CheckEligibility(profile(), future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(GeminiStepEligibilityCheckerTest, WaitsForCountryData) {
  GeminiStepEligibilityChecker checker;
  base::test::TestFuture<bool> future;

  UpdateAccountCapabilities(MakePrimaryAccountAvailable("test@example.com"));

  checker.CheckEligibility(profile(), future.GetCallback());

  ASSERT_FALSE(future.IsReady());

  SetCountry("us");
  AdvanceClock(base::Milliseconds(500));

  EXPECT_FALSE(future.Get());
}

TEST_F(GeminiStepEligibilityCheckerTest, WaitsForAccountCapabilities) {
  GeminiStepEligibilityChecker checker;
  base::test::TestFuture<bool> future;

  SetCountry("us");

  const AccountInfo account_info =
      MakePrimaryAccountAvailable("test@example.com");

  checker.CheckEligibility(profile(), future.GetCallback());

  ASSERT_FALSE(future.IsReady());

  UpdateAccountCapabilities(account_info);

  EXPECT_FALSE(future.Get());
}

TEST_F(GeminiStepEligibilityCheckerTest,
       ResolvesImmediatelyIfAllDataAvailable) {
  GeminiStepEligibilityChecker checker;
  base::test::TestFuture<bool> future;

  SetCountry("us");

  UpdateAccountCapabilities(MakePrimaryAccountAvailable("test@example.com"));

  checker.CheckEligibility(profile(), future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(GeminiStepEligibilityCheckerTest, FailsOnIdentityManagerShutdown) {
  GeminiStepEligibilityChecker checker;
  base::test::TestFuture<bool> future;

  SetCountry("us");

  AccountInfo account_info = MakePrimaryAccountAvailable("test@example.com");

  checker.CheckEligibility(profile(), future.GetCallback());

  ASSERT_FALSE(future.IsReady());

  checker.OnIdentityManagerShutdown(identity_test_env()->identity_manager());

  EXPECT_FALSE(future.Get());
}
