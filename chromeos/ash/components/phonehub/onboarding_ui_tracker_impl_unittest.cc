// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/onboarding_ui_tracker_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromeos/ash/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/ash/components/phonehub/feature_status.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

namespace {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

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
    fake_feature_status_provider_->SetStatus(
        FeatureStatus::kNotEligibleForFeature);
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

  void SetFeatureState(Feature feature, FeatureState state) {
    fake_multidevice_setup_client_.SetFeatureState(feature, state);
  }

  void DismissSetupUi() { controller_->DismissSetupUi(); }

  bool ShouldShowOnboardingUi() const {
    return controller_->ShouldShowOnboardingUi();
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

  void HandleGetStarted() {
    controller_->HandleGetStarted(/*is_icon_clicked_when_nudge_visible=*/false);
  }

  void InvokePendingSetFeatureEnabledStateCallback(bool expected_enabled) {
    fake_multidevice_setup_client_.InvokePendingSetFeatureEnabledStateCallback(
        Feature::kPhoneHub, expected_enabled, std::nullopt, true);
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

TEST_F(OnboardingUiTrackerImplTest, HideUiWhenFeatureIsEnabled) {
  SetStatus(FeatureStatus::kNotEligibleForFeature);
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 0U);

  SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 1U);
  EXPECT_TRUE(ShouldShowOnboardingUi());

  // Simulate feature disabled feature. Expect onboarding UI to still be
  // displayed.
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 1U);
  EXPECT_TRUE(ShouldShowOnboardingUi());

  // Toggle the feature to be enabled. Expect onboarding UI to no longer be
  // displayed.
  SetFeatureState(Feature::kPhoneHub, FeatureState::kEnabledByUser);
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 2U);
  EXPECT_FALSE(ShouldShowOnboardingUi());

  // Toggle the feature back to disabled. Expect onboarding UI to still be
  // hidden.
  SetFeatureState(Feature::kPhoneHub, FeatureState::kDisabledByUser);
  EXPECT_EQ(GetOnShouldShowOnboardingUiChangedCallCount(), 2U);
  EXPECT_FALSE(ShouldShowOnboardingUi());
}

}  // namespace phonehub
}  // namespace ash
