// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/memory_saver_bubble_view.h"

#include <tuple>

#include "base/byte_size.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_observer.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/performance_controls/test_support/memory_saver_browser_test_mixin.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"
#include "chrome/browser/ui/views/performance_controls/memory_saver_resource_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr base::ByteSize kMemorySavings = base::MiB(100);
}  // namespace

class StubMemorySaverBubbleObserver : public MemorySaverBubbleObserver {
 public:
  void OnBubbleShown() override {}
  void OnBubbleHidden() override {}
};

class MemorySaverBubbleViewTest
    : public MemorySaverBrowserTestMixin<InProcessBrowserTest> {
 public:
  void SetUpOnMainThread() override {
    MemorySaverBrowserTestMixin::SetUpOnMainThread();

    unconditionally_discard_pages_ = std::make_unique<
        performance_manager::testing::ScopedSetAllPagesDiscardableForTesting>();

    browser()->GetProfile()->GetPrefs()->SetInteger(
        prefs::kMemorySaverChipExpandedCount, 0);

    AddNewTab(kMemorySavings, ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

    SetMemorySaverModeEnabled(true);
  }
  void TearDownOnMainThread() override {
    unconditionally_discard_pages_.reset();
    auto* bubble_view = GetBubbleView();
    if (bubble_view && bubble_view->GetWidget()) {
      bubble_view->GetWidget()->CloseNow();
    }
    MemorySaverBrowserTestMixin::TearDownOnMainThread();
  }

  void AddNewTab(base::ByteSize memory_savings,
                 mojom::LifecycleUnitDiscardReason discard_reason) {
    TabStripModel* tab_strip_model = browser()->GetTabStripModel();
    if (tab_strip_model->count() == 1 &&
        (tab_strip_model->GetWebContentsAt(0)
             ->GetLastCommittedURL()
             .is_empty() ||
         tab_strip_model->GetWebContentsAt(0)->GetLastCommittedURL() ==
             GURL("about:blank"))) {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(
          browser(), GetURL("foo.com", "/title1.html")));
    } else {
      ASSERT_TRUE(AddTabAtIndex(tab_strip_model->count(),
                                GetURL("foo.com", "/title1.html"),
                                ui::PAGE_TRANSITION_LINK));
    }
    content::WebContents* const contents =
        tab_strip_model->GetActiveWebContents();
    performance_manager::user_tuning::UserPerformanceTuningManager::
        PreDiscardResourceUsage::CreateForWebContents(contents, memory_savings,
                                                      discard_reason);
  }

  void SetTabDiscardState(int tab_index, bool is_discarded) {
    if (is_discarded) {
      base::ByteSize savings = kMemorySavings;
      mojom::LifecycleUnitDiscardReason reason =
          ::mojom::LifecycleUnitDiscardReason::PROACTIVE;
      content::WebContents* const old_contents =
          browser()->GetTabStripModel()->GetWebContentsAt(tab_index);
      if (auto* old_usage =
              performance_manager::user_tuning::UserPerformanceTuningManager::
                  PreDiscardResourceUsage::FromWebContents(old_contents)) {
        savings = old_usage->memory_footprint_estimate();
        reason = old_usage->discard_reason();
      }

      TryDiscardTabAt(tab_index);

      content::WebContents* const new_contents =
          browser()->GetTabStripModel()->GetWebContentsAt(tab_index);
      if (auto* new_usage =
              performance_manager::user_tuning::UserPerformanceTuningManager::
                  PreDiscardResourceUsage::FromWebContents(new_contents)) {
        new_usage->UpdateDiscardInfo(savings, reason);
      } else {
        performance_manager::user_tuning::UserPerformanceTuningManager::
            PreDiscardResourceUsage::CreateForWebContents(new_contents, savings,
                                                          reason);
      }
    }
  }

  page_actions::PageActionViewInterface* GetPageActionView(
      BrowserWindowInterface* b = nullptr) {
    if (!b) {
      b = browser();
    }
    auto* provider =
        BrowserView::GetBrowserViewForBrowser(b)->toolbar_button_provider();
    return provider->GetPageActionViewInterface(kActionShowMemorySaverChip);
  }

  views::View* GetBubbleView(BrowserWindowInterface* b = nullptr) {
    if (!b) {
      b = browser();
    }
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
        MemorySaverBubbleView::kMemorySaverDialogBodyElementId,
        views::ElementTrackerViews::GetContextForView(
            BrowserView::GetBrowserViewForBrowser(b)));
  }

  template <class T>
  T* GetMatchingView(ui::ElementIdentifier identifier,
                     BrowserWindowInterface* b = nullptr) {
    views::View* bubble_view = GetBubbleView(b);
    if (!bubble_view || !bubble_view->GetWidget()) {
      return nullptr;
    }
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForWidget(
            bubble_view->GetWidget());
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingViewAs<T>(
        identifier, context);
  }

  void ClickPageActionChip(BrowserWindowInterface* b = nullptr) {
    if (!b) {
      b = browser();
    }
    page_actions::PageActionTestAccessor(b, kActionShowMemorySaverChip).Click();
  }

  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<
      performance_manager::testing::ScopedSetAllPagesDiscardableForTesting>
      unconditionally_discard_pages_;
};

