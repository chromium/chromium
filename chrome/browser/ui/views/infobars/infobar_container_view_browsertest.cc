// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_container_view.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/features.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view_utils.h"

namespace {

class PriorityInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  PriorityInfoBarDelegate(infobars::InfoBarDelegate::InfobarPriority priority,
                          const std::u16string& message)
      : priority_(priority), message_(message) {}

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return infobars::InfoBarDelegate::TEST_INFOBAR;
  }

  infobars::InfoBarDelegate::InfobarPriority GetPriority() const override {
    return priority_;
  }

  std::u16string GetMessageText() const override { return message_; }

  // Ensure each infobar is treated as unique.
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override {
    return false;
  }

 private:
  const infobars::InfoBarDelegate::InfobarPriority priority_;
  const std::u16string message_;
};

}  // namespace

class InfoBarContainerViewBrowserTest : public InProcessBrowserTest {
 public:
  InfoBarContainerViewBrowserTest() = default;

  InfoBarContainerView* GetInfoBarContainer() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->infobar_container();
  }

  infobars::InfoBarManager* GetInfoBarManager() {
    return infobars::ContentInfoBarManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  infobars::InfoBar* AddInfoBar(
      infobars::InfoBarDelegate::InfobarPriority priority,
      const std::string& message) {
    auto delegate = std::make_unique<PriorityInfoBarDelegate>(
        priority, base::UTF8ToUTF16(message));
    return GetInfoBarManager()->AddInfoBar(
        std::make_unique<ConfirmInfoBar>(std::move(delegate)));
  }

  // Returns the message text of all currently visible infobar views.
  std::vector<std::string> GetVisibleInfoBarMessages() {
    std::vector<std::string> messages;
    InfoBarContainerView* container = GetInfoBarContainer();
    for (views::View* child : container->children()) {
      if (auto* infobar_view = AsViewClass<InfoBarView>(child);
          infobar_view && child->GetVisible()) {
        if (auto* delegate =
                infobar_view->delegate()->AsConfirmInfoBarDelegate()) {
          messages.push_back(base::UTF16ToUTF8(delegate->GetMessageText()));
        }
      }
    }
    return messages;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

//
// Tests for standard (non-prioritized) behavior.
//
class InfoBarContainerStandardTest : public InfoBarContainerViewBrowserTest {
 public:
  InfoBarContainerStandardTest() {
    feature_list_.InitAndDisableFeature(infobars::kInfobarPrioritization);
  }
};

IN_PROC_BROWSER_TEST_F(InfoBarContainerStandardTest, AllAddedInfobarsAreShown) {
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kDefault, "InfoBar 1");
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kCriticalSecurity,
             "InfoBar 2");
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kLow, "InfoBar 3");

  // In standard mode, all infobars are visible regardless of priority.
  std::vector<std::string> visible_messages = GetVisibleInfoBarMessages();
  EXPECT_EQ(3u, visible_messages.size());
  EXPECT_EQ("InfoBar 1", visible_messages[0]);
  EXPECT_EQ("InfoBar 2", visible_messages[1]);
  EXPECT_EQ("InfoBar 3", visible_messages[2]);
}

IN_PROC_BROWSER_TEST_F(InfoBarContainerStandardTest, RemoveInfoBar) {
  infobars::InfoBar* infobar1 = AddInfoBar(
      infobars::InfoBarDelegate::InfobarPriority::kDefault, "InfoBar 1");
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kDefault, "InfoBar 2");

  ASSERT_EQ(2u, GetVisibleInfoBarMessages().size());
  GetInfoBarManager()->RemoveInfoBar(infobar1);

  std::vector<std::string> visible_messages = GetVisibleInfoBarMessages();
  EXPECT_EQ(1u, visible_messages.size());
  EXPECT_EQ("InfoBar 2", visible_messages[0]);
}

