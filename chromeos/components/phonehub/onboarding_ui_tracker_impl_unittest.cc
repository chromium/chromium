// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/onboarding_ui_tracker_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chromeos/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/components/phonehub/feature_status.h"
#include "chromeos/components/phonehub/pref_names.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

using multidevice_setup::mojom::Feature;

class FakeObserver : public OnboardingUiTracker::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // OnboardingUiTracker::Observer:
  void OnShouldShowOnboardingUiChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

}  // namespace

class OnboardingUiTrackerImplTest : public testing::Test {
 protected:
  OnboardingUiTrackerImplTest() = default;
  OnboardingUiTrackerImplTest(const OnboardingUiTrackerImplTest&) = delete;
  OnboardingUiTrackerImplTest& operator=(const OnboardingUiTrackerImplTest&) =
      delete;
  ~OnboardingUiTrackerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    OnboardingUiTrackerImpl::RegisterPrefs(pref_service_.registry());
    fake_feature_status_provider_ =
        std::make_unique<FakeFeatureStatusProvider>();
    controller_ = std::make_unique<OnboardingUiTrackerImpl>(
        &pref_service_, fake_feature_status_provider_.get(),
        &fake_multidevice_setup_client_,
        base::BindRepeating(
            &OnboardingUiTrackerImplTest::ShowMultideviceSetupDialog,
            base::Unretained(this)));
    controller_->AddObserver(&fake_observer_);
  }

  void TearDown() override { controller_->RemoveObserver(&fake_observer_); }

  void ShowMultideviceSetupDialog() { ++setup_dialog_shown_count_; }

  size_t setup_dialog_shown_count() { return setup_dialog_shown_count_; }

  void SetStatus(FeatureStatus feature_status) {
    fake_feature_status_provider_->SetStatus(feature_status);
  }

  void DismissSetupUi() { controller_->DismissSetupUi(); }

  bool ShouldShowOnboardingUi() const {
    return controller_->ShouldShowOnboardingUi();
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

  void HandleGetStarted() { controller_->HandleGetStarted(); }

  void InvokePendingSetFeatureEnabledStateCallback(bool expected_enabled) {
    fake_multidevice_setup_client_.InvokePendingSetFeatureEnabledStateCallback(
        Feature::kPhoneHub, expected_enabled, base::nullopt, true);
  }

  size_t GetOnShouldShowOnboardingUiChangedCallCount() {
    return fake_observer_.num_calls();
  }

 private:
  size_t setup_dialog_shown_count_ = 0;
  TestingPrefServiceSimple pref_service_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  std::unique_ptr<FakeFeatureStatusProvider> fake_feature_status_provider_;
  FakeObserver fake_observer_;
  std::unique_ptr<OnboardingUiTracker> controller_;
};

TEST_F(OnboardingUiTrackerImplTest, ShouldShowUiWhenEligiblePhoneButNotSetup) {
  SetStatus(FeatureStatus::kNotEligibleForFeature);
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 0U);

  SetStatus(FeatureStatus::kEligiblePhoneButNotSetUp);
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 1U);
  EXPECT_TRUE(ShouldShowOnboardingUi());

  // User clicks get started button.
  EXPECT_EQ(setup_dialog_shown_count(), 0U);
  HandleGetStarted();
  EXPECT_EQ(setup_dialog_shown_count(), 1U);
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 1U);

  // User dismisses setup flow. After dismissal, ShouldShowOnboardingUi() should
  // always be false.
  DismissSetupUi();
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 2U);
  EXPECT_FALSE(ShouldShowOnboardingUi());

  // User clicks the get started button a second time; should still open.
  EXPECT_EQ(setup_dialog_shown_count(), 1U);
  HandleGetStarted();
  EXPECT_EQ(setup_dialog_shown_count(), 2U);
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 2U);

  // User dismisses setup flow a second time.
  DismissSetupUi();
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 2U);
  EXPECT_FALSE(ShouldShowOnboardingUi());
}

TEST_F(OnboardingUiTrackerImplTest, ShouldShowUiWhenDisabled) {
  SetStatus(FeatureStatus::kNotEligibleForFeature);
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 0U);

  SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 1U);
  EXPECT_TRUE(ShouldShowOnboardingUi());

  // User clicks get started button.
  HandleGetStarted();
  InvokePendingSetFeatureEnabledStateCallback(true);
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 1U);

  // User dismisses setup flow. After dismissal, ShouldShowOnboardingUi() should
  // always be false.
  DismissSetupUi();
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 2U);
  EXPECT_FALSE(ShouldShowOnboardingUi());

  // User clicks the get started button a second time; should still open.
  HandleGetStarted();
  InvokePendingSetFeatureEnabledStateCallback(true);
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 2U);

  // User dismisses setup flow a second time.
  DismissSetupUi();
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 2U);
  EXPECT_FALSE(ShouldShowOnboardingUi());
}

}  // namespace phonehub
}  // namespace chromeos
