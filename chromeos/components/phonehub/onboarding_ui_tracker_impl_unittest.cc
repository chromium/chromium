// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/onboarding_ui_tracker_impl.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

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
    controller_ = std::make_unique<OnboardingUiTrackerImpl>();
    controller_->AddObserver(&fake_observer_);
  }

  void TearDown() override { controller_->RemoveObserver(&fake_observer_); }

  bool ShouldShowOnboardingUi() const {
    return controller_->ShouldShowOnboardingUi();
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 private:
  FakeObserver fake_observer_;
  std::unique_ptr<OnboardingUiTracker> controller_;
};

// TODO(https://crbug.com/1106937): Remove this test once we have real
// functionality to test.
TEST_F(OnboardingUiTrackerImplTest, Initialize) {
  EXPECT_FALSE(ShouldShowOnboardingUi());
}

}  // namespace phonehub
}  // namespace chromeos