IN_PROC_BROWSER_TEST_F(InfoBarContainerStandardTest, ReplaceInfoBar) {
  infobars::InfoBar* old_bar = AddInfoBar(
      infobars::InfoBarDelegate::InfobarPriority::kDefault, "Original Message");

  ASSERT_EQ(1u, GetVisibleInfoBarMessages().size());

  // Create a new delegate/infobar to replace the old one.
  auto new_delegate = std::make_unique<PriorityInfoBarDelegate>(
      infobars::InfoBarDelegate::InfobarPriority::kDefault,
      u"Replacement Message");

  GetInfoBarManager()->ReplaceInfoBar(
      old_bar, std::make_unique<ConfirmInfoBar>(std::move(new_delegate)));

  std::vector<std::string> messages = GetVisibleInfoBarMessages();
  ASSERT_EQ(1u, messages.size());
  EXPECT_EQ("Replacement Message", messages[0]);
}

IN_PROC_BROWSER_TEST_F(InfoBarContainerStandardTest,
                       NavigationDismissesInfoBar) {
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kDefault, "Transient");
  ASSERT_EQ(1u, GetVisibleInfoBarMessages().size());

  // Navigate to a new URL. Most delegates (like ConfirmInfoBarDelegate)
  // are configured to expire on navigation by default.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));

  EXPECT_TRUE(GetInfoBarContainer()->IsEmpty());
  EXPECT_TRUE(GetVisibleInfoBarMessages().empty());
}

//
// Tests for priority-based behavior.
//
class InfoBarContainerPriorityTest : public InfoBarContainerViewBrowserTest {
 public:
  InfoBarContainerPriorityTest() {
    // These caps match the design doc's defaults.
    feature_list_.InitAndEnableFeatureWithParameters(
        infobars::kInfobarPrioritization, {{"max_visible_critical", "2"},
                                           {"max_visible_default", "1"},
                                           {"max_visible_low", "1"}});
  }
};

IN_PROC_BROWSER_TEST_F(InfoBarContainerPriorityTest, CriticalStacksUpToCap) {
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kCriticalSecurity,
             "Critical 1");
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kCriticalSecurity,
             "Critical 2");
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kCriticalSecurity,
             "Critical 3 (Queued)");

  // Only the first two critical infobars should be visible.
  std::vector<std::string> visible = GetVisibleInfoBarMessages();
  EXPECT_EQ(2u, visible.size());
  EXPECT_EQ("Critical 1", visible[0]);
  EXPECT_EQ("Critical 2", visible[1]);
}

IN_PROC_BROWSER_TEST_F(InfoBarContainerPriorityTest, DefaultIsSingleVisible) {
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kDefault, "Default 1");
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kDefault,
             "Default 2 (Queued)");

  // Only the first default infobar should be visible.
  std::vector<std::string> visible = GetVisibleInfoBarMessages();
  EXPECT_EQ(1u, visible.size());
  EXPECT_EQ("Default 1", visible[0]);
}

IN_PROC_BROWSER_TEST_F(InfoBarContainerPriorityTest,
                       CriticalBlocksDefaultAndLow) {
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kCriticalSecurity,
             "Critical 1");
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kDefault,
             "Default (Queued)");
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kLow, "Low (Queued)");

  // While a critical infobar is visible, no lower-priority ones are shown.
  std::vector<std::string> visible = GetVisibleInfoBarMessages();
  EXPECT_EQ(1u, visible.size());
  EXPECT_EQ("Critical 1", visible[0]);
}

IN_PROC_BROWSER_TEST_F(InfoBarContainerPriorityTest,
                       PromotionAfterCriticalRemoved) {
  infobars::InfoBar* critical_bar =
      AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kCriticalSecurity,
                 "Critical 1");
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kLow, "Low (Queued)");
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kDefault,
             "Default (Queued)");

  // Initial state: Only critical is visible.
  ASSERT_EQ(1u, GetVisibleInfoBarMessages().size());
  ASSERT_EQ("Critical 1", GetVisibleInfoBarMessages()[0]);

  GetInfoBarManager()->RemoveInfoBar(critical_bar);

  // After critical is removed, default should be promoted (higher priority
  // than low).
  std::vector<std::string> visible = GetVisibleInfoBarMessages();
  EXPECT_EQ(1u, visible.size());
  EXPECT_EQ("Default (Queued)", visible[0]);
}

