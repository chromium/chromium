// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_view_cuj_controller.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace {

using enum ::split_tabs::SplitTabCreatedSource;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::ReturnRef;

constexpr std::pair<split_tabs::SplitTabCreatedSource,
                    ui::CustomElementEventType>
    kSourceEvents[] = {
        {kTabContextMenu, kSplitViewEnteredViaTabContextMenuCustomEventId},
        {kToolbarButton, kSplitViewEnteredViaToolbarCustomEventId},
        {kDragAndDropLink, kSplitViewEnteredViaDragLinkCustomEventId},
        {kLinkContextMenu, kSplitViewEnteredViaLinkContextMenuCustomEventId},
        {kBookmarkContextMenu, kSplitViewEnteredViaOtherCustomEventId},
};

class TestBrowserElements : public BrowserElements {
 public:
  DECLARE_SAFE_CAST_TARGET()
  TestBrowserElements(BrowserWindowInterface& browser, ui::ElementContext ctx)
      : BrowserElements(browser), ctx_(ctx) {}
  ui::ElementContext GetContext() override { return ctx_; }

 private:
  const ui::ElementContext ctx_;
};
DEFINE_SAFE_CAST_TARGET(TestBrowserElements)

class TestSplitWindow {
 public:
  explicit TestSplitWindow(
      Profile* profile,
      int initial_tabs = 0,
      ui::ElementContext ctx =
          ui::ElementContext::CreateFakeContextForTesting(1))
      : profile_(profile),
        tab_strip_model_(&delegate_, profile),
        browser_view_(kBrowserViewElementId, ctx) {
    delegate_.SetBrowserWindowInterface(&browser_);
    ON_CALL(browser_, GetTabStripModel())
        .WillByDefault(Return(&tab_strip_model_));
    ON_CALL(browser_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(browser_, GetProfile()).WillByDefault(Return(profile));
    elements_.emplace(browser_, ctx);
    browser_view_.Show();

    auto sub = [&](ui::CustomElementEventType event) {
      subscriptions_.push_back(
          ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
              event, ctx,
              base::BindLambdaForTesting([this, event](ui::TrackedElement*) {
                events_.push_back(event);
              })));
    };
    for (auto [_, event] : kSourceEvents) {
      sub(event);
    }
    sub(kSplitViewSustainedComparisonCustomEventId);
    sub(kSplitViewExitedOnNtpCustomEventId);
    sub(kSplitViewExitedEarlyCustomEventId);

    controller_.emplace(&browser_);
    for (int i = 0; i < initial_tabs; ++i) {
      AddTab();
    }
  }

  ~TestSplitWindow() {
    controller_.reset();
    tab_strip_model_.CloseAllTabs();
    delegate_.SetBrowserWindowInterface(nullptr);
  }

  tabs::TabInterface* AddTab(const char* url = "https://a.test") {
    auto wc =
        content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
    content::WebContentsTester::For(wc.get())->NavigateAndCommit(GURL(url));
    auto* raw = wc.get();
    tab_strip_model_.AppendWebContents(std::move(wc), true);
    return tab_strip_model_.GetTabForWebContents(raw);
  }

  auto CreateSplit(
      int index = 0,
      split_tabs::SplitTabCreatedSource source = kLinkContextMenu) {
    if (tab_strip_model_.active_index() == index) {
      index = index == 0 ? 1 : 0;
    }
    return tab_strip_model_.AddToNewSplit({index}, {}, source);
  }

  auto TakeEvents() { return std::exchange(events_, {}); }
  TabStripModel& model() { return tab_strip_model_; }
  ui::ElementContext context() const { return browser_view_.context(); }
  void HideBrowserView() { browser_view_.Hide(); }
  void ShowBrowserView() { browser_view_.Show(); }
  void DestroyController() { controller_.reset(); }

 private:
  const raw_ptr<Profile> profile_;
  ui::UnownedUserDataHost user_data_host_;
  testing::NiceMock<MockBrowserWindowInterface> browser_;
  TestTabStripModelDelegate delegate_;
  TabStripModel tab_strip_model_;
  std::optional<TestBrowserElements> elements_;
  ui::test::TestElement browser_view_;
  std::vector<ui::CustomElementEventType> events_;
  std::vector<base::CallbackListSubscription> subscriptions_;
  std::optional<SplitViewCujController> controller_;
};

class SplitViewCujControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  SplitViewCujControllerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 private:
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
};

TEST_F(SplitViewCujControllerTest,
       EntrySourcesSustainedCompletionAndQuickExit) {
  TestSplitWindow w(profile(), /*initial_tabs=*/1);
  for (auto [source, entry_event] : kSourceEvents) {
    w.AddTab("https://b.test");
    w.CreateSplit(0, source);
    task_environment()->FastForwardBy(base::Seconds(14));
    EXPECT_THAT(w.TakeEvents(), ElementsAre(entry_event));
    task_environment()->FastForwardBy(base::Seconds(1));
    EXPECT_THAT(w.TakeEvents(),
                ElementsAre(kSplitViewSustainedComparisonCustomEventId));
    w.model().CloseWebContentsAt(1, TabCloseTypes::CLOSE_USER_GESTURE);
    EXPECT_THAT(w.TakeEvents(), IsEmpty());
  }

  // Closing a populated split before 15s emits
  // `kSplitViewExitedEarlyCustomEventId`.
  w.AddTab("https://c.test");
  w.CreateSplit(0, kDragAndDropLink);
  task_environment()->FastForwardBy(base::Seconds(5));
  w.model().CloseWebContentsAt(1, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_THAT(w.TakeEvents(),
              ElementsAre(kSplitViewEnteredViaDragLinkCustomEventId,
                          kSplitViewExitedEarlyCustomEventId));
}

TEST_F(SplitViewCujControllerTest, NtpBounceVsPopulatedViaReplaceOrNavigation) {
  TestSplitWindow w(profile(), /*initial_tabs=*/2);

  // Unused Split View NTP closed after 20s emits `ExitedOnNtp`, not sustained.
  w.AddTab(chrome::kChromeUISplitViewNewTabPageURL);
  w.CreateSplit(1, kToolbarButton);
  task_environment()->FastForwardBy(base::Seconds(20));
  EXPECT_THAT(w.TakeEvents(),
              ElementsAre(kSplitViewEnteredViaToolbarCustomEventId));
  w.model().CloseWebContentsAt(2, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_THAT(w.TakeEvents(), ElementsAre(kSplitViewExitedOnNtpCustomEventId));

  // Populating NTP via `UpdateTabInSplit()` starts the 15s timer to completion.
  auto* ntp1 = w.AddTab(chrome::kChromeUISplitViewNewTabPageURL);
  const auto split1 = w.CreateSplit(1, kTabContextMenu);
  w.model().UpdateTabInSplit(ntp1->GetHandle(),
                             w.model().GetTabAtIndex(0)->GetHandle(),
                             TabStripModel::SplitUpdateType::kReplace);
  task_environment()->FastForwardBy(base::Seconds(15));
  w.model().RemoveSplit(split1);
  EXPECT_THAT(w.TakeEvents(),
              ElementsAre(kSplitViewEnteredViaTabContextMenuCustomEventId,
                          kSplitViewSustainedComparisonCustomEventId));

  // Populating NTP via navigation also starts the 15s timer to completion
  // (and committing another NTP URL first does not start the timer or loop).
  auto* ntp2 = w.AddTab(chrome::kChromeUISplitViewNewTabPageURL);
  const auto split2 = w.CreateSplit(0, kToolbarButton);
  content::WebContentsTester::For(ntp2->GetContents())
      ->NavigateAndCommit(GURL(chrome::kChromeUISplitViewNewTabPageURL));
  content::WebContentsTester::For(ntp2->GetContents())
      ->NavigateAndCommit(GURL("https://chromium.org"));
  task_environment()->FastForwardBy(base::Seconds(15));
  w.model().RemoveSplit(split2);
  EXPECT_THAT(w.TakeEvents(),
              ElementsAre(kSplitViewEnteredViaToolbarCustomEventId,
                          kSplitViewSustainedComparisonCustomEventId));
}

TEST_F(SplitViewCujControllerTest,
       MutationsRestoreWindowCloseAndCrossWindowDrag) {
  TestSplitWindow w1(profile(), /*initial_tabs=*/4);
  const auto restored = split_tabs::SplitTabId::GenerateNew();
  w1.model().RestoreSplit(restored, {0, 1}, {});
  task_environment()->FastForwardBy(base::Seconds(20));
  w1.model().RemoveSplit(restored);
  EXPECT_THAT(w1.TakeEvents(), IsEmpty());

  const auto first = w1.CreateSplit(0);
  w1.model().ActivateTabAt(1);
  const auto second = w1.CreateSplit(0);
  w1.model().ReverseTabsInSplit(second);
  w1.model().RemoveSplit(first);
  EXPECT_THAT(w1.TakeEvents(),
              ElementsAre(kSplitViewEnteredViaLinkContextMenuCustomEventId));

  TestSplitWindow w2(profile(), /*initial_tabs=*/0,
                     ui::ElementContext::CreateFakeContextForTesting(2));
  task_environment()->FastForwardBy(base::Seconds(5));
  w2.model().InsertDetachedSplitTabAt(
      w1.model().DetachSplitTabForInsertion(second), 0, /*pinned=*/false);
  task_environment()->FastForwardBy(base::Seconds(10));
  EXPECT_THAT(w1.TakeEvents(), IsEmpty());
  EXPECT_THAT(w2.TakeEvents(),
              ElementsAre(kSplitViewSustainedComparisonCustomEventId));
  w2.model().RemoveSplit(second);

  // Full-window closes before 15s suppress early-exit events.
  w2.CreateSplit(0);
  w2.model().ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTab);
  EXPECT_THAT(w2.TakeEvents(),
              ElementsAre(kSplitViewEnteredViaLinkContextMenuCustomEventId));
  w1.CreateSplit(0);
  w1.model().CloseAllTabs();
  EXPECT_THAT(w1.TakeEvents(),
              ElementsAre(kSplitViewEnteredViaLinkContextMenuCustomEventId));
}

TEST_F(SplitViewCujControllerTest,
       HiddenPopulatedWindowDoesNotStarveOrStealDwell) {
  TestSplitWindow w1(profile(), /*initial_tabs=*/2,
                     ui::ElementContext::CreateFakeContextForTesting(1));
  TestSplitWindow w2(profile(), /*initial_tabs=*/2,
                     ui::ElementContext::CreateFakeContextForTesting(2));

  // Hide whichever window's SplitTabId sorts first in `active_splits` so the
  // non-starvation check is deterministic.
  const auto s1 = w1.CreateSplit(0);
  const auto s2 = w2.CreateSplit(0);
  EXPECT_THAT(w1.TakeEvents(),
              ElementsAre(kSplitViewEnteredViaLinkContextMenuCustomEventId));
  TestSplitWindow& hidden_win = s1 < s2 ? w1 : w2;
  TestSplitWindow& visible_win = s1 < s2 ? w2 : w1;
  task_environment()->FastForwardBy(base::Seconds(5));
  hidden_win.HideBrowserView();
  task_environment()->FastForwardBy(base::Seconds(10));
  EXPECT_THAT(hidden_win.TakeEvents(), IsEmpty());
  EXPECT_THAT(visible_win.TakeEvents(),
              ElementsAre(kSplitViewSustainedComparisonCustomEventId));
  hidden_win.ShowBrowserView();
  w1.model().RemoveSplit(s1);
  w2.model().RemoveSplit(s2);

  // Hiding the only populated window pauses the timer rather than polling or
  // counting hidden dwell time; showing it again resumes a full 15s timer.
  const auto s3 = w1.CreateSplit(0);
  task_environment()->FastForwardBy(base::Seconds(1));
  w1.HideBrowserView();
  task_environment()->FastForwardBy(base::Seconds(20));
  EXPECT_THAT(w1.TakeEvents(),
              ElementsAre(kSplitViewEnteredViaLinkContextMenuCustomEventId));
  w1.ShowBrowserView();
  task_environment()->FastForwardBy(base::Seconds(14));
  EXPECT_THAT(w1.TakeEvents(), IsEmpty());
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_THAT(w1.TakeEvents(),
              ElementsAre(kSplitViewSustainedComparisonCustomEventId));
  w1.model().RemoveSplit(s3);

  // Synchronously removing the split inside the sustained-comparison event
  // callback must not double-emit an early-exit event.
  const auto s4 = w1.CreateSplit(0);
  auto reentrant_sub =
      ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
          kSplitViewSustainedComparisonCustomEventId, w1.context(),
          base::BindLambdaForTesting(
              [&](ui::TrackedElement*) { w1.model().RemoveSplit(s4); }));
  task_environment()->FastForwardBy(base::Seconds(15));
  EXPECT_THAT(w1.TakeEvents(),
              ElementsAre(kSplitViewEnteredViaLinkContextMenuCustomEventId,
                          kSplitViewSustainedComparisonCustomEventId));
}

