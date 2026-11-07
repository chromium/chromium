// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/horizontal_tab_strip_region_view.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/user_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/test/base/scoped_channel_override.h"
#endif

class LayoutWaiter : public views::ViewObserver {
 public:
  explicit LayoutWaiter(views::View* view) : view_(view) {
    view_->AddObserver(this);
  }
  ~LayoutWaiter() override {
    if (view_) {
      view_->RemoveObserver(this);
    }
  }

  void Wait() {
    // The view can be null if it's deleted before Wait() is called.
    if (!view_) {
      return;
    }
    run_loop_.Run();
  }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override {
    // Quit the run loop when the observed view's bounds change.
    run_loop_.Quit();
  }

  void OnViewIsDeleting(views::View* observed_view) override {
    // If the view is deleted while we're waiting, stop observing and
    // ensure we don't try to access it later.
    view_ = nullptr;
    run_loop_.Quit();
  }

 private:
  raw_ptr<views::View> view_ = nullptr;
  base::RunLoop run_loop_;
};

// HorizontalTabStripRegionViewTestBase contains no test cases.
class HorizontalTabStripRegionViewTest : public InProcessBrowserTest {
 public:
  HorizontalTabStripRegionViewTest()
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {
    feature_list_.InitAndDisableFeature(tabs::kTabStripUnification);
  }
  HorizontalTabStripRegionViewTest(const HorizontalTabStripRegionViewTest&) = delete;
  HorizontalTabStripRegionViewTest& operator=(const HorizontalTabStripRegionViewTest&) = delete;
  ~HorizontalTabStripRegionViewTest() override = default;

  void SetUp() override {
    InProcessBrowserTest::SetUp();

    // Prevent hover cards from appearing when the mouse is over the tab. Tests
    // don't typically account for this possibly, so it can cause unrelated
    // tests to fail due to tab data not being set. See crbug.com/40672885.
    Tab::SetShowHoverCardOnMouseHoverForTesting(false);
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    Tab::SetShowHoverCardOnMouseHoverForTesting(true);
  }

 protected:
  HorizontalTabStripRegionView* tab_strip_region_view() {
    return views::AsViewClass<HorizontalTabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  }

  TabStrip* tab_strip() { return tab_strip_region_view()->tab_strip(); }

  TabStripModel* tab_strip_model() { return browser()->GetTabStripModel(); }

  views::View* new_tab_button() {
    return BrowserElementsViews::From(browser())->GetViewAs<views::View>(
        kNewTabButtonElementId);
  }

  views::View* reserved_grab_handle_space() {
    return BrowserElementsViews::From(browser())->GetViewAs<views::View>(
        kTabStripFrameGrabHandleElementId);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
};

// TODO(crbug.com/41493572): Skip for now due to test failing when CR2023
// enabled.
IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewTest,
                       DISABLED_GrabHandleSpaceStaysVisible) {
  const int kTabStripRegionViewWidth = 500;
  tab_strip_region_view()->SetBounds(0, 0, kTabStripRegionViewWidth, 20);

  for (int i = 0; i < 100; ++i) {
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, i == 0);
    {
      LayoutWaiter waiter(
          tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
      RunScheduledLayouts();
      waiter.Wait();
    }
    EXPECT_LE(reserved_grab_handle_space()->bounds().right(),
              kTabStripRegionViewWidth);
  }
}

// TODO(crbug.com/41493572): Skip for now due to test failing when CR2023
// enabled.
IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewTest,
                       DISABLED_NewTabButtonStaysVisible) {
  const int kTabStripRegionViewWidth = 500;
  tab_strip_region_view()->SetBounds(0, 0, kTabStripRegionViewWidth, 20);

  for (int i = 0; i < 100; ++i) {
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, i == 0);
    {
      LayoutWaiter waiter(
          tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
      RunScheduledLayouts();
      waiter.Wait();
    }
    EXPECT_LE(new_tab_button()->bounds().right(), kTabStripRegionViewWidth);
  }
}

// TODO(crbug.com/41493572): Skip for now due to test failing when CR2023
// enabled.
IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewTest,
                       DISABLED_NewTabButtonRightOfTabs) {
  const int kTabStripRegionViewWidth = 500;
  tab_strip_region_view()->SetBounds(0, 0, kTabStripRegionViewWidth, 20);

  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);

  {
    LayoutWaiter waiter(tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
    RunScheduledLayouts();
    waiter.Wait();
  }

  EXPECT_EQ(new_tab_button()->bounds().x(),
            tab_strip()->tab_at(0)->bounds().right());
}