IN_PROC_BROWSER_TEST_F(InfoBarContainerPriorityTest, FIFOWithinSamePriority) {
  infobars::InfoBar* default1 = AddInfoBar(
      infobars::InfoBarDelegate::InfobarPriority::kDefault, "Default 1");
  infobars::InfoBar* default2 = AddInfoBar(
      infobars::InfoBarDelegate::InfobarPriority::kDefault, "Default 2");
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kDefault, "Default 3");

  // Remove the first default infobar, the second one should appear.
  GetInfoBarManager()->RemoveInfoBar(default1);
  ASSERT_EQ(1u, GetVisibleInfoBarMessages().size());
  EXPECT_EQ("Default 2", GetVisibleInfoBarMessages()[0]);

  // Remove the second, the third should appear.
  GetInfoBarManager()->RemoveInfoBar(default2);
  ASSERT_EQ(1u, GetVisibleInfoBarMessages().size());
  EXPECT_EQ("Default 3", GetVisibleInfoBarMessages()[0]);
}

IN_PROC_BROWSER_TEST_F(InfoBarContainerPriorityTest,
                       DefaultQueuedIfLowIsVisible) {
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kLow, "Low 1");
  ASSERT_EQ(1u, GetVisibleInfoBarMessages().size());
  ASSERT_EQ("Low 1", GetVisibleInfoBarMessages()[0]);

  // Adding a default infobar will queue it because a low-priority one is
  // already occupying the single non-critical slot.
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kDefault,
             "Default (Queued)");
  EXPECT_EQ(1u, GetVisibleInfoBarMessages().size());
  EXPECT_EQ("Low 1", GetVisibleInfoBarMessages()[0]);
}

IN_PROC_BROWSER_TEST_F(InfoBarContainerPriorityTest,
                       TabSwitchingPreservesState) {
  // Setup Tab 1 with a visible critical and a queued default infobar.
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kCriticalSecurity,
             "Critical");
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kDefault,
             "Default (Queued)");
  ASSERT_EQ(1u, GetVisibleInfoBarMessages().size());
  ASSERT_EQ("Critical", GetVisibleInfoBarMessages()[0]);

  // Open and switch to a new tab.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(1);

  // The new tab should have an existing, but empty, infobar container.
  ASSERT_TRUE(GetInfoBarContainer()->IsEmpty());

  // Switch back to the first tab.
  browser()->tab_strip_model()->ActivateTabAt(0);

  // The state should be preserved.
  EXPECT_EQ(1u, GetVisibleInfoBarMessages().size());
  EXPECT_EQ("Critical", GetVisibleInfoBarMessages()[0]);
}

//
// Tests for split tab behavior, parameterized by whether prioritization is
// enabled.
//
class InfoBarContainerSplitTabTest : public InfoBarContainerViewBrowserTest,
                                     public testing::WithParamInterface<bool> {
 public:
  InfoBarContainerSplitTabTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kSideBySide};
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsPrioritizationEnabled()) {
      enabled_features.push_back(infobars::kInfobarPrioritization);
    } else {
      disabled_features.push_back(infobars::kInfobarPrioritization);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool IsPrioritizationEnabled() const { return GetParam(); }

 protected:
  // Splits the tab at `index_to_split` with the currently active tab.
  void SplitTabWithActive(int index_to_split) {
    browser()->tab_strip_model()->AddToNewSplit(
        {index_to_split},
        split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kVertical,
                                       0.5f),
        split_tabs::SplitTabCreatedSource::kToolbarButton);
  }

  // Switches focus between the two panes of the active split tab.
  void SwitchSplitTabFocus() {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    ASSERT_TRUE(tab_strip_model->IsActiveTabSplit());

    // In this test setup with exactly two tabs (0 and 1) involved in a split,
    // switching focus simply means activating the other index.
    const int active_index = tab_strip_model->active_index();
    const int next_index = (active_index == 0) ? 1 : 0;

    tab_strip_model->ActivateTabAt(
        next_index, TabStripUserGestureDetails(
                        TabStripUserGestureDetails::GestureType::kOther));
  }
};

