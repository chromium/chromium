// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/tab_scrubber/tab_scrubber.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "ash/display/event_transformation_handler.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/test/scoped_icu_locale.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/split_tab_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/wm_helper.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view_utils.h"
#include "ui/wm/core/window_util.h"

namespace {

constexpr int kScrubbingGestureFingerCount = 3;

// Waits until the immersive mode reveal ends, and therefore the top view of
// the browser is no longer visible.
class ImmersiveRevealEndedWaiter : public ImmersiveModeController::Observer {
 public:
  explicit ImmersiveRevealEndedWaiter(
      ImmersiveModeController* immersive_controller)
      : immersive_controller_(immersive_controller) {
    immersive_controller_->AddObserver(this);
  }

  ImmersiveRevealEndedWaiter(const ImmersiveRevealEndedWaiter&) = delete;
  ImmersiveRevealEndedWaiter& operator=(const ImmersiveRevealEndedWaiter&) =
      delete;

  ~ImmersiveRevealEndedWaiter() override {
    if (immersive_controller_) {
      immersive_controller_->RemoveObserver(this);
    }
  }

  void Wait() {
    if (!immersive_controller_ || !immersive_controller_->IsRevealed()) {
      return;
    }

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void MaybeQuitRunLoop() {
    if (!quit_closure_.is_null()) {
      std::move(quit_closure_).Run();
    }
  }

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealEnded() override { MaybeQuitRunLoop(); }

  void OnImmersiveModeControllerDestroyed() override {
    MaybeQuitRunLoop();
    immersive_controller_->RemoveObserver(this);
    immersive_controller_ = nullptr;
  }

  raw_ptr<ImmersiveModeController> immersive_controller_;
  base::OnceClosure quit_closure_;
};

}  // namespace

namespace ash {

class TabScrubberTest : public InProcessBrowserTest,
                        public TabStripModelObserver,
                        public testing::WithParamInterface<bool> {
 public:
  TabScrubberTest() {
    scoped_feature_list_.InitWithFeatureState(tabs::kTabStripUnification,
                                              GetParam());
  }

  TabScrubberTest(const TabScrubberTest&) = delete;
  TabScrubberTest& operator=(const TabScrubberTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kNaturalScrollDefault);
  }

  void SetUpOnMainThread() override {
    TabScrubber::GetInstance()->use_default_activation_delay_ = false;
    // Disable external monitor scaling of coordinates.
    ash::Shell* shell = ash::Shell::Get();
    shell->event_transformation_handler()->set_transformation_mode(
        ash::EventTransformationHandler::TRANSFORM_NONE);

    wm_helper_ = std::make_unique<exo::WMHelper>();
  }

  void TearDownOnMainThread() override {
    wm_helper_.reset();

    browser()->GetTabStripModel()->RemoveObserver(this);
  }

  int GetStartX(BrowserWindowInterface* browser,
                int index,
                TabScrubber::Direction direction) {
    return TabScrubber::GetStartPoint(
               BrowserView::GetBrowserViewForBrowser(browser), index, direction)
        .x();
  }

  int GetTabCenter(BrowserWindowInterface* browser, int index) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    CHECK(browser_view);
    return TabScrubber::GetTabBoundsInStrip(browser_view, index)
        .CenterPoint()
        .x();
  }

  // The simulated scroll event's offsets are calculated in the tests rather
  // than generated by the real event system. For the offsets calculation to be
  // correct in an RTL layout, we must invert the direction since it is
  // calculated here in the tests based on the indices and they're inverted in
  // RTL layouts:
  // Tab indices in an English layout : 0 - 1 - 2 - 3 - 4.
  // Tab indices in an Arabic layout  : 4 - 3 - 2 - 1 - 0.
  TabScrubber::Direction InvertDirectionIfNeeded(
      TabScrubber::Direction direction) {
    if (base::i18n::IsRTL()) {
      return direction == TabScrubber::LEFT ? TabScrubber::RIGHT
                                            : TabScrubber::LEFT;
    }

    return direction;
  }

  // Sends one scroll event synchronously without initial or final
  // fling events.
  void SendScrubEvent(BrowserWindowInterface* browser, int index) {
    auto event_generator = CreateEventGenerator(browser);
    int active_index = browser->GetTabStripModel()->active_index();
    TabScrubber::Direction direction =
        index < active_index ? TabScrubber::LEFT : TabScrubber::RIGHT;

    direction = InvertDirectionIfNeeded(direction);

    int offset = GetTabCenter(browser, index) -
                 GetStartX(browser, active_index, direction);
    ui::ScrollEvent scroll_event(ui::EventType::kScroll, gfx::Point(0, 0),
                                 ui::EventTimeForNow(), 0, offset, 0, offset, 0,
                                 kScrubbingGestureFingerCount);
    event_generator->Dispatch(&scroll_event);
  }

