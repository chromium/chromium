// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_webui_profile_service.h"

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/waap/initial_webui_profile_service_factory.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_prerenderer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_provider_key.h"

namespace {

constexpr char kTakeResultHistogram[] =
    "InitialWebUI.Toolbar.ProfilePrewarmTakeResult";
using TakeResult = WebUIContentsPrerenderer::TakeResult;

// Not the 3 s default, so that the tests check that the param is read.
constexpr base::TimeDelta kPaintDeadline = base::Seconds(4);
constexpr base::TimeDelta kRefillDelay = base::Milliseconds(500);

// A mock browser window that the profile's browser collection tracks.
struct TestBrowser {
  testing::NiceMock<MockBrowserWindowInterface> window;
  BrowserWindowInterface::BrowserDidCloseCallback close_callback;
};

// Tests when the refill builds the next toolbar, with mock time. Without
// pre-navigation, a prewarmed toolbar needs no WebUI. It never paints here, so
// every refill waits for the paint deadline.
class InitialWebUIProfileServiceRefillTest
    : public ChromeRenderViewHostTestHarness {
 public:
  InitialWebUIProfileServiceRefillTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kWebUIReloadButton,
          {{"WebUIReloadButtonPrewarmWebUI", "true"},
           {"WebUIReloadButtonPrewarmWebUIPreNavigate", "false"},
           {"WebUIReloadButtonProfilePrewarming", "true"}}},
         {features::kTopChromeWebUIPrerendering,
          {{"TopChromeWebUIPrerenderingDelay", "500ms"},
           {"TopChromeWebUIPrerenderingPaintDeadline", "4s"}}}},
        {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // The profile's only normal window.
    AddBrowser(browser_, BrowserWindowInterface::TYPE_NORMAL);

    service_ = InitialWebUIProfileServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(service_);
    ASSERT_TRUE(prerenderer());
    ASSERT_FALSE(service_->has_toolbar_contents_for_testing());
    ASSERT_FALSE(prerenderer()->is_refill_pending_for_testing());
  }

  void TearDown() override {
    if (service_) {
      prerenderer()->Reset();
    }
    service_ = nullptr;
    CloseBrowser(other_browser_);
    CloseBrowser();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  // Adds `browser` of `type` to the profile's browser collection.
  void AddBrowser(TestBrowser& browser, BrowserWindowInterface::Type type) {
    ON_CALL(browser.window, RegisterBrowserDidClose(testing::_))
        .WillByDefault(
            [&browser](
                BrowserWindowInterface::BrowserDidCloseCallback callback) {
              browser.close_callback = std::move(callback);
              return base::CallbackListSubscription();
            });
    ON_CALL(browser.window, GetType()).WillByDefault(testing::Return(type));
    ON_CALL(browser.window, GetProfile())
        .WillByDefault(testing::Return(profile()));
    BrowserManagerServiceFactory::GetForProfile(profile())
        ->AddBrowserForTesting(&browser.window);
  }

  // Closes `browser`, which notifies the profile's browser collection.
  void CloseBrowser(TestBrowser& browser) {
    if (browser.close_callback) {
      std::move(browser.close_callback).Run(&browser.window);
    }
  }

  // Closes the profile's first window.
  void CloseBrowser() { CloseBrowser(browser_); }

  // Gives the service both a prewarmed toolbar and a pending refill.
  void HoldSpareWithPendingRefill() {
    WaitForFirstSpare();
    prerenderer()->ScheduleRefill(nullptr);
    ASSERT_TRUE(service_->has_toolbar_contents_for_testing());
    ASSERT_TRUE(prerenderer()->is_refill_pending_for_testing());
  }

  // Simulates the profile's first window taking the toolbar (recording
  // kNoSpare and scheduling the first refill) and fast-forwards past the
  // deadline and delay to establish the first refilled spare.
  void WaitForFirstSpare() {
    ASSERT_FALSE(service_->has_toolbar_contents_for_testing());
    EXPECT_FALSE(service_->TakeToolbarContents());
    EXPECT_TRUE(prerenderer()->is_refill_pending_for_testing());
    task_environment()->FastForwardBy(kPaintDeadline + kRefillDelay);
    ASSERT_TRUE(service_->has_toolbar_contents_for_testing());
    EXPECT_FALSE(prerenderer()->is_refill_pending_for_testing());
  }

  std::unique_ptr<content::WebContents> CreateTestContents() {
    return content::WebContentsTester::CreateTestWebContents(
        profile(), content::SiteInstance::Create(profile()));
  }

  InitialWebUIProfileService* service() { return service_; }
  WebUIContentsPrerenderer* prerenderer() {
    return service_->toolbar_prerenderer_for_testing();
  }

  // A second window, which tests add when they need one.
  TestBrowser other_browser_;

 private:
  base::test::ScopedFeatureList feature_list_;
  TestBrowser browser_;
  raw_ptr<InitialWebUIProfileService> service_ = nullptr;
};

