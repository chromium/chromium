// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_webui_profile_service.h"

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/waap/initial_webui_profile_service_factory.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_user_profile.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_prerenderer.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider_key.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/test/test_layout_provider.h"

namespace {

constexpr char kTakeResultHistogram[] =
    "InitialWebUI.Toolbar.ProfilePrewarmTakeResult";
using TakeResult = WebUIContentsPrerenderer::TakeResult;

// Not the 3 s default, so that the tests check that the param is read.
constexpr base::TimeDelta kPaintDeadline = base::Seconds(4);
constexpr base::TimeDelta kRefillDelay = base::Milliseconds(500);

// A mock browser window that the browser collections track.
struct TestBrowser {
  testing::NiceMock<MockBrowserWindowInterface> window;
  BrowserWindowInterface::DidBecomeActiveCallback activate_callback;
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
    EXPECT_FALSE(service_->TakeToolbarContents(*profile()));
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

// Without the evict param, the prerenderer keeps its spare under any memory
// pressure.
TEST_F(InitialWebUIProfileServiceRefillTest, DoesNotEvictByDefault) {
  EXPECT_TRUE(prerenderer()->IsPassive());
}

TEST_F(InitialWebUIProfileServiceRefillTest, RefillsAfterDeadlineAndDelay) {
  base::HistogramTester histogram_tester;
  // First window builds its own toolbar and schedules the first refill.
  EXPECT_FALSE(service()->TakeToolbarContents(*profile()));
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
      service()->TakeToolbarContents(*profile());
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
      service()->TakeToolbarContents(*profile());
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

TEST(InitialWebUIProfileServiceTest,
     UnboundPrerenderingRequiresSharedRendererAndPreNavigate) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kTopChromeWebUISharedRenderer, {}},
         {features::kTopChromeWebUIPrerendering,
          {{"TopChromeWebUIPrerenderingUnbound", "true"}}},
         {features::kWebUIReloadButton,
          {{"WebUIReloadButtonPrewarmWebUI", "true"},
           {"WebUIReloadButtonPrewarmWebUIPreNavigate", "true"},
           {"WebUIReloadButtonProfilePrewarming", "true"}}}},
        {});
    EXPECT_TRUE(InitialWebUIProfileService::IsUnboundPrerenderingEnabled());
  }
  // Without PreNavigate, falls back to Phase 1.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kTopChromeWebUISharedRenderer, {}},
         {features::kTopChromeWebUIPrerendering,
          {{"TopChromeWebUIPrerenderingUnbound", "true"}}},
         {features::kWebUIReloadButton,
          {{"WebUIReloadButtonPrewarmWebUI", "true"},
           {"WebUIReloadButtonPrewarmWebUIPreNavigate", "false"},
           {"WebUIReloadButtonProfilePrewarming", "true"}}}},
        {});
    EXPECT_TRUE(InitialWebUIProfileService::IsPrerenderingEnabled());
    EXPECT_FALSE(InitialWebUIProfileService::IsUnboundPrerenderingEnabled());
  }
  // Without TopChromeWebUISharedRenderer, falls back to Phase 1.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kTopChromeWebUIPrerendering,
          {{"TopChromeWebUIPrerenderingUnbound", "true"}}},
         {features::kWebUIReloadButton,
          {{"WebUIReloadButtonPrewarmWebUI", "true"},
           {"WebUIReloadButtonPrewarmWebUIPreNavigate", "true"},
           {"WebUIReloadButtonProfilePrewarming", "true"}}}},
        {features::kTopChromeWebUISharedRenderer});
    EXPECT_TRUE(InitialWebUIProfileService::IsPrerenderingEnabled());
    EXPECT_FALSE(InitialWebUIProfileService::IsUnboundPrerenderingEnabled());
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
      service_->TakeToolbarContents(*profile());
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
      service()->TakeToolbarContents(*profile());
  EXPECT_TRUE(contents);
  EXPECT_FALSE(service()->has_toolbar_contents_for_testing());
  EXPECT_FALSE(service()->toolbar_prerenderer_for_testing());
  EXPECT_FALSE(service()->TakeToolbarContents(*profile()));
}

