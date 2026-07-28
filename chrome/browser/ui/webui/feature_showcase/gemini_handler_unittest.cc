// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/gemini_handler.h"

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_metrics.h"
#include "chrome/browser/ui/webui/feature_showcase/gemini.mojom.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/glic/glic_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class GeminiHandlerTest : public testing::Test {
 public:
  GeminiHandlerTest() = default;
  ~GeminiHandlerTest() override = default;

  void SetUp() override {
    glic_profile_manager_ = std::make_unique<glic::GlicProfileManager>();
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());

    profile_ = testing_profile_manager_->CreateTestingProfile("test_profile");

    glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);

    mock_glic_service_ =
        std::make_unique<testing::NiceMock<glic::MockGlicKeyedService>>(
            profile_, IdentityManagerFactory::GetForProfile(profile_),
            TestingBrowserProcess::GetGlobal()->profile_manager(),
            glic_profile_manager_.get(), nullptr, nullptr);
  }

  void TearDown() override {
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
  }

  Profile* profile() { return profile_; }
  glic::GlicKeyedService* glic_service() { return mock_glic_service_.get(); }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<glic::GlicProfileManager> glic_profile_manager_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<testing::NiceMock<glic::MockGlicKeyedService>>
      mock_glic_service_;
};

TEST_F(GeminiHandlerTest, AcceptConsent) {
  mojo::PendingReceiver<feature_showcase::mojom::GeminiPageHandler> receiver;
  GeminiHandler handler(std::move(receiver), glic_service());

  handler.AcceptConsent();

  EXPECT_EQ(profile()->GetPrefs()->GetInteger(glic::prefs::kGlicCompletedFre),
            static_cast<int>(glic::prefs::FreStatus::kCompleted));

  histogram_tester_.ExpectUniqueSample(
      "ProfilePicker.FREFlow.FeatureShowcase.StepUserAction.Gemini",
      FeatureShowcaseStepUserAction::kAccepted, 1);
}

TEST_F(GeminiHandlerTest, SkipConsent) {
  mojo::PendingReceiver<feature_showcase::mojom::GeminiPageHandler> receiver;
  GeminiHandler handler(std::move(receiver), glic_service());

  handler.SkipConsent();

  EXPECT_EQ(profile()->GetPrefs()->GetInteger(glic::prefs::kGlicCompletedFre),
            static_cast<int>(glic::prefs::FreStatus::kIncomplete));

  histogram_tester_.ExpectUniqueSample(
      "ProfilePicker.FREFlow.FeatureShowcase.StepUserAction.Gemini",
      FeatureShowcaseStepUserAction::kDeclined, 1);
}