TEST_F(InitialWebUIProfileServiceRefillTest, RefillsAfterDeadlineAndDelay) {
  base::HistogramTester histogram_tester;
  // First window builds its own toolbar and schedules the first refill.
  EXPECT_FALSE(service()->TakeToolbarContents());
  histogram_tester.ExpectUniqueSample(kTakeResultHistogram,
                                      TakeResult::kNoSpare, 1);
  EXPECT_TRUE(prerenderer()->is_refill_pending_for_testing());

  task_environment()->FastForwardBy(kPaintDeadline + kRefillDelay -
                                    base::Milliseconds(1));
  EXPECT_FALSE(service()->has_toolbar_contents_for_testing());
  task_environment()->FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(service()->has_toolbar_contents_for_testing());
  EXPECT_FALSE(prerenderer()->is_refill_pending_for_testing());

  // Second window adopts the refilled spare and schedules the next refill.
  std::unique_ptr<content::WebContents> taken =
      service()->TakeToolbarContents();
  ASSERT_TRUE(taken);
  EXPECT_TRUE(prerenderer()->is_refill_pending_for_testing());
  histogram_tester.ExpectBucketCount(kTakeResultHistogram, TakeResult::kAdopted,
                                     1);
}

TEST_F(InitialWebUIProfileServiceRefillTest,
       NonNormalWindowCloseKeepsSpareAndRefill) {
  AddBrowser(other_browser_, BrowserWindowInterface::TYPE_POPUP);
  HoldSpareWithPendingRefill();

  CloseBrowser(other_browser_);
  EXPECT_TRUE(service()->has_toolbar_contents_for_testing());
  EXPECT_TRUE(prerenderer()->is_refill_pending_for_testing());
}

TEST_F(InitialWebUIProfileServiceRefillTest,
       NonLastNormalWindowCloseKeepsSpareAndRefill) {
  AddBrowser(other_browser_, BrowserWindowInterface::TYPE_NORMAL);
  HoldSpareWithPendingRefill();

  CloseBrowser(other_browser_);
  EXPECT_TRUE(service()->has_toolbar_contents_for_testing());
  EXPECT_TRUE(prerenderer()->is_refill_pending_for_testing());

  // Closing the last normal window drops both.
  CloseBrowser();
  EXPECT_FALSE(service()->has_toolbar_contents_for_testing());
  EXPECT_FALSE(prerenderer()->is_refill_pending_for_testing());
}

TEST_F(InitialWebUIProfileServiceRefillTest, DoesNotRefillWhileQuitting) {
  WaitForFirstSpare();
  std::unique_ptr<content::WebContents> taken =
      service()->TakeToolbarContents();
  ASSERT_TRUE(taken);
  browser_shutdown::SetTryingToQuit(true);
  task_environment()->FastForwardBy(kPaintDeadline + kRefillDelay);
  EXPECT_FALSE(service()->has_toolbar_contents_for_testing());
  EXPECT_FALSE(prerenderer()->is_refill_pending_for_testing());
  browser_shutdown::SetTryingToQuit(false);
}

TEST_F(InitialWebUIProfileServiceRefillTest, AdoptsFreshSpare) {
  std::unique_ptr<content::WebContents> spare = service()->CreateSpare();
  ASSERT_TRUE(spare);
  EXPECT_TRUE(service()->IsSpareCurrent(*spare));
}

TEST_F(InitialWebUIProfileServiceRefillTest,
       CreatesNoSpareWithoutNormalBrowser) {
  CloseBrowser();
  EXPECT_FALSE(service()->CreateSpare());
}