TEST_F(SplitViewCujControllerTest,
       NavigatingPopulatedPaneBackToNtpPausesTimer) {
  TestSplitWindow w(profile(), /*initial_tabs=*/1);
  auto* pane2 = w.AddTab("https://b.test");
  const auto split = w.CreateSplit(0, kTabContextMenu);

  task_environment()->FastForwardBy(base::Seconds(5));
  content::WebContentsTester::For(pane2->GetContents())
      ->NavigateAndCommit(GURL(chrome::kChromeUISplitViewNewTabPageURL));
  task_environment()->FastForwardBy(base::Seconds(10));
  EXPECT_THAT(w.TakeEvents(),
              ElementsAre(kSplitViewEnteredViaTabContextMenuCustomEventId));

  content::WebContentsTester::For(pane2->GetContents())
      ->NavigateAndCommit(GURL("https://c.test"));
  task_environment()->FastForwardBy(base::Seconds(14));
  EXPECT_THAT(w.TakeEvents(), IsEmpty());
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_THAT(w.TakeEvents(),
              ElementsAre(kSplitViewSustainedComparisonCustomEventId));
  w.model().RemoveSplit(split);
}

TEST_F(SplitViewCujControllerTest,
       DiscardedSplitPaneTracksReplacementAndEmptyDiscardStaysUnpopulated) {
  TestSplitWindow w(profile(), /*initial_tabs=*/1);
  auto* ntp = w.AddTab(chrome::kChromeUISplitViewNewTabPageURL);
  const auto split = w.CreateSplit(0, kToolbarButton);

  // Discarding an uncommitted NTP leaves an empty URL on the replacement
  // WebContents, which must stay unpopulated while re-attaching navigation
  // tracking.
  auto replacement =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  w.model().DiscardWebContents(ntp->GetContents(), std::move(replacement));
  task_environment()->FastForwardBy(base::Seconds(20));
  EXPECT_THAT(w.TakeEvents(),
              ElementsAre(kSplitViewEnteredViaToolbarCustomEventId));

  content::WebContentsTester::For(ntp->GetContents())
      ->NavigateAndCommit(GURL("https://chromium.org"));
  task_environment()->FastForwardBy(base::Seconds(14));
  EXPECT_THAT(w.TakeEvents(), IsEmpty());
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_THAT(w.TakeEvents(),
              ElementsAre(kSplitViewSustainedComparisonCustomEventId));
  w.model().RemoveSplit(split);
}