  enum ScrubType {
    EACH_TAB,
    SKIP_TABS,
    REPEAT_TABS,
  };

  // Sends asynchronous events and waits for tab at |index| to become
  // active.
  void Scrub(BrowserWindowInterface* browser, int index, ScrubType scrub_type) {
    auto event_generator = CreateEventGenerator(browser);
    activation_order_.clear();
    int active_index = browser->GetTabStripModel()->active_index();
    ASSERT_NE(index, active_index);
    ASSERT_TRUE(scrub_type != SKIP_TABS || ((index - active_index) % 2) == 0);
    TabScrubber::Direction direction;
    int increment;
    if (index < active_index) {
      direction = TabScrubber::LEFT;
      increment = -1;
    } else {
      direction = TabScrubber::RIGHT;
      increment = 1;
    }

    direction = InvertDirectionIfNeeded(direction);

    if (scrub_type == SKIP_TABS) {
      increment *= 2;
    }
    browser->GetTabStripModel()->AddObserver(this);
    ScrollGenerator scroll_generator(event_generator.get());
    int last = GetStartX(browser, active_index, direction);
    for (int i = active_index + increment; i != (index + increment);
         i += increment) {
      int tab_center = GetTabCenter(browser, i);
      scroll_generator.GenerateScroll(tab_center - last);
      last = GetStartX(browser, i, direction);
      if (scrub_type == REPEAT_TABS) {
        scroll_generator.GenerateScroll(increment);
        last += increment;
      }
    }
    browser->GetTabStripModel()->RemoveObserver(this);
  }

  // Sends events and waits for tab at |index| to become active
  // if it's different from the currently active tab.
  // If the active tab is expected to stay the same, send events
  // synchronously (as we don't have anything to wait for).
  void SendScrubSequence(BrowserWindowInterface* browser,
                         float x_offset,
                         int index) {
    auto event_generator = CreateEventGenerator(browser);
    browser->GetTabStripModel()->AddObserver(this);
    ScrollGenerator scroll_generator(event_generator.get());
    scroll_generator.GenerateScroll(x_offset);
    browser->GetTabStripModel()->RemoveObserver(this);
  }

