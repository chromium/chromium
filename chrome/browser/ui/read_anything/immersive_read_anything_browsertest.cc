// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/views/view.h"

class ImmersiveReadAnythingBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ImmersiveReadAnythingBrowserTest() {
    feature_list_.InitWithFeatureState(features::kImmersiveReadAnything,
                                       IsImmersiveEnabled());
  }

  void SetUpOnMainThread() override {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    ASSERT_NE(browser_view, nullptr);
    browser_view->GetWidget()->Show();
  }

 protected:
  bool IsImmersiveEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ImmersiveReadAnythingBrowserTest,
                       OverlayExistsIfImmersiveFeatureEnabled) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* contents_container =
      browser_view->GetActiveContentsContainerView();
  ASSERT_NE(contents_container, nullptr);

  views::View* overlay_view = nullptr;
  int overlay_count = 0;

  for (views::View* child : contents_container->children()) {
    if (child->GetID() == VIEW_ID_READ_ANYTHING_OVERLAY) {
      overlay_view = child;
      overlay_count++;
    }
  }

  if (IsImmersiveEnabled()) {
    ASSERT_NE(overlay_view, nullptr)
        << "Overlay should exist when Immersive Reading Mode is enabled.";
    EXPECT_FALSE(overlay_view->GetVisible()) << "Overlay should be hidden.";
  } else {
    EXPECT_EQ(nullptr, overlay_view);
    EXPECT_EQ(0, overlay_count);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ImmersiveReadAnythingBrowserTest,
                         testing::Bool());

class ImmersiveReadAnythingSplitViewBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ImmersiveReadAnythingSplitViewBrowserTest() {
    feature_list_.InitWithFeatureStates({
        {features::kSideBySide, true},
        {features::kImmersiveReadAnything, IsImmersiveEnabled()},
    });
  }

  void SetUpOnMainThread() override {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    ASSERT_NE(browser_view, nullptr);
    browser_view->GetWidget()->Show();

    chrome::NewTab(browser());
    browser()->tab_strip_model()->ActivateTabAt(0);
    std::vector<int> other_tab_indices = {1};
    split_tabs::SplitTabVisualData visual_data;
    split_tabs::SplitTabCreatedSource source =
        split_tabs::SplitTabCreatedSource::kToolbarButton;
    browser()->tab_strip_model()->AddToNewSplit(other_tab_indices, visual_data,
                                                source);
  }

 protected:
  bool IsImmersiveEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ImmersiveReadAnythingSplitViewBrowserTest,
                       OverlayExistsOnSplitViewsWhenImmersiveIsEnabled) {
  const auto ContainsReadAnythingOverlay = [](views::View* container) {
    if (!container) {
      return false;
    }

    for (views::View* child : container->children()) {
      if (child->GetID() == VIEW_ID_READ_ANYTHING_OVERLAY) {
        return true;
      }
    }

    return false;
  };

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* container_0 = browser_view->GetActiveContentsContainerView();
  ASSERT_NE(container_0, nullptr);
  ASSERT_EQ(ContainsReadAnythingOverlay(container_0), IsImmersiveEnabled());

  browser()->tab_strip_model()->ActivateTabAt(1);
  views::View* container_1 = browser_view->GetActiveContentsContainerView();
  ASSERT_NE(container_1, nullptr);
  ASSERT_NE(container_0, container_1);
  ASSERT_EQ(ContainsReadAnythingOverlay(container_1), IsImmersiveEnabled());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ImmersiveReadAnythingSplitViewBrowserTest,
                         testing::Bool());