class InitialWebUIProfileServiceParamsTest : public testing::Test {
 protected:
  void InitFeatures(const std::string& skip_memory_pressure,
                    const std::string& evict_memory_pressure) {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kWebUIReloadButton,
          {{"WebUIReloadButtonPrewarmWebUI", "true"},
           {"WebUIReloadButtonPrewarmWebUIPreNavigate", "false"},
           {"WebUIReloadButtonProfilePrewarming", "true"}}},
         {features::kTopChromeWebUIPrerendering,
          {{"TopChromeWebUIPrerenderingDelay", "500ms"},
           {"TopChromeWebUIPrerenderingPaintDeadline", "4s"},
           {"TopChromeWebUIPrerenderingSkipMemoryPressure",
            skip_memory_pressure},
           {"TopChromeWebUIPrerenderingEvictMemoryPressure",
            evict_memory_pressure}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(InitialWebUIProfileServiceParamsTest, ReadsCritical) {
  InitFeatures("critical", "moderate");
  WebUIContentsPrerenderer::Params params =
      InitialWebUIProfileService::GetPrerenderParamsForTesting();
  EXPECT_EQ(base::Seconds(4), params.paint_deadline);
  EXPECT_EQ(base::Milliseconds(500), params.delay);
  EXPECT_EQ(base::MemoryLimit::CriticalPressureThreshold(),
            params.skip_memory_limit);
  EXPECT_EQ(base::MemoryLimit::ModeratePressureThreshold(),
            params.evict_memory_limit);
  EXPECT_EQ("InitialWebUIProfileService", params.consumer_name);
}

TEST_F(InitialWebUIProfileServiceParamsTest, ReadsModerate) {
  InitFeatures("moderate", "never");
  WebUIContentsPrerenderer::Params params =
      InitialWebUIProfileService::GetPrerenderParamsForTesting();
  EXPECT_EQ(base::MemoryLimit::ModeratePressureThreshold(),
            params.skip_memory_limit);
  EXPECT_FALSE(params.evict_memory_limit.has_value());
}

class InitialWebUIProfileServiceUnboundTest
    : public ChromeRenderViewHostTestHarness {
 public:
  explicit InitialWebUIProfileServiceUnboundTest(
      std::string evict_memory_pressure = "never")
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kTopChromeWebUISharedRenderer, {}},
         {features::kThemeChangeOptimization, {}},
         {features::kWebUIReloadButton,
          {{"WebUIReloadButtonPrewarmWebUI", "true"},
           {"WebUIReloadButtonPrewarmWebUIPreNavigate", "true"},
           {"WebUIReloadButtonProfilePrewarming", "true"}}},
         {features::kTopChromeWebUIPrerendering,
          {{"TopChromeWebUIPrerenderingDelay", "500ms"},
           {"TopChromeWebUIPrerenderingPaintDeadline", "4s"},
           {"TopChromeWebUIPrerenderingEvictMemoryPressure",
            std::move(evict_memory_pressure)},
           {"TopChromeWebUIPrerenderingUnbound", "true"}}}},
        {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    EXPECT_FALSE(InitialWebUIProfileService::GetForTopChromeWebUIProfile());
    scoped_top_chrome_profile_ =
        std::make_unique<webui::ScopedTopChromeWebUIProfileForTesting>();
    AddBrowser(browser_, profile(), BrowserWindowInterface::TYPE_NORMAL);
    ASSERT_TRUE(service());
    ASSERT_TRUE(prerenderer());
    ASSERT_FALSE(service()->has_toolbar_contents_for_testing());
    ASSERT_FALSE(prerenderer()->is_refill_pending_for_testing());
  }

  void TearDown() override {
    CloseBrowser(other_browser_);
    CloseBrowser(browser_);
    scoped_top_chrome_profile_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void AddBrowser(TestBrowser& browser,
                  Profile* browser_profile,
                  BrowserWindowInterface::Type type) {
    ON_CALL(browser.window, RegisterDidBecomeActive(testing::_))
        .WillByDefault(
            [&browser](
                BrowserWindowInterface::DidBecomeActiveCallback callback) {
              browser.activate_callback = std::move(callback);
              return base::CallbackListSubscription();
            });
    ON_CALL(browser.window, RegisterBrowserDidClose(testing::_))
        .WillByDefault(
            [&browser](
                BrowserWindowInterface::BrowserDidCloseCallback callback) {
              browser.close_callback = std::move(callback);
              return base::CallbackListSubscription();
            });
    ON_CALL(browser.window, GetType()).WillByDefault(testing::Return(type));
    ON_CALL(browser.window, GetProfile())
        .WillByDefault(testing::Return(browser_profile));
    BrowserManagerServiceFactory::GetForProfile(browser_profile)
        ->AddBrowserForTesting(&browser.window);
  }

  void ActivateBrowser(TestBrowser& browser) {
    ASSERT_TRUE(browser.activate_callback);
    browser.activate_callback.Run(&browser.window);
  }

  void CloseBrowser(TestBrowser& browser) {
    if (browser.close_callback) {
      std::move(browser.close_callback).Run(&browser.window);
    }
  }

  void WaitForFirstSpare() {
    ASSERT_FALSE(service()->has_toolbar_contents_for_testing());
    EXPECT_FALSE(service()->TakeToolbarContents(*profile()));
    EXPECT_TRUE(prerenderer()->is_refill_pending_for_testing());
    task_environment()->FastForwardBy(kPaintDeadline + kRefillDelay);
    ASSERT_TRUE(service()->has_toolbar_contents_for_testing());
    EXPECT_FALSE(prerenderer()->is_refill_pending_for_testing());
  }

  InitialWebUIProfileService* service() {
    return InitialWebUIProfileService::GetForTopChromeWebUIProfile();
  }

  WebUIContentsPrerenderer* prerenderer() {
    return service()->toolbar_prerenderer_for_testing();
  }

  TestBrowser browser_;
  TestBrowser other_browser_;
  std::unique_ptr<webui::ScopedTopChromeWebUIProfileForTesting>
      scoped_top_chrome_profile_;

 private:
  views::test::TestLayoutProvider layout_provider_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(InitialWebUIProfileServiceUnboundTest,
       EnabledSkipsPerProfileServiceAndBindsOnTake) {
  ASSERT_TRUE(InitialWebUIProfileService::IsUnboundPrerenderingEnabled());
  EXPECT_FALSE(InitialWebUIProfileServiceFactory::GetForProfile(profile()));

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(service()->TakeToolbarContents(*profile()));
  histogram_tester.ExpectUniqueSample(kTakeResultHistogram,
                                      TakeResult::kNoSpare, 1);

  task_environment()->FastForwardBy(kPaintDeadline + kRefillDelay);
  content::WebContents* spare = service()->toolbar_web_contents_for_testing();
  ASSERT_TRUE(spare);
  EXPECT_EQ(webui::GetTopChromeWebUIProfile(), spare->GetBrowserContext());
  EXPECT_FALSE(webui::GetUserProfile(spare));
  EXPECT_FALSE(PrefsTabHelper::FromWebContents(spare));
  EXPECT_TRUE(WebUIToolbarUnboundSeedUserData::FromWebContents(spare));

  // Take for a regular profile binds that profile and removes the seed data.
  std::unique_ptr<content::WebContents> taken_regular =
      service()->TakeToolbarContents(*profile());
  ASSERT_EQ(spare, taken_regular.get());
  EXPECT_EQ(profile(), webui::GetUserProfile(taken_regular.get()));
  EXPECT_TRUE(PrefsTabHelper::FromWebContents(taken_regular.get()));
  EXPECT_FALSE(
      WebUIToolbarUnboundSeedUserData::FromWebContents(taken_regular.get()));
  histogram_tester.ExpectBucketCount(kTakeResultHistogram, TakeResult::kAdopted,
                                     1);

  // Refills an unbound spare and serves an Incognito (OTR) profile too.
  task_environment()->FastForwardBy(kPaintDeadline + kRefillDelay);
  ASSERT_TRUE(service()->has_toolbar_contents_for_testing());
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  std::unique_ptr<content::WebContents> taken_otr =
      service()->TakeToolbarContents(*otr_profile);
  ASSERT_TRUE(taken_otr);
  EXPECT_EQ(otr_profile, webui::GetUserProfile(taken_otr.get()));
  EXPECT_TRUE(PrefsTabHelper::FromWebContents(taken_otr.get()));
  histogram_tester.ExpectBucketCount(kTakeResultHistogram, TakeResult::kAdopted,
                                     2);
}

TEST_F(InitialWebUIProfileServiceUnboundTest,
       SeedsSpareFromLastActiveNormalBrowserOriginalProfile) {
  TestingProfile second_profile;
  second_profile.GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
  const ui::ColorProviderKey baseline_key =
      ui::NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(nullptr);
  const auto target_color_mode =
      baseline_key.color_mode == ui::ColorProviderKey::ColorMode::kDark
          ? ui::ColorProviderKey::ColorMode::kLight
          : ui::ColorProviderKey::ColorMode::kDark;
  ThemeServiceFactory::GetForProfile(&second_profile)
      ->SetBrowserColorScheme(target_color_mode ==
                                      ui::ColorProviderKey::ColorMode::kLight
                                  ? ThemeService::BrowserColorScheme::kLight
                                  : ThemeService::BrowserColorScheme::kDark);

  // Activate an OTR normal browser on `second_profile` (plus a popup browser on
  // `profile()` after it to confirm popups are ignored); `CreateSpare()` must
  // seed from `second_profile` (its original profile) without binding it.
  Profile* otr_second =
      second_profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  TestBrowser otr_browser;
  AddBrowser(otr_browser, otr_second, BrowserWindowInterface::TYPE_NORMAL);
  ActivateBrowser(otr_browser);
  TestBrowser popup_browser;
  AddBrowser(popup_browser, profile(), BrowserWindowInterface::TYPE_POPUP);
  ActivateBrowser(popup_browser);

  WaitForFirstSpare();
  content::WebContents* spare = service()->toolbar_web_contents_for_testing();
  ASSERT_TRUE(spare);
  EXPECT_FALSE(webui::GetUserProfile(spare));
  EXPECT_EQ(target_color_mode, spare->GetColorMode());
  const auto* seed_data =
      WebUIToolbarUnboundSeedUserData::FromWebContents(spare);
  ASSERT_TRUE(seed_data);
  EXPECT_EQ(WebUIToolbarUI::GetProfileButtonEligibility(&second_profile),
            seed_data->button_eligibility());
  EXPECT_TRUE(seed_data->home_button_should_be_shown());

  CloseBrowser(popup_browser);
  CloseBrowser(otr_browser);
}

TEST_F(InitialWebUIProfileServiceUnboundTest,
       TemplateProfileDestroyedBeforeTakeStillAdoptsAndBinds) {
  auto second_profile = std::make_unique<TestingProfile>();
  second_profile->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
  TestBrowser second_browser;
  AddBrowser(second_browser, second_profile.get(),
             BrowserWindowInterface::TYPE_NORMAL);
  ActivateBrowser(second_browser);

  WaitForFirstSpare();
  ASSERT_TRUE(service()->has_toolbar_contents_for_testing());

  // Close and destroy the template profile while `profile()` still has a normal
  // browser open. Taking from `profile()` must still adopt and bind cleanly.
  CloseBrowser(second_browser);
  second_profile.reset();
  ASSERT_TRUE(service()->has_toolbar_contents_for_testing());

  base::HistogramTester histogram_tester;
  std::unique_ptr<content::WebContents> taken =
      service()->TakeToolbarContents(*profile());
  ASSERT_TRUE(taken);
  histogram_tester.ExpectUniqueSample(kTakeResultHistogram,
                                      TakeResult::kAdopted, 1);
  EXPECT_EQ(profile(), webui::GetUserProfile(taken.get()));
  EXPECT_FALSE(WebUIToolbarUnboundSeedUserData::FromWebContents(taken.get()));
}

TEST_F(InitialWebUIProfileServiceUnboundTest,
       DropsSpareWhenLastNormalWindowAcrossProfilesCloses) {
  TestingProfile second_profile;
  TestBrowser second_profile_browser;
  TestBrowser popup_browser;
  AddBrowser(second_profile_browser, &second_profile,
             BrowserWindowInterface::TYPE_NORMAL);
  AddBrowser(popup_browser, profile(), BrowserWindowInterface::TYPE_POPUP);

  WaitForFirstSpare();
  prerenderer()->ScheduleRefill(nullptr);
  ASSERT_TRUE(service()->has_toolbar_contents_for_testing());
  ASSERT_TRUE(prerenderer()->is_refill_pending_for_testing());

  // Closing a popup keeps the spare and pending refill.
  CloseBrowser(popup_browser);
  EXPECT_TRUE(service()->has_toolbar_contents_for_testing());
  EXPECT_TRUE(prerenderer()->is_refill_pending_for_testing());

  // Closing the first profile's last window keeps the spare while the second
  // profile still has a normal window.
  CloseBrowser(browser_);
  EXPECT_TRUE(service()->has_toolbar_contents_for_testing());
  EXPECT_TRUE(prerenderer()->is_refill_pending_for_testing());

  // Closing the last normal window across all profiles drops both.
  CloseBrowser(second_profile_browser);
  EXPECT_FALSE(service()->has_toolbar_contents_for_testing());
  EXPECT_FALSE(prerenderer()->is_refill_pending_for_testing());
}

TEST_F(InitialWebUIProfileServiceUnboundTest,
       DestroysServiceBeforeTopChromeWebUIProfileShutdownAndReturnsNullAfter) {
  WaitForFirstSpare();
  content::WebContentsDestroyedWatcher spare_watcher(
      service()->toolbar_web_contents_for_testing());

  scoped_top_chrome_profile_.reset();
  EXPECT_TRUE(spare_watcher.IsDestroyed());
  EXPECT_FALSE(InitialWebUIProfileService::GetForTopChromeWebUIProfile());
}

TEST_F(InitialWebUIProfileServiceUnboundTest,
       IgnoresSystemThemeVersionBumpInBaselineKey) {
  WaitForFirstSpare();

  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  const size_t version_before =
      native_theme->GetColorProviderKey(nullptr).system_theme_version;
  native_theme->NotifyOnNativeThemeUpdated();
  ASSERT_EQ(version_before + 1,
            native_theme->GetColorProviderKey(nullptr).system_theme_version);

  base::HistogramTester histogram_tester;
  std::unique_ptr<content::WebContents> taken =
      service()->TakeToolbarContents(*profile());
  EXPECT_TRUE(taken);
  histogram_tester.ExpectUniqueSample(kTakeResultHistogram,
                                      TakeResult::kAdopted, 1);
}

TEST_F(InitialWebUIProfileServiceUnboundTest,
       DropsStaleSpareWhenNativeThemeColorModeOrTouchUiChanges) {
  WaitForFirstSpare();

  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  const auto original_scheme = native_theme->preferred_color_scheme();
  native_theme->set_preferred_color_scheme(
      original_scheme == ui::NativeTheme::PreferredColorScheme::kDark
          ? ui::NativeTheme::PreferredColorScheme::kLight
          : ui::NativeTheme::PreferredColorScheme::kDark);

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(service()->TakeToolbarContents(*profile()));
  histogram_tester.ExpectUniqueSample(kTakeResultHistogram,
                                      TakeResult::kDroppedStale, 1);
  native_theme->set_preferred_color_scheme(original_scheme);

  // Refill with restored theme, then flip touch UI mode.
  task_environment()->FastForwardBy(kPaintDeadline + kRefillDelay);
  ASSERT_TRUE(service()->has_toolbar_contents_for_testing());

  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper(
      !ui::TouchUiController::Get()->touch_ui());
  EXPECT_FALSE(service()->TakeToolbarContents(*profile()));
  histogram_tester.ExpectBucketCount(kTakeResultHistogram,
                                     TakeResult::kDroppedStale, 2);
}

class InitialWebUIProfileServiceUnboundEvictionTest
    : public InitialWebUIProfileServiceUnboundTest {
 public:
  InitialWebUIProfileServiceUnboundEvictionTest()
      : InitialWebUIProfileServiceUnboundTest("critical") {}

  void TearDown() override {
    test_memory_consumer_registry_.NotifyUpdateMemoryLimit(
        base::MemoryLimit::Default());
    InitialWebUIProfileServiceUnboundTest::TearDown();
  }

 protected:
  base::TestMemoryConsumerRegistry test_memory_consumer_registry_;
};

TEST_F(InitialWebUIProfileServiceUnboundEvictionTest,
       EvictsUnboundSpareAtMemoryLimit) {
  EXPECT_FALSE(prerenderer()->IsPassive());
  WaitForFirstSpare();
  content::WebContentsDestroyedWatcher spare_watcher(
      service()->toolbar_web_contents_for_testing());

  // Moderate pressure is above the critical evict limit, so the spare stays.
  test_memory_consumer_registry_.NotifyUpdateMemoryLimit(
      base::MemoryLimit::ModeratePressureThreshold());
  test_memory_consumer_registry_.NotifyReleaseMemory();
  EXPECT_TRUE(service()->has_toolbar_contents_for_testing());

  // Critical pressure evicts the unbound spare.
  test_memory_consumer_registry_.NotifyUpdateMemoryLimit(
      base::MemoryLimit::CriticalPressureThreshold());
  test_memory_consumer_registry_.NotifyReleaseMemory();
  EXPECT_FALSE(service()->has_toolbar_contents_for_testing());
  EXPECT_TRUE(spare_watcher.IsDestroyed());
}

}  // namespace