  // Sends alt-tab key press event to start the window cycle list.
  void StartCyclingWindows(BrowserWindowInterface* browser) {
    auto event_generator = CreateEventGenerator(browser);
    // Views use VKEY_MENU for both left and right Alt keys.
    event_generator->PressKey(ui::VKEY_MENU, ui::EF_NONE);
    event_generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_ALT_DOWN);
    event_generator->ReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_ALT_DOWN);
  }

  // Sends alt-tab key release event to start the window cycle list.
  void StopCyclingWindows(BrowserWindowInterface* browser) {
    auto event_generator = CreateEventGenerator(browser);
    event_generator->ReleaseKey(ui::VKEY_MENU, ui::EF_NONE);
  }

  bool IsTabScrubberEnabled() {
    return TabScrubber::GetInstance()->GetEnabledForTesting();
  }

  void FinishScrub(bool activate) {
    TabScrubber::GetInstance()->FinishScrub(activate);
  }

  void AddTabs(BrowserWindowInterface* browser, int num_tabs) {
    for (int i = 0; i < num_tabs; ++i) {
      AddBlankTabAndShow(browser);
    }
    ASSERT_EQ(num_tabs + 1, browser->GetTabStripModel()->count());
    ASSERT_EQ(num_tabs, browser->GetTabStripModel()->active_index());
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    CHECK(browser_view);
    // Perform any scheduled layouts so the tabstrip is in a steady state.
    browser_view->GetWidget()->LayoutRootViewIfNecessary();
  }

  // TabStripModelObserver overrides.
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (tab_strip_model->empty() || !selection.active_tab_changed()) {
      return;
    }

    ASSERT_TRUE(selection.new_model.active().has_value());
    activation_order_.push_back(selection.new_model.active().value());
  }

  std::unique_ptr<ui::test::EventGenerator> CreateEventGenerator(
      BrowserWindowInterface* browser) {
    aura::Window* window = browser->GetWindow()->GetNativeWindow();
    aura::Window* root = window->GetRootWindow();
    return std::make_unique<ui::test::EventGenerator>(root, window);
  }

  void VerifyUnifiedOverflowScrub() {
    ASSERT_TRUE(GetParam());
    browser()->GetWindow()->SetBounds(gfx::Rect(0, 0, 500, 400));
    AddTabs(browser(), 24);
    browser()->GetTabStripModel()->SetTabPinned(0, true);

    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    ASSERT_TRUE(browser_view);
    browser_view->GetWidget()->LayoutRootViewIfNecessary();
    browser_view->GetWidget()->LayoutRootViewIfNecessary();

    auto* region_view = views::AsViewClass<BaseTabStripRegionView>(
        browser_view->tab_strip_view());
    ASSERT_TRUE(region_view);
    auto* tab_strip_view =
        views::AsViewClass<TabStripView>(region_view->GetTabStripView());
    ASSERT_TRUE(tab_strip_view);
    views::ScrollView* unpinned_scroll_view =
        tab_strip_view->unpinned_tabs_scroll_view();
    ASSERT_TRUE(unpinned_scroll_view);
    ASSERT_TRUE(unpinned_scroll_view->IsHorizontalContentOverflowing());

    auto get_tab_view = [&](int index) {
      return views::AsViewClass<TabView>(region_view->GetTabAnchorView(
          browser()->GetTabStripModel()->GetTabAtIndex(index)->GetHandle()));
    };

    TabView* first_unpinned_tab = get_tab_view(1);
    TabView* last_unpinned_tab = get_tab_view(24);
    ASSERT_TRUE(first_unpinned_tab);
    ASSERT_TRUE(last_unpinned_tab);

    last_unpinned_tab->ScrollViewToVisible();
    ASSERT_EQ(24, browser()->GetTabStripModel()->active_index());
    EXPECT_TRUE(unpinned_scroll_view->GetVisibleRect().Contains(
        last_unpinned_tab->bounds()));
    EXPECT_FALSE(unpinned_scroll_view->GetVisibleRect().Contains(
        first_unpinned_tab->bounds()));

    // Scrub onto off-screen unpinned tab 1 without flinging yet and verify it
    // becomes hovered and scrolled into view.
    auto event_generator = CreateEventGenerator(browser());
    const int tab_count = browser()->GetTabStripModel()->count();
    const float scale = std::clamp(1.0f - tab_count * 0.02f, 0.25f, 1.0f);
    const TabScrubber::Direction direction =
        InvertDirectionIfNeeded(TabScrubber::LEFT);
    const float offset_to_tab_1 =
        (GetTabCenter(browser(), 1) - GetStartX(browser(), 24, direction)) /
        scale;
    ui::ScrollEvent scrub_to_tab_1(
        ui::EventType::kScroll, gfx::Point(0, 0), ui::EventTimeForNow(), 0,
        offset_to_tab_1, 0, offset_to_tab_1, 0, kScrubbingGestureFingerCount);
    event_generator->Dispatch(&scrub_to_tab_1);
    EXPECT_TRUE(TabScrubber::GetInstance()->IsActivationPending());
    EXPECT_EQ(1, TabScrubber::GetInstance()->highlighted_tab());
    EXPECT_TRUE(first_unpinned_tab->IsHovering());
    EXPECT_TRUE(unpinned_scroll_view->GetVisibleRect().Contains(
        first_unpinned_tab->bounds()));

    // Continue scrubbing onto pinned tab 0 across the scroll container boundary
    // and activate it.
    const float offset_to_pinned = base::i18n::IsRTL() ? 5000.0f : -5000.0f;
    ui::ScrollEvent scrub_to_pinned(
        ui::EventType::kScroll, gfx::Point(0, 0), ui::EventTimeForNow(), 0,
        offset_to_pinned, 0, offset_to_pinned, 0, kScrubbingGestureFingerCount);
    event_generator->Dispatch(&scrub_to_pinned);
    EXPECT_EQ(0, TabScrubber::GetInstance()->highlighted_tab());
    TabScrubber::GetInstance()->FinishScrub(true);
    EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());

    // Scrub all the way back to the off-screen trailing unpinned tab 24.
    SendScrubSequence(browser(), -offset_to_pinned * 2, 24);
    EXPECT_EQ(24, browser()->GetTabStripModel()->active_index());
    EXPECT_TRUE(unpinned_scroll_view->GetVisibleRect().Contains(
        last_unpinned_tab->bounds()));
  }

  // History of tab activation. Scrub() resets it.
  std::vector<size_t> activation_order_;

 private:
  // Used to generate a sequence of scrolls. Starts with a cancel, is followed
  // by any number of scrolls and finally a fling-start. After every event this
  // forces the TabScrubber to complete any pending activation.
  class ScrollGenerator {
   public:
    explicit ScrollGenerator(ui::test::EventGenerator* event_generator)
        : event_generator_(event_generator) {
      ui::ScrollEvent fling_cancel(ui::EventType::kScrollFlingCancel,
                                   gfx::Point(), time_for_next_event_, 0, 0, 0,
                                   0, 0, kScrubbingGestureFingerCount);
      event_generator->Dispatch(&fling_cancel);
      if (TabScrubber::GetInstance()->IsActivationPending()) {
        TabScrubber::GetInstance()->FinishScrub(true);
      }
    }

    ScrollGenerator(const ScrollGenerator&) = delete;
    ScrollGenerator& operator=(const ScrollGenerator&) = delete;

    ~ScrollGenerator() {
      ui::ScrollEvent fling_start(ui::EventType::kScrollFlingStart,
                                  gfx::Point(), time_for_next_event_, 0,
                                  last_x_offset_, 0, last_x_offset_, 0,
                                  kScrubbingGestureFingerCount);
      event_generator_->Dispatch(&fling_start);
      if (TabScrubber::GetInstance()->IsActivationPending()) {
        TabScrubber::GetInstance()->FinishScrub(true);
      }
    }

    void GenerateScroll(int x_offset) {
      time_for_next_event_ += base::Milliseconds(100);
      ui::ScrollEvent scroll(ui::EventType::kScroll, gfx::Point(),
                             time_for_next_event_, 0, x_offset, 0, x_offset, 0,
                             kScrubbingGestureFingerCount);
      last_x_offset_ = x_offset;
      event_generator_->Dispatch(&scroll);
      if (TabScrubber::GetInstance()->IsActivationPending()) {
        TabScrubber::GetInstance()->FinishScrub(true);
      }
    }

    raw_ptr<ui::test::EventGenerator> event_generator_;
    base::TimeTicks time_for_next_event_ = ui::EventTimeForNow();
    int last_x_offset_ = 0;
  };

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<exo::WMHelper> wm_helper_;

  const gfx::AnimationTestApi::RenderModeResetter disable_rich_animations_ =
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
};