// TODO(crbug.com/41496209): Skip for now due to test failing when CR2023
// enabled.
IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewTest, DISABLED_NewTabButtonInkDrop) {
  constexpr int kTabStripRegionViewWidth = 500;
  tab_strip_region_view()->SetBounds(
      0, 0, kTabStripRegionViewWidth,
      GetLayoutConstant(LayoutConstant::kTabStripHeight));

  // Add a few tabs and simulate the new tab button's ink drop animation. This
  // should not cause any crashes since the ink drop layer size as well as the
  // ink drop container size should remain equal to the new tab button visible
  // bounds size. https://crbug.com/40563841.
  auto* button = static_cast<TabStripControlButton*>(new_tab_button());
  for (int i = 0; i < 10; ++i) {
    button->AnimateToStateForTesting(views::InkDropState::ACTION_TRIGGERED);
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
    {
      LayoutWaiter waiter(
          tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
      RunScheduledLayouts();
      waiter.Wait();
    }
    button->AnimateToStateForTesting(views::InkDropState::HIDDEN);
  }
}

// We want to make sure that the following children views sits flush with the
// top of tab strip region view:
// * tab strip
// * new tab button
// This is important in ensuring that we maximise the targetable area of these
// views when the tab strip is flush with the top of the screen when the window
// is maximized (Fitt's Law).
IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewTest,
                       ChildrenAreFlushWithTopOfTabStripRegionView) {
  tab_strip_region_view()->SetBounds(0, 0, 1000, 100);
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);

  {
    LayoutWaiter waiter(tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
    RunScheduledLayouts();
    waiter.Wait();
  }

  // The tab strip should sit flush with the top of the
  // |tab_strip_region_view()|.
  gfx::Point tab_strip_origin(tab_strip()->bounds().origin());
  views::View::ConvertPointToTarget(tab_strip(), tab_strip_region_view(),
                                    &tab_strip_origin);
  EXPECT_EQ(0, tab_strip_origin.y());

  // The new tab button should sit flush with the top of the
  // |tab_strip_region_view()|.
  gfx::Point new_tab_button_origin(new_tab_button()->bounds().origin());
  views::View::ConvertPointToTarget(tab_strip(), tab_strip_region_view(),
                                    &new_tab_button_origin);
  EXPECT_EQ(0, new_tab_button_origin.y());
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewTest, HasPaneRole) {
  ui::AXNodeData ax_node_data;
  tab_strip_region_view()->GetViewAccessibility().GetAccessibleNodeData(
      &ax_node_data);
  EXPECT_EQ(ax::mojom::Role::kPane, ax_node_data.role);
  EXPECT_FALSE(ax_node_data.HasState(ax::mojom::State::kMultiselectable));
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewTest,
                       IncognitoLeadingButtonsCheckDoesntCrash) {
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  HorizontalTabStripRegionView* incognito_tab_strip_region_view =
      views::AsViewClass<HorizontalTabStripRegionView>(
          BrowserView::GetBrowserViewForBrowser(incognito_browser)
              ->tab_strip_view());
  // This should not crash.
  incognito_tab_strip_region_view->HasLeadingButtons();
}

// When scrolling is disabled, the tab strip cannot be larger than the container
// so tabs that do not fit in the tabstrip will become invisible. This is the
// opposite behavior from
// HorizontalTabStripRegionViewTestWithScrollingEnabled.TabStripCanBeLargerThanContainer.
IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewTest,
                       TabStripCannotBeLargerThanContainer) {
  const int minimum_active_width = TabStyle::Get()->GetMinimumInactiveWidth();
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  tab_strip()->StopAnimating();
  RunScheduledLayouts();

  // Add tabs to the tabstrip until it is full.
  while (tab_strip()->tab_at(0)->width() > minimum_active_width) {
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
    tab_strip()->StopAnimating();
    RunScheduledLayouts();
    EXPECT_LT(tab_strip()->width(), tab_strip_region_view()->width());
  }

  // Add a few more tabs after the tabstrip is full to ensure tabs added
  // afterwards are not visible.
  for (int i = 0; i < 10; i++) {
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
    tab_strip()->StopAnimating();
    RunScheduledLayouts();
  }
  EXPECT_LT(tab_strip()->width(), tab_strip_region_view()->width());
  EXPECT_FALSE(
      tab_strip()->tab_at(tab_strip()->GetModelCount() - 1)->GetVisible());
}