TEST_F(InitialWebUIProfileServiceRefillTest,
       DropsStaleSpareAfterHomeButtonChange) {
  std::unique_ptr<content::WebContents> spare = service()->CreateSpare();
  ASSERT_TRUE(spare);
  profile()->GetPrefs()->SetBoolean(
      prefs::kShowHomeButton,
      !profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_FALSE(service()->IsSpareCurrent(*spare));
}

TEST_F(InitialWebUIProfileServiceRefillTest,
       DropsStaleSpareWhenColorModeDiffers) {
  auto inputs = service()->GetToolbarInputsForTesting();
  inputs.color_provider_key.color_mode =
      inputs.color_provider_key.color_mode ==
              ui::ColorProviderKey::ColorMode::kLight
          ? ui::ColorProviderKey::ColorMode::kDark
          : ui::ColorProviderKey::ColorMode::kLight;
  service()->set_spare_inputs_for_testing(inputs);
  std::unique_ptr<content::WebContents> spare = CreateTestContents();
  EXPECT_FALSE(service()->IsSpareCurrent(*spare));
}

TEST(InitialWebUIProfileServiceTest, PrerenderingRequiresFeaturesAndParams) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kTopChromeWebUIPrerendering, {}},
         {features::kWebUIReloadButton,
          {{"WebUIReloadButtonPrewarmWebUI", "true"},
           {"WebUIReloadButtonProfilePrewarming", "true"}}}},
        {});
    EXPECT_TRUE(InitialWebUIProfileService::IsPrerenderingEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kTopChromeWebUIPrerendering, {}}},
        {features::kWebUIReloadButton});
    EXPECT_FALSE(InitialWebUIProfileService::IsPrerenderingEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kTopChromeWebUIPrerendering, {}},
         {features::kWebUIReloadButton,
          {{"WebUIReloadButtonPrewarmWebUI", "false"},
           {"WebUIReloadButtonProfilePrewarming", "true"}}}},
        {});
    EXPECT_FALSE(InitialWebUIProfileService::IsPrerenderingEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kTopChromeWebUIPrerendering, {}},
         {features::kWebUIReloadButton,
          {{"WebUIReloadButtonPrewarmWebUI", "true"},
           {"WebUIReloadButtonProfilePrewarming", "false"}}}},
        {});
    EXPECT_FALSE(InitialWebUIProfileService::IsPrerenderingEnabled());
  }
}

class InitialWebUIProfileServiceNoRefillTest
    : public ChromeRenderViewHostTestHarness {
 public:
  InitialWebUIProfileServiceNoRefillTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kWebUIReloadButton,
          {{"WebUIReloadButtonPrewarmWebUI", "true"},
           {"WebUIReloadButtonPrewarmWebUIPreNavigate", "false"},
           {"WebUIReloadButtonProfilePrewarming", "true"}}}},
        {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    service_ = InitialWebUIProfileServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(service_);
  }

  void TearDown() override {
    if (service_) {
      service_->TakeToolbarContents();
    }
    service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  InitialWebUIProfileService* service() { return service_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<InitialWebUIProfileService> service_ = nullptr;
};

TEST_F(InitialWebUIProfileServiceNoRefillTest,
       ConstructorPrewarmsWithRefillOff) {
  // With prerendering off, the constructor still prewarms a toolbar at profile
  // creation, and taking it leaves no spare and schedules no refill.
  EXPECT_TRUE(service()->has_toolbar_contents_for_testing());
  std::unique_ptr<content::WebContents> contents =
      service()->TakeToolbarContents();
  EXPECT_TRUE(contents);
  EXPECT_FALSE(service()->has_toolbar_contents_for_testing());
  EXPECT_FALSE(service()->toolbar_prerenderer_for_testing());
  EXPECT_FALSE(service()->TakeToolbarContents());
}

class InitialWebUIProfileServiceParamsTest : public testing::Test {
 protected:
  void InitFeatures(const std::string& memory_pressure) {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kWebUIReloadButton,
          {{"WebUIReloadButtonPrewarmWebUI", "true"},
           {"WebUIReloadButtonPrewarmWebUIPreNavigate", "false"},
           {"WebUIReloadButtonProfilePrewarming", "true"}}},
         {features::kTopChromeWebUIPrerendering,
          {{"TopChromeWebUIPrerenderingDelay", "500ms"},
           {"TopChromeWebUIPrerenderingPaintDeadline", "4s"},
           {"TopChromeWebUIPrerenderingMemoryPressure", memory_pressure}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(InitialWebUIProfileServiceParamsTest, ReadsCritical) {
  InitFeatures("critical");
  WebUIContentsPrerenderer::Params params =
      InitialWebUIProfileService::GetPrerenderParamsForTesting();
  EXPECT_EQ(base::Seconds(4), params.paint_deadline);
  EXPECT_EQ(base::Milliseconds(500), params.delay);
  EXPECT_EQ(base::MemoryLimit::CriticalPressureThreshold(),
            params.skip_memory_limit);
  EXPECT_EQ("InitialWebUIProfileService", params.consumer_name);
}

TEST_F(InitialWebUIProfileServiceParamsTest, ReadsModerate) {
  InitFeatures("moderate");
  EXPECT_EQ(base::MemoryLimit::ModeratePressureThreshold(),
            InitialWebUIProfileService::GetPrerenderParamsForTesting()
                .skip_memory_limit);
}

}  // namespace