// Swipe a single tab in each direction.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, Single) {
  base::HistogramTester histogram_tester;
  AddTabs(browser(), 1);

  Scrub(browser(), 0, EACH_TAB);
  EXPECT_THAT(activation_order_, testing::ElementsAre(0));
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());
  histogram_tester.ExpectUniqueSample("Tabs.ScrubDistance", 1, 1);
  histogram_tester.ExpectTotalCount("Tabs.ScrubDuration", 1);

  Scrub(browser(), 1, EACH_TAB);
  EXPECT_THAT(activation_order_, testing::ElementsAre(1));
  EXPECT_EQ(1, browser()->GetTabStripModel()->active_index());
  histogram_tester.ExpectUniqueSample("Tabs.ScrubDistance", 1, 2);
  histogram_tester.ExpectTotalCount("Tabs.ScrubDuration", 2);
}

// Swipe 4 tabs in each direction. Each of the tabs should become active.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, Multi) {
  AddTabs(browser(), 4);

  Scrub(browser(), 0, EACH_TAB);
  EXPECT_THAT(activation_order_, testing::ElementsAre(3, 2, 1, 0));
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());

  Scrub(browser(), 4, EACH_TAB);
  EXPECT_THAT(activation_order_, testing::ElementsAre(1, 2, 3, 4));
  EXPECT_EQ(4, browser()->GetTabStripModel()->active_index());
}

IN_PROC_BROWSER_TEST_P(TabScrubberTest, MultiBrowser) {
  AddTabs(browser(), 1);
  Scrub(browser(), 0, EACH_TAB);
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());

  BrowserWindowInterface* browser2 = CreateBrowser(browser()->GetProfile());
  browser2->GetWindow()->Activate();
  ASSERT_TRUE(browser2->GetWindow()->IsActive());
  ASSERT_FALSE(browser()->GetWindow()->IsActive());
  AddTabs(browser2, 1);

  Scrub(browser2, 0, EACH_TAB);
  EXPECT_EQ(0, browser2->GetTabStripModel()->active_index());
}