TEST_F(SplitViewCujControllerTest,
       DestroyingPopulatedControllerRefreshesRemainingSplits) {
  TestSplitWindow w1(profile(), /*initial_tabs=*/2,
                     ui::ElementContext::CreateFakeContextForTesting(1));
  TestSplitWindow w2(profile(), /*initial_tabs=*/1,
                     ui::ElementContext::CreateFakeContextForTesting(2));
  auto* w2_ntp = w2.AddTab(chrome::kChromeUISplitViewNewTabPageURL);

  w1.CreateSplit(0, kLinkContextMenu);
  const auto w2_split = w2.CreateSplit(0, kToolbarButton);
  task_environment()->FastForwardBy(base::Seconds(5));
  w1.DestroyController();

  // The stale timer from `w1` was stopped, so populating `w2` at t=14s must
  // wait a full 15s rather than firing at t=15s.
  task_environment()->FastForwardBy(base::Seconds(9));
  content::WebContentsTester::For(w2_ntp->GetContents())
      ->NavigateAndCommit(GURL("https://chromium.org"));
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_THAT(w1.TakeEvents(),
              ElementsAre(kSplitViewEnteredViaLinkContextMenuCustomEventId));
  EXPECT_THAT(w2.TakeEvents(), IsEmpty());

  task_environment()->FastForwardBy(base::Seconds(14));
  EXPECT_THAT(w2.TakeEvents(),
              ElementsAre(kSplitViewSustainedComparisonCustomEventId));
  w2.model().RemoveSplit(w2_split);
}

}  // namespace
