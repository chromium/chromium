// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_battery_saver_control.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/toolbar/mock_webui_toolbar_control_delegate.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/safe_castable.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "chrome/common/chrome_features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/common/content_features.h"
#endif

namespace {

// Minimal `BrowserElements` implementation backed by a fake `ElementContext`
// for unit tests.
class TestBrowserElements : public BrowserElements {
 public:
  DECLARE_SAFE_CAST_TARGET()

  TestBrowserElements(BrowserWindowInterface& browser,
                      ui::ElementContext context)
      : BrowserElements(browser), context_(context) {}
  ~TestBrowserElements() override = default;

  // BrowserElements:
  ui::ElementContext GetContext() override { return context_; }

 private:
  const ui::ElementContext context_;
};

DEFINE_SAFE_CAST_TARGET(TestBrowserElements)

}  // namespace

class WebUIBatterySaverControlTestBase : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    browser_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();

    ON_CALL(*browser_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));

    browser_elements_ = std::make_unique<TestBrowserElements>(
        *browser_interface_, ui::ElementContext::CreateFakeContextForTesting(
                                 &unowned_user_data_host_));

    user_education_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserUserEducationInterface>>(
            browser_interface_.get());

    delegate_ =
        std::make_unique<testing::NiceMock<MockWebUIToolbarControlDelegate>>();
    ON_CALL(*delegate_, GetBrowser())
        .WillByDefault(testing::Return(browser_interface_.get()));
  }

  void TearDown() override {
    control_.reset();
    delegate_.reset();
    user_education_interface_.reset();
    browser_elements_.reset();
    browser_interface_.reset();
    testing::Test::TearDown();
  }

 protected:
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      browser_interface_;
  std::unique_ptr<TestBrowserElements> browser_elements_;
  std::unique_ptr<testing::NiceMock<MockBrowserUserEducationInterface>>
      user_education_interface_;
  std::unique_ptr<testing::NiceMock<MockWebUIToolbarControlDelegate>> delegate_;
  std::unique_ptr<WebUIBatterySaverControl> control_;
};

class WebUIBatterySaverControlTest : public WebUIBatterySaverControlTestBase {
 public:
  WebUIBatterySaverControlTest() {
#if BUILDFLAG(IS_CHROMEOS)
    // Disable OS-managed battery saver so the non-OS-managed control path is
    // covered on ChromeOS as well.
    feature_list_.InitAndDisableFeature(ash::features::kBatterySaver);
#endif
  }

  void SetUp() override {
    WebUIBatterySaverControlTestBase::SetUp();
    control_ = std::make_unique<WebUIBatterySaverControl>(delegate_.get());
  }

#if BUILDFLAG(IS_CHROMEOS)
 private:
  base::test::ScopedFeatureList feature_list_;
#endif
};

TEST_F(WebUIBatterySaverControlTest, ShowAndHide) {
  EXPECT_CALL(*delegate_, OnPreferredSizeChanged()).Times(1);
  EXPECT_CALL(
      *delegate_,
      OnBatterySaverControlStateChanged(testing::Pointee(testing::Field(
          &toolbar_ui_api::mojom::BatterySaverControlState::should_be_shown,
          true))));
  control_->Show();

  EXPECT_CALL(*delegate_, OnPreferredSizeChanged()).Times(1);
  EXPECT_CALL(
      *delegate_,
      OnBatterySaverControlStateChanged(testing::Pointee(testing::Field(
          &toolbar_ui_api::mojom::BatterySaverControlState::should_be_shown,
          false))));
  control_->Hide();
}

#if BUILDFLAG(IS_CHROMEOS)
class WebUIBatterySaverControlManagedByOSTest
    : public WebUIBatterySaverControlTestBase {
 public:
  WebUIBatterySaverControlManagedByOSTest() {
    feature_list_.InitWithFeatures(
        {ash::features::kBatterySaver, features::kInitialWebUI,
         features::kWebUIBatterySaverButton},
        {});
  }

  void SetUp() override {
    WebUIBatterySaverControlTestBase::SetUp();
    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
    tuning_environment_.SetUp(&local_state_);
  }

  void TearDown() override {
    control_.reset();
    tuning_environment_.TearDown();
    WebUIBatterySaverControlTestBase::TearDown();
  }

 protected:
  void ExpectNoUpdatesOrPromo() {
    EXPECT_CALL(*delegate_, OnPreferredSizeChanged()).Times(0);
    EXPECT_CALL(*delegate_, OnBatterySaverControlStateChanged(testing::_))
        .Times(0);
    EXPECT_CALL(*user_education_interface_, MaybeShowFeaturePromo(testing::_))
        .Times(0);
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  performance_manager::user_tuning::TestUserPerformanceTuningManagerEnvironment
      tuning_environment_;
};

TEST_F(WebUIBatterySaverControlManagedByOSTest,
       InitDoesNotShowWhenBatterySaverActive) {
  performance_manager::user_tuning::
      TestUserPerformanceTuningManagerEnvironment::SetBatterySaverMode(
          &local_state_, /*enabled=*/true);
  ExpectNoUpdatesOrPromo();

  control_ = std::make_unique<WebUIBatterySaverControl>(delegate_.get());
  control_->Init();

  EXPECT_FALSE(control_->IsVisible());
  EXPECT_FALSE(control_->CreateState()->should_be_shown);
}

TEST_F(WebUIBatterySaverControlManagedByOSTest,
       ShowDoesNothingWhenManagedByOS) {
  control_ = std::make_unique<WebUIBatterySaverControl>(delegate_.get());
  ExpectNoUpdatesOrPromo();

  control_->Show();

  EXPECT_FALSE(control_->IsVisible());
}

TEST_F(WebUIBatterySaverControlManagedByOSTest,
       ShowBubbleDoesNothingWhenManagedByOS) {
  control_ = std::make_unique<WebUIBatterySaverControl>(delegate_.get());
  ExpectNoUpdatesOrPromo();

  control_->ShowBubble();

  EXPECT_EQ(control_->bubble_for_testing(), nullptr);
}
#endif  // BUILDFLAG(IS_CHROMEOS)