// Tests that tab scrubbing works correctly for a full-screen browser.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, FullScreenBrowser) {
  // Initializes the position of mouse. Makes the mouse away from the tabstrip
  // to prevent any interference on this test.
  auto event_generator = CreateEventGenerator(browser());
  event_generator->MoveMouseTo(
      gfx::Point(0, browser()->GetWindow()->GetBounds().height()));
  AddTabs(browser(), 6);
  browser()->GetTabStripModel()->ActivateTabAt(4);

  chrome::ToggleFullscreenMode(browser());
  auto* const immersive_controller = ImmersiveModeController::From(browser());
  EXPECT_TRUE(immersive_controller->IsEnabled());

  ImmersiveRevealEndedWaiter waiter(immersive_controller);
  waiter.Wait();

  EXPECT_FALSE(immersive_controller->IsRevealed());

  EXPECT_EQ(4, browser()->GetTabStripModel()->active_index());
  Scrub(browser(), 0, EACH_TAB);
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());
  EXPECT_THAT(activation_order_, testing::ElementsAre(3, 2, 1, 0));
}

// Swipe 4 tabs in each direction with an extra swipe within each. The same
// 4 tabs should become active.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, Repeated) {
  AddTabs(browser(), 4);

  Scrub(browser(), 0, REPEAT_TABS);
  EXPECT_THAT(activation_order_, testing::ElementsAre(3, 2, 1, 0));
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());

  Scrub(browser(), 4, REPEAT_TABS);
  EXPECT_THAT(activation_order_, testing::ElementsAre(1, 2, 3, 4));
  EXPECT_EQ(4, browser()->GetTabStripModel()->active_index());
}

// Confirm that we get the last tab made active when we skip tabs.
// These tests have 5 total tabs. We will only received scroll events
// on tabs 0, 2 and 4.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, Skipped) {
  base::HistogramTester histogram_tester;
  AddTabs(browser(), 4);

  Scrub(browser(), 0, SKIP_TABS);
  EXPECT_THAT(activation_order_, testing::ElementsAre(2, 0));
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());
  histogram_tester.ExpectUniqueSample("Tabs.ScrubDistance", 2, 2);
  histogram_tester.ExpectTotalCount("Tabs.ScrubDuration", 2);

  Scrub(browser(), 4, SKIP_TABS);
  EXPECT_THAT(activation_order_, testing::ElementsAre(2, 4));
  EXPECT_EQ(4, browser()->GetTabStripModel()->active_index());
  histogram_tester.ExpectUniqueSample("Tabs.ScrubDistance", 2, 4);
  histogram_tester.ExpectTotalCount("Tabs.ScrubDuration", 4);
}

// Confirm that nothing happens when the swipe is small.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, NoChange) {
  AddTabs(browser(), 1);

  SendScrubSequence(browser(), -1, 1);
  EXPECT_EQ(1, browser()->GetTabStripModel()->active_index());

  SendScrubSequence(browser(), 1, 1);
  EXPECT_EQ(1, browser()->GetTabStripModel()->active_index());
}

// Confirm that very large swipes go to the beginning and and of the tabstrip.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, Bounds) {
  AddTabs(browser(), 1);

  SendScrubSequence(browser(), -10000, 0);
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());

  SendScrubSequence(browser(), 10000, 1);
  EXPECT_EQ(1, browser()->GetTabStripModel()->active_index());
}

IN_PROC_BROWSER_TEST_P(TabScrubberTest, DeleteHighlighted) {
  AddTabs(browser(), 1);

  SendScrubEvent(browser(), 0);
  EXPECT_TRUE(TabScrubber::GetInstance()->IsActivationPending());
  browser()->GetTabStripModel()->CloseWebContentsAt(0,
                                                    TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(TabScrubber::GetInstance()->IsActivationPending());
}

// Delete the currently highlighted tab. Make sure the TabScrubber is
// aware.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, DeleteBeforeHighlighted) {
  AddTabs(browser(), 2);

  SendScrubEvent(browser(), 1);
  EXPECT_TRUE(TabScrubber::GetInstance()->IsActivationPending());
  browser()->GetTabStripModel()->CloseWebContentsAt(0,
                                                    TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(0, TabScrubber::GetInstance()->highlighted_tab());
}

// Move the currently highlighted tab and confirm it gets tracked.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, MoveHighlighted) {
  AddTabs(browser(), 1);

  SendScrubEvent(browser(), 0);
  EXPECT_TRUE(TabScrubber::GetInstance()->IsActivationPending());
  browser()->GetTabStripModel()->SelectTabAt(0);
  browser()->GetTabStripModel()->DeselectTabAt(1);
  browser()->GetTabStripModel()->MoveSelectedTabsTo(1, std::nullopt);
  EXPECT_EQ(1, TabScrubber::GetInstance()->highlighted_tab());
}