class MemorySaverBubbleViewSavingsTest
    : public MemorySaverBubbleViewTest,
      public testing::WithParamInterface<std::tuple<base::ByteSize, int>> {};

// When the page action chip is clicked, the dialog should open.
IN_PROC_BROWSER_TEST_F(MemorySaverBubbleViewTest, ShouldOpenDialogOnClick) {
  SetTabDiscardState(0, true);

  EXPECT_EQ(GetBubbleView(), nullptr);

  ClickPageActionChip();

  EXPECT_NE(GetBubbleView(), nullptr);
}

// When the dialog is closed, UMA metrics should be logged.
IN_PROC_BROWSER_TEST_F(MemorySaverBubbleViewTest,
                       ShouldLogMetricsOnDialogDismiss) {
  SetTabDiscardState(0, true);

  // Open bubble
  StubMemorySaverBubbleObserver observer;
  auto* bubble = MemorySaverBubbleView::ShowBubble(
      browser(), GetPageActionView()->GetBubbleAnchor(), &observer);
  ASSERT_NE(GetBubbleView(), nullptr);

  // Close bubble
  bubble->Close();
  ASSERT_EQ(GetBubbleView(), nullptr);

  histogram_tester_.ExpectUniqueSample(
      "PerformanceControls.MemorySaver.BubbleAction",
      MemorySaverBubbleActionType::kDismiss, 1);
}

// The domain of the current site should be rendered as a subtitle.
IN_PROC_BROWSER_TEST_F(MemorySaverBubbleViewTest,
                       ShouldRenderDomainInDialogSubtitle) {
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::Widget* widget = GetBubbleView()->GetWidget();
  views::BubbleDialogDelegate* const bubble_delegate =
      widget->widget_delegate()->AsBubbleDialogDelegate();
  EXPECT_EQ(bubble_delegate->GetSubtitle(), u"foo.com");
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(MemorySaverBubbleViewTest,
                       ShowDialogWithoutExcludeSiteButtonInGuestMode) {
  BrowserWindowInterface* guest_browser = CreateGuestBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(guest_browser,
                                           GetURL("foo.com", "/title1.html")));

  content::WebContents* const contents =
      guest_browser->GetTabStripModel()->GetActiveWebContents();
  performance_manager::user_tuning::UserPerformanceTuningManager::
      PreDiscardResourceUsage::CreateForWebContents(
          contents, kMemorySavings,
          ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  auto* manager = performance_manager::user_tuning::
      UserPerformanceTuningManager::GetInstance();
  manager->DiscardPageForTesting(contents);

  ClickPageActionChip(guest_browser);

  // Exclude site button shouldn't be shown since guest users can't exclude
  // sites from being discarded
  views::Button* const cancel_button = GetMatchingView<views::Button>(
      MemorySaverBubbleView::kMemorySaverDialogCancelButton, guest_browser);
  EXPECT_EQ(cancel_button, nullptr);
}
#endif

IN_PROC_BROWSER_TEST_F(MemorySaverBubbleViewTest,
                       ShouldCollapseChipAfterNavigatingTabsWithDialogOpen) {
  AddNewTab(kMemorySavings, ::mojom::LifecycleUnitDiscardReason::PROACTIVE);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  EXPECT_EQ(2, tab_strip_model->count());

  tab_strip_model->ActivateTabAt(0);
  SetTabDiscardState(1, true);
  tab_strip_model->ActivateTabAt(1);
  content::WaitForLoadStop(tab_strip_model->GetWebContentsAt(1));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return page_actions::PageActionTestAccessor(browser(),
                                                kActionShowMemorySaverChip)
        .IsChipVisible();
  }));

  SetTabDiscardState(0, true);

  tab_strip_model->SelectNextTab();
  content::WaitForLoadStop(tab_strip_model->GetWebContentsAt(0));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return page_actions::PageActionTestAccessor(browser(),
                                                kActionShowMemorySaverChip)
        .IsChipVisible();
  }));

  ClickPageActionChip();
  tab_strip_model->SelectPreviousTab();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !page_actions::PageActionTestAccessor(browser(),
                                                 kActionShowMemorySaverChip)
                .IsChipVisible();
  }));
}