class HorizontalTabStripRegionViewNewTest : public InProcessBrowserTest {
 public:
  HorizontalTabStripRegionViewNewTest()
      : glic_test_env_({}, GetEnabledFeatures(), GetDisabledFeatures()) {}
  HorizontalTabStripRegionViewNewTest(
      const HorizontalTabStripRegionViewNewTest&) = delete;
  HorizontalTabStripRegionViewNewTest& operator=(
      const HorizontalTabStripRegionViewNewTest&) = delete;
  ~HorizontalTabStripRegionViewNewTest() override = default;

 protected:
  HorizontalTabStripRegionViewNew* tab_strip_region_view() {
    return views::AsViewClass<HorizontalTabStripRegionViewNew>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  }

  views::View* tab_strip_action_container() {
    return BrowserElementsViews::From(browser())->GetViewAs<views::View>(
        kTabStripActionContainerElementId);
  }

  views::View* reserved_grab_handle_space() {
    return BrowserElementsViews::From(browser())->GetViewAs<views::View>(
        kTabStripFrameGrabHandleElementId);
  }

 private:
  static std::vector<base::test::FeatureRef> GetEnabledFeatures() {
    std::vector<base::test::FeatureRef> enabled =
        glic::GetDefaultEnabledGlicTestFeatures();
    enabled.push_back(tabs::kTabStripUnification);
    return enabled;
  }

  static std::vector<base::test::FeatureRef> GetDisabledFeatures() {
    std::vector<base::test::FeatureRef> disabled =
        glic::GetDefaultDisabledGlicTestFeatures();
    disabled.push_back(features::kGlicHorizontalTabToolbarButton);
    return disabled;
  }

  glic::GlicTestEnvironment glic_test_env_;
};

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewNewTest,
                       TabStripActionContainerIsNotInWindowCaption) {
  RunScheduledLayouts();
  HorizontalTabStripRegionViewNew* const region_view = tab_strip_region_view();
  views::View* const action_container = tab_strip_action_container();
  views::View* const grab_handle = reserved_grab_handle_space();
  ASSERT_NE(region_view, nullptr);
  ASSERT_NE(action_container, nullptr);
  ASSERT_NE(grab_handle, nullptr);
  ASSERT_TRUE(action_container->GetVisible());
  ASSERT_FALSE(action_container->bounds().IsEmpty());
  gfx::Point action_container_center =
      action_container->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToTarget(action_container, region_view,
                                    &action_container_center);
  gfx::Point grab_handle_center = grab_handle->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToTarget(grab_handle, region_view,
                                    &grab_handle_center);

  const bool is_action_container_in_caption =
      region_view->IsPositionInWindowCaption(action_container_center);
  const bool is_grab_handle_in_caption =
      region_view->IsPositionInWindowCaption(grab_handle_center);

  EXPECT_FALSE(is_action_container_in_caption);
  EXPECT_TRUE(is_grab_handle_in_caption);
}

class HorizontalTabStripRegionViewUnifiedTest : public InProcessBrowserTest {
 public:
  HorizontalTabStripRegionViewUnifiedTest() {
    feature_list_.InitAndEnableFeature(tabs::kTabStripUnification);
  }
  HorizontalTabStripRegionViewUnifiedTest(
      const HorizontalTabStripRegionViewUnifiedTest&) = delete;
  HorizontalTabStripRegionViewUnifiedTest& operator=(
      const HorizontalTabStripRegionViewUnifiedTest&) = delete;
  ~HorizontalTabStripRegionViewUnifiedTest() override = default;