// Move a tab to before the highlighted one. Make sure that the highlighted tab
// index is updated correctly.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, MoveBefore) {
  AddTabs(browser(), 2);

  SendScrubEvent(browser(), 1);
  EXPECT_TRUE(TabScrubber::GetInstance()->IsActivationPending());
  browser()->GetTabStripModel()->SelectTabAt(0);
  browser()->GetTabStripModel()->SelectTabAt(2);
  browser()->GetTabStripModel()->MoveSelectedTabsTo(2, std::nullopt);
  EXPECT_EQ(0, TabScrubber::GetInstance()->highlighted_tab());
}

// Move a tab to after the highlighted one. Make sure that the highlighted tab
// index is updated correctly.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, MoveAfter) {
  AddTabs(browser(), 2);

  SendScrubEvent(browser(), 1);
  EXPECT_TRUE(TabScrubber::GetInstance()->IsActivationPending());
  browser()->GetTabStripModel()->MoveSelectedTabsTo(0, std::nullopt);
  EXPECT_EQ(2, TabScrubber::GetInstance()->highlighted_tab());
}

// Close the browser while an activation is pending.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, CloseBrowser) {
  AddTabs(browser(), 1);

  SendScrubEvent(browser(), 0);
  EXPECT_TRUE(TabScrubber::GetInstance()->IsActivationPending());
  browser()->GetWindow()->Close();
  EXPECT_FALSE(TabScrubber::GetInstance()->IsActivationPending());
}

// In an RTL layout, swipe 4 tabs in each direction. Each of the tabs should
// become active.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, RTLMulti) {
  base::i18n::ScopedDefaultIcuLocale scoped_locale(
      base::i18n::GetKnownLanguageTag("ar"));
  ASSERT_TRUE(base::i18n::IsRTL());

  AddTabs(browser(), 4);

  Scrub(browser(), 0, EACH_TAB);
  EXPECT_THAT(activation_order_, testing::ElementsAre(3, 2, 1, 0));
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());

  Scrub(browser(), 4, EACH_TAB);
  EXPECT_THAT(activation_order_, testing::ElementsAre(1, 2, 3, 4));
  EXPECT_EQ(4, browser()->GetTabStripModel()->active_index());
}

// In an RTL layout, confirm that we get the last tab made active when we skip
// tabs. These tests have 5 total tabs. We will only received scroll events
// on tabs 0, 2 and 4.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, RTLSkipped) {
  base::i18n::ScopedDefaultIcuLocale scoped_locale(
      base::i18n::GetKnownLanguageTag("ar"));
  ASSERT_TRUE(base::i18n::IsRTL());

  AddTabs(browser(), 4);

  Scrub(browser(), 0, SKIP_TABS);
  EXPECT_THAT(activation_order_, testing::ElementsAre(2, 0));
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());

  Scrub(browser(), 4, SKIP_TABS);
  EXPECT_THAT(activation_order_, testing::ElementsAre(2, 4));
  EXPECT_EQ(4, browser()->GetTabStripModel()->active_index());
}

// In an RTL layout, move a tab to before the highlighted one. Make sure that
// the highlighted tab index is updated correctly.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, RTLMoveBefore) {
  base::i18n::ScopedDefaultIcuLocale scoped_locale(
      base::i18n::GetKnownLanguageTag("ar"));
  ASSERT_TRUE(base::i18n::IsRTL());

  AddTabs(browser(), 2);

  SendScrubEvent(browser(), 1);
  EXPECT_TRUE(TabScrubber::GetInstance()->IsActivationPending());
  browser()->GetTabStripModel()->SelectTabAt(0);
  browser()->GetTabStripModel()->SelectTabAt(2);
  browser()->GetTabStripModel()->MoveSelectedTabsTo(2, std::nullopt);
  EXPECT_EQ(0, TabScrubber::GetInstance()->highlighted_tab());
}

