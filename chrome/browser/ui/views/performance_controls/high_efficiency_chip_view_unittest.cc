// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/high_efficiency_chip_view.h"

#include "chrome/browser/ui/performance_controls/tab_discard_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"

class TestPageActionIconDelegate : public IconLabelBubbleView::Delegate,
                                   public PageActionIconView::Delegate {
 public:
  TestPageActionIconDelegate() = default;
  virtual ~TestPageActionIconDelegate() = default;

  // IconLabelBubbleView::Delegate:
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override {
    return gfx::kPlaceholderColor;
  }
  SkColor GetIconLabelBubbleBackgroundColor() const override {
    return gfx::kPlaceholderColor;
  }

  // PageActionIconView::Delegate:
  content::WebContents* GetWebContentsForPageActionIconView() override {
    return web_contents_;
  }

  void SetWebContents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }

 private:
  content::WebContents* web_contents_;
};

class DiscardMockNavigationHandle : public content::MockNavigationHandle {
 public:
  void SetWasDiscarded(bool was_discarded) { was_discarded_ = was_discarded; }
  bool ExistingDocumentWasDiscarded() const override { return was_discarded_; }

 private:
  bool was_discarded_ = false;
};

class TestHighEfficiencyChipView : public HighEfficiencyChipView {
 public:
  explicit TestHighEfficiencyChipView(
      CommandUpdater* command_updater,
      Browser* browser,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate)
      : HighEfficiencyChipView(command_updater,
                               browser,
                               icon_label_bubble_delegate,
                               page_action_icon_delegate) {}

  bool IsLabelVisible() { return label()->GetVisible(); }
};

class HighEfficiencyChipViewTest : public ChromeViewsTestBase {
 public:
 protected:
  HighEfficiencyChipViewTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    profile_ = std::make_unique<TestingProfile>();
    Browser::CreateParams browser_params(profile_.get(), true);
    window_ = std::make_unique<TestBrowserWindow>();
    browser_params.window = window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(browser_params));

    widget_ = CreateTestWidget();
    delegate_ = TestPageActionIconDelegate();
    view_ =
        widget_->SetContentsView(std::make_unique<TestHighEfficiencyChipView>(
            /*command_updater=*/nullptr, browser(), delegate(), delegate()));

    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    browser_.reset();
    window_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void SetTabDiscardState(bool is_discarded) {
    TabDiscardTabHelper* tab_helper = TabDiscardTabHelper::FromWebContents(
        delegate()->GetWebContentsForPageActionIconView());
    std::unique_ptr<DiscardMockNavigationHandle> navigation_handle =
        std::make_unique<DiscardMockNavigationHandle>();
    navigation_handle.get()->SetWasDiscarded(is_discarded);
    tab_helper->DidStartNavigation(navigation_handle.get());
  }

  TestHighEfficiencyChipView* view() { return view_; }
  views::Widget* widget() { return widget_.get(); }
  TestPageActionIconDelegate* delegate() { return &delegate_; }

  Profile* profile() { return profile_.get(); }
  Browser* browser() { return browser_.get(); }

 private:
  TestPageActionIconDelegate delegate_;
  raw_ptr<TestHighEfficiencyChipView> view_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<BrowserWindow> window_;
};

// When the previous page has a tab discard state of true, when the icon is
// updated it should be visible.
TEST_F(HighEfficiencyChipViewTest, ShouldShowForDiscardedPage) {
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories;

  // Initialize WebContents with the TabDiscardHelper
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  TabDiscardTabHelper::CreateForWebContents(web_contents.get());
  delegate()->SetWebContents(web_contents.get());

  SetTabDiscardState(true);
  view()->Update();
  EXPECT_TRUE(view()->GetVisible());
}

// When the previous page was not previously discarded, the icon should not be
// visible.
TEST_F(HighEfficiencyChipViewTest, ShouldNotShowForRegularPage) {
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories;

  // Initialize WebContents with the TabDiscardHelper
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  TabDiscardTabHelper::CreateForWebContents(web_contents.get());
  delegate()->SetWebContents(web_contents.get());

  SetTabDiscardState(false);
  view()->Update();
  EXPECT_FALSE(view()->GetVisible());
}

// When the previous page was not previously discarded, the icon should not be
// visible.
TEST_F(HighEfficiencyChipViewTest, ShouldHideLabelAfterThreeTimes) {
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories;

  // Initialize WebContents with the TabDiscardHelper
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  TabDiscardTabHelper::CreateForWebContents(web_contents.get());
  delegate()->SetWebContents(web_contents.get());

  // Open the tab 3 times with the label being visible.
  for (int i = 0; i < 3; i++) {
    SetTabDiscardState(true);
    view()->Update();
    EXPECT_TRUE(view()->IsLabelVisible());
    SetTabDiscardState(false);
    view()->Update();
  }

  // On the 4th time, the label should be hidden.
  SetTabDiscardState(true);
  view()->Update();
  EXPECT_FALSE(view()->IsLabelVisible());
}