 protected:
  views::Button* new_tab_button() {
    return BrowserElementsViews::From(browser())->GetViewAs<views::Button>(
        kNewTabButtonElementId);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewUnifiedTest,
                       RecordsNewTabButtonUserAction) {
  base::UserActionTester user_action_tester;
  ASSERT_NE(new_tab_button(), nullptr);

  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(5, 5),
                             gfx::Point(5, 5), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  views::test::ButtonTestApi(new_tab_button()).NotifyClick(press_event);

  EXPECT_EQ(2, browser()->GetTabStripModel()->count());
  EXPECT_EQ(1, user_action_tester.GetActionCount("NewTab_Button"));
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewUnifiedTest,
                       RecordsTimeToCreateNewTabFromPress) {
  base::HistogramTester histogram_tester;
  ASSERT_NE(new_tab_button(), nullptr);

  // Adding a tab without pressing the button should not record the metric.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  histogram_tester.ExpectTotalCount("TabStrip.TimeToCreateNewTabFromPress", 0);

  // Pressing the new tab button should record the creation latency once.
  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(5, 5),
                             gfx::Point(5, 5), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  views::test::ButtonTestApi(new_tab_button()).NotifyClick(press_event);
  histogram_tester.ExpectTotalCount("TabStrip.TimeToCreateNewTabFromPress", 1);
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewUnifiedTest,
                       RecordsNewTabHistogramBucket) {
  base::HistogramTester histogram_tester;
  ASSERT_NE(new_tab_button(), nullptr);

  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(5, 5),
                             gfx::Point(5, 5), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  views::test::ButtonTestApi(new_tab_button()).NotifyClick(press_event);

  histogram_tester.ExpectBucketCount("Tab.NewTab", NewTabTypes::kNewTabButton,
                                     1);
  histogram_tester.ExpectBucketCount("Tab.NewTab", NewTabTypes::kNewTabCommand,
                                     0);

  actions::ActionItem* new_tab_action =
      actions::ActionManager::Get().FindAction(
          kActionNewTab, BrowserActions::From(browser())->root_action_item());
  ASSERT_NE(new_tab_action, nullptr);
  new_tab_action->InvokeAction();
  histogram_tester.ExpectBucketCount("Tab.NewTab", NewTabTypes::kNewTabButton,
                                     1);
  histogram_tester.ExpectBucketCount("Tab.NewTab", NewTabTypes::kNewTabCommand,
                                     1);
}

class HorizontalTabStripRegionViewUnifiedGuestTest
    : public HorizontalTabStripRegionViewUnifiedTest {
 public:
  HorizontalTabStripRegionViewUnifiedGuestTest() = default;
  HorizontalTabStripRegionViewUnifiedGuestTest(
      const HorizontalTabStripRegionViewUnifiedGuestTest&) = delete;
  HorizontalTabStripRegionViewUnifiedGuestTest& operator=(
      const HorizontalTabStripRegionViewUnifiedGuestTest&) = delete;
  ~HorizontalTabStripRegionViewUnifiedGuestTest() override = default;

#if BUILDFLAG(IS_CHROMEOS)
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HorizontalTabStripRegionViewUnifiedTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    user_manager::kGuestUserName);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
    command_line->AppendSwitch(switches::kIncognito);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
};

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewUnifiedGuestTest,
                       RecordsTabCountGuest) {
#if BUILDFLAG(IS_CHROMEOS)
  BrowserWindowInterface* guest_browser = browser();
#else
  BrowserWindowInterface* guest_browser = CreateGuestBrowser();
#endif  // BUILDFLAG(IS_CHROMEOS)
  ASSERT_NE(guest_browser, nullptr);
  ASSERT_TRUE(guest_browser->GetProfile()->IsGuestSession());

  base::HistogramTester histogram_tester;
  chrome::AddTabAt(guest_browser, GURL("about:blank"), -1, true);
  histogram_tester.ExpectUniqueSample("Tab.Count.Guest", 2, 1);
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewUnifiedTest,
                       RecordsTabCountIncognito) {
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  ASSERT_NE(incognito_browser, nullptr);
  ASSERT_TRUE(incognito_browser->GetProfile()->IsIncognitoProfile());

  base::HistogramTester histogram_tester;
  chrome::AddTabAt(incognito_browser, GURL("about:blank"), -1, true);
  histogram_tester.ExpectUniqueSample("Tab.Count.Incognito", 2, 1);
}

class HorizontalTabStripRegionViewUnifiedIsolatedTest
    : public HorizontalTabStripRegionViewUnifiedTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HorizontalTabStripRegionViewUnifiedTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        enterprise_isolated_mode::switches::
            kForceEnterpriseIsolatedModeReplacesIncognito);
  }

 private:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
  chrome::ScopedChannelOverride channel_override_{
      chrome::ScopedChannelOverride::Channel::kDev};
#endif
};

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewUnifiedIsolatedTest,
                       RecordsTabCountIsolated) {
  BrowserWindowInterface* isolated_browser = CreateIncognitoBrowser();
  ASSERT_NE(isolated_browser, nullptr);
  ASSERT_TRUE(
      isolated_browser->GetProfile()->IsEnterpriseIsolatedModeProfile());

  base::HistogramTester histogram_tester;
  chrome::AddTabAt(isolated_browser, GURL("about:blank"), -1, true);
  histogram_tester.ExpectUniqueSample("Tab.Count.Isolated", 2, 1);
}