// If the window cycle list is open, the tab scrubber should be disabled.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, DisabledIfWindowCycleListOpen) {
  AddTabs(browser(), 4);

  // Create a second browser, but don't make it active.
  BrowserWindowInterface* browser2 = CreateBrowser(browser()->GetProfile());
  browser()->GetWindow()->Activate();
  ASSERT_FALSE(browser2->GetWindow()->IsActive());
  ASSERT_TRUE(browser()->GetWindow()->IsActive());

  // Open window cycle list. It should be open now so tab scrubber should be
  // disabled.
  StartCyclingWindows(browser());
  EXPECT_FALSE(IsTabScrubberEnabled());
  Scrub(browser(), 0, EACH_TAB);
  EXPECT_EQ(0u, activation_order_.size());
  EXPECT_EQ(4, browser()->GetTabStripModel()->active_index());

  // Stop cycling. Scrub should work again.
  StopCyclingWindows(browser());
  EXPECT_TRUE(IsTabScrubberEnabled());
  Scrub(browser(), 0, EACH_TAB);
  EXPECT_THAT(activation_order_, testing::ElementsAre(3, 2, 1, 0));
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());
}

IN_PROC_BROWSER_TEST_P(TabScrubberTest, VerticalAndHorizontalScroll) {
  auto event_generator = CreateEventGenerator(browser());
  constexpr int kOffset = 100;

  {
    // If y offset is larger than x offset, the event should be recognized as a
    // vertical scroll and should not begin scrubbing.
    ui::ScrollEvent vertical_scroll_event(
        ui::EventType::kScroll, gfx::Point(0, 0), ui::EventTimeForNow(), 0,
        /*x_offset=*/0, /*y_offset=*/kOffset,
        /*x_offset_ordinal_=*/0, /*y_offset=*/kOffset,
        kScrubbingGestureFingerCount);
    event_generator->Dispatch(&vertical_scroll_event);
    EXPECT_FALSE(vertical_scroll_event.handled());
  }

  {
    // If x offset is larger than y offset, the event should be recognized as a
    // horizontal scroll and should begin scrubbing.
    ui::ScrollEvent horizontal_scroll_event(
        ui::EventType::kScroll, gfx::Point(0, 0), ui::EventTimeForNow(), 0,
        /*x_offset=*/kOffset, /*y_offset=*/0,
        /*x_offset_ordinal_=*/kOffset, /*y_offset=*/0,
        kScrubbingGestureFingerCount);
    event_generator->Dispatch(&horizontal_scroll_event);
    EXPECT_TRUE(horizontal_scroll_event.handled());
  }

  {
    // Finish scrubbing by dispatching fling scroll event. For finishing the
    // event, it is not required to be horizontal scroll. This happens for
    // example when the user start scrubbing with a horizontal scroll and the
    // fingers go up at the end of the scroll.
    ui::ScrollEvent fling_scroll_event(
        ui::EventType::kScrollFlingStart, gfx::Point(0, 0),
        ui::EventTimeForNow(), 0,
        /*x_offset=*/0, /*y_offset=*/kOffset,
        /*x_offset_ordinal_=*/0, /*y_offset=*/kOffset, 0);
    event_generator->Dispatch(&fling_scroll_event);
    EXPECT_TRUE(fling_scroll_event.handled());
  }
}

// Check scroll events other than 3-fingers scroll are not handled by
// TabScrubber.
IN_PROC_BROWSER_TEST_P(TabScrubberTest, EventHandling) {
  auto event_generator = CreateEventGenerator(browser());
  constexpr int kOffset = 100;

  {
    // Begin scrubbing and mark the event as handled for 3-fingers scroll event.
    ui::ScrollEvent scroll_event_with_3_fingers(
        ui::EventType::kScroll, gfx::Point(0, 0), ui::EventTimeForNow(), 0,
        kOffset, 0, kOffset, 0, kScrubbingGestureFingerCount);
    event_generator->Dispatch(&scroll_event_with_3_fingers);
    EXPECT_TRUE(scroll_event_with_3_fingers.handled());
  }

  {
    // Fling scroll event which is called during the scrubbing should be
    // consumed here.
    ui::ScrollEvent fling_scroll_event(ui::EventType::kScrollFlingStart,
                                       gfx::Point(0, 0), ui::EventTimeForNow(),
                                       0, kOffset, 0, kOffset, 0, 0);
    event_generator->Dispatch(&fling_scroll_event);
    EXPECT_TRUE(fling_scroll_event.handled());
  }

  {
    // Fling scroll event should NOT be consumed here if the scrubbing is not
    // ongoing. Do NOT handle the event for this scenario.
    ui::ScrollEvent fling_scroll_event(ui::EventType::kScrollFlingStart,
                                       gfx::Point(0, 0), ui::EventTimeForNow(),
                                       0, kOffset, 0, kOffset, 0, 0);
    event_generator->Dispatch(&fling_scroll_event);
    EXPECT_FALSE(fling_scroll_event.handled());
  }

  {
    // Other scroll events should be not handled by TabScrubber.
    ui::ScrollEvent scroll_event_with_2_fingers(
        ui::EventType::kScroll, gfx::Point(0, 0), ui::EventTimeForNow(), 0,
        kOffset, 0, kOffset, 0,
        /*finger_count=*/2);
    event_generator->Dispatch(&scroll_event_with_2_fingers);
    EXPECT_FALSE(scroll_event_with_2_fingers.handled());
  }
}