IN_PROC_BROWSER_TEST_P(InfoBarContainerSplitTabTest,
                       InfobarsAreIndependentInSplitTabs) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // 1. Set up two tabs. The browser starts with one tab, so navigate it and
  // add one more.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL("chrome://version"), ui::PAGE_TRANSITION_TYPED));
  tab_strip_model->ActivateTabAt(1);
  ASSERT_EQ(2, tab_strip_model->count());

  // 2. Add infobars to the active tab (index 1).
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kCriticalSecurity,
             "Critical on Tab 2");
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kDefault,
             "Default on Tab 2");

  if (IsPrioritizationEnabled()) {
    ASSERT_EQ(1u, GetVisibleInfoBarMessages().size());
    EXPECT_EQ("Critical on Tab 2", GetVisibleInfoBarMessages()[0]);
  } else {
    ASSERT_EQ(2u, GetVisibleInfoBarMessages().size());
    EXPECT_EQ("Critical on Tab 2", GetVisibleInfoBarMessages()[0]);
    EXPECT_EQ("Default on Tab 2", GetVisibleInfoBarMessages()[1]);
  }

  // 3. Split tab 0 with the active tab (tab 1).
  SplitTabWithActive(0);
  // The tabs are now split but remain as distinct indices in the model.
  ASSERT_EQ(2, tab_strip_model->count());
  ASSERT_TRUE(tab_strip_model->IsActiveTabSplit());

  // 4. Verify the infobars are still visible in the active pane (Tab 1).
  EXPECT_EQ(GURL("chrome://version"),
            tab_strip_model->GetActiveWebContents()->GetURL());
  if (IsPrioritizationEnabled()) {
    EXPECT_EQ(1u, GetVisibleInfoBarMessages().size());
  } else {
    EXPECT_EQ(2u, GetVisibleInfoBarMessages().size());
  }

  // 5. Switch focus to the other pane (which was tab 0).
  SwitchSplitTabFocus();
  EXPECT_EQ(GURL("about:blank"),
            tab_strip_model->GetActiveWebContents()->GetURL());

  // 6. Verify this pane has no infobars.
  EXPECT_TRUE(GetInfoBarContainer()->IsEmpty());
  EXPECT_TRUE(GetVisibleInfoBarMessages().empty());

  // 7. Add an infobar to this second pane.
  AddInfoBar(infobars::InfoBarDelegate::InfobarPriority::kDefault,
             "Default on Tab 1");
  ASSERT_EQ(1u, GetVisibleInfoBarMessages().size());
  EXPECT_EQ("Default on Tab 1", GetVisibleInfoBarMessages()[0]);

  // 8. Switch focus back to the first pane and verify its state is unchanged.
  SwitchSplitTabFocus();
  EXPECT_EQ(GURL("chrome://version"),
            tab_strip_model->GetActiveWebContents()->GetURL());
  if (IsPrioritizationEnabled()) {
    ASSERT_EQ(1u, GetVisibleInfoBarMessages().size());
    EXPECT_EQ("Critical on Tab 2", GetVisibleInfoBarMessages()[0]);
  } else {
    ASSERT_EQ(2u, GetVisibleInfoBarMessages().size());
    EXPECT_EQ("Critical on Tab 2", GetVisibleInfoBarMessages()[0]);
    EXPECT_EQ("Default on Tab 2", GetVisibleInfoBarMessages()[1]);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    InfoBarContainerSplitTabTest,
    testing::Bool(),
    [](const testing::TestParamInfo<InfoBarContainerSplitTabTest::ParamType>&
           info) {
      return info.param ? "PrioritizationEnabled" : "PrioritizationDisabled";
    });