// The memory savings should be rendered within the resource view.
IN_PROC_BROWSER_TEST_F(MemorySaverBubbleViewTest,
                       ShouldRenderMemorySavingsInResourceView) {
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::Label* label = GetMatchingView<views::Label>(
      MemorySaverResourceView::kMemorySaverResourceViewMemorySavingsElementId);
  EXPECT_TRUE(label->GetText().find(ui::FormatBytes(kMemorySavings)) !=
              std::string::npos);
}

// The memory savings should not be rendered within the text above the resource
// view.
IN_PROC_BROWSER_TEST_F(MemorySaverBubbleViewTest,
                       ShouldNotRenderMemorySavingsInDialogBodyText) {
  SetTabDiscardState(0, true);

  ClickPageActionChip();

  views::Label* label = GetMatchingView<views::Label>(
      MemorySaverBubbleView::kMemorySaverDialogBodyElementId);
  EXPECT_EQ(label->GetText().find(ui::FormatBytes(kMemorySavings)),
            std::string::npos);

  EXPECT_NE(label->GetText().find(
                l10n_util::GetStringUTF16(IDS_MEMORY_SAVER_DIALOG_BODY)),
            std::string::npos);
}

// The correct label should be rendered for different memory savings amounts.
IN_PROC_BROWSER_TEST_P(MemorySaverBubbleViewSavingsTest,
                       ShowsCorrectLabelsForDifferentSavings) {
  AddNewTab(std::get<0>(GetParam()),
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  tab_strip_model->ActivateTabAt(0);
  SetTabDiscardState(1, true);
  tab_strip_model->ActivateTabAt(1);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return page_actions::PageActionTestAccessor(browser(),
                                                kActionShowMemorySaverChip)
        .IsChipVisible();
  }));

  ClickPageActionChip();

  views::Label* label = GetMatchingView<views::Label>(
      MemorySaverResourceView::kMemorySaverResourceViewMemoryLabelElementId);
  EXPECT_EQ(label->GetText(),
            l10n_util::GetStringUTF16(std::get<1>(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MemorySaverBubbleViewSavingsTest,
    ::testing::Values(
        std::tuple{base::MiB(50), IDS_MEMORY_SAVER_DIALOG_SMALL_SAVINGS_LABEL},
        std::tuple{base::MiB(100),
                   IDS_MEMORY_SAVER_DIALOG_MEDIUM_SAVINGS_LABEL},
        std::tuple{base::MiB(150),
                   IDS_MEMORY_SAVER_DIALOG_MEDIUM_SAVINGS_LABEL},
        std::tuple{base::MiB(600), IDS_MEMORY_SAVER_DIALOG_LARGE_SAVINGS_LABEL},
        std::tuple{base::MiB(900),
                   IDS_MEMORY_SAVER_DIALOG_VERY_LARGE_SAVINGS_LABEL}));