IN_PROC_BROWSER_TEST_P(TabScrubberTest, SplitTabHover) {
  AddTabs(browser(), 2);
  browser()->GetTabStripModel()->AddToNewSplit(
      {0, 1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kToolbarButton);
  browser()->GetTabStripModel()->ActivateTabAt(2);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view);
  browser_view->GetWidget()->LayoutRootViewIfNecessary();

  SendScrubEvent(browser(), 1);
  EXPECT_TRUE(TabScrubber::GetInstance()->IsActivationPending());
  EXPECT_EQ(1, TabScrubber::GetInstance()->highlighted_tab());

  if (GetParam()) {
    auto* region_view = views::AsViewClass<BaseTabStripRegionView>(
        browser_view->tab_strip_view());
    ASSERT_TRUE(region_view);
    auto* tab_view_0 =
        views::AsViewClass<TabView>(region_view->GetTabAnchorView(
            browser()->GetTabStripModel()->GetTabAtIndex(0)->GetHandle()));
    auto* tab_view_1 =
        views::AsViewClass<TabView>(region_view->GetTabAnchorView(
            browser()->GetTabStripModel()->GetTabAtIndex(1)->GetHandle()));
    ASSERT_TRUE(tab_view_0);
    ASSERT_TRUE(tab_view_1);
    auto* split_view = views::AsViewClass<SplitTabView>(tab_view_0->parent());
    ASSERT_TRUE(split_view);
    EXPECT_EQ(split_view, tab_view_1->parent());
    EXPECT_TRUE(split_view->IsHovering());
    EXPECT_TRUE(tab_view_0->IsHovering());
    EXPECT_TRUE(tab_view_1->IsHovering());
  }

  FinishScrub(true);
  EXPECT_EQ(1, browser()->GetTabStripModel()->active_index());

  if (GetParam()) {
    auto* region_view = views::AsViewClass<BaseTabStripRegionView>(
        browser_view->tab_strip_view());
    auto* tab_view_0 =
        views::AsViewClass<TabView>(region_view->GetTabAnchorView(
            browser()->GetTabStripModel()->GetTabAtIndex(0)->GetHandle()));
    auto* tab_view_1 =
        views::AsViewClass<TabView>(region_view->GetTabAnchorView(
            browser()->GetTabStripModel()->GetTabAtIndex(1)->GetHandle()));
    auto* split_view = views::AsViewClass<SplitTabView>(tab_view_0->parent());
    ASSERT_TRUE(split_view);
    EXPECT_FALSE(split_view->IsHovering());
    EXPECT_FALSE(tab_view_0->IsHovering());
    EXPECT_FALSE(tab_view_1->IsHovering());
  }
}

IN_PROC_BROWSER_TEST_P(TabScrubberTest, Overflow) {
  if (!GetParam()) {
    GTEST_SKIP() << "Scrollable tab strip overflow requires unified tab strip.";
  }
  VerifyUnifiedOverflowScrub();
}

IN_PROC_BROWSER_TEST_P(TabScrubberTest, RTLOverflow) {
  if (!GetParam()) {
    GTEST_SKIP() << "Scrollable tab strip overflow requires unified tab strip.";
  }
  base::i18n::ScopedDefaultIcuLocale scoped_locale(
      base::i18n::GetKnownLanguageTag("ar"));
  ASSERT_TRUE(base::i18n::IsRTL());
  VerifyUnifiedOverflowScrub();
}

INSTANTIATE_TEST_SUITE_P(All,
                         TabScrubberTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Unified" : "Legacy";
                         });

}  // namespace ash
