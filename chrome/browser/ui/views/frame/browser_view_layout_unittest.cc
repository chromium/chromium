// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view_layout.h"

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/contents_layout_manager.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/separator.h"

namespace {

// Save space for the separator.
constexpr int kToolbarHeight = 30 - views::Separator::kThickness;

class MockBrowserViewLayoutDelegate : public BrowserViewLayoutDelegate {
 public:
  MockBrowserViewLayoutDelegate() = default;
  ~MockBrowserViewLayoutDelegate() override = default;

  void set_download_shelf_needs_layout(bool layout) {
    download_shelf_needs_layout_ = layout;
  }
  void set_tab_strip_visible(bool visible) {
    tab_strip_visible_ = visible;
  }
  void set_toolbar_visible(bool visible) {
    toolbar_visible_ = visible;
  }
  void set_bookmark_bar_visible(bool visible) {
    bookmark_bar_visible_ = visible;
  }
  void set_content_separator_enabled(bool visible) {
    content_separator_enabled_ = visible;
  }
  void set_top_controls_slide_enabled(bool enabled) {
    top_controls_slide_enabled_ = enabled;
  }
  void set_top_controls_shown_ratio(float ratio) {
    top_controls_shown_ratio_ = ratio;
  }

  // BrowserViewLayout::Delegate overrides:
  bool IsTabStripVisible() const override { return tab_strip_visible_; }
  gfx::Rect GetBoundsForTabStripRegionInBrowserView() const override {
    return gfx::Rect();
  }
  int GetTopInsetInBrowserView() const override { return 0; }
  int GetThemeBackgroundXInset() const override { return 0; }
  bool IsToolbarVisible() const override { return toolbar_visible_; }
  bool IsBookmarkBarVisible() const override { return bookmark_bar_visible_; }
  bool IsContentsSeparatorEnabled() const override {
    return content_separator_enabled_;
  }
  bool DownloadShelfNeedsLayout() const override {
    return download_shelf_needs_layout_;
  }

  ExclusiveAccessBubbleViews* GetExclusiveAccessBubble() const override {
    return nullptr;
  }
  bool IsTopControlsSlideBehaviorEnabled() const override {
    return top_controls_slide_enabled_;
  }
  float GetTopControlsSlideBehaviorShownRatio() const override {
    return top_controls_shown_ratio_;
  }
  bool SupportsWindowFeature(
      const Browser::WindowFeature feature) const override {
    static const base::NoDestructor<base::flat_set<Browser::WindowFeature>>
        supported_features({
            Browser::FEATURE_TABSTRIP,
            Browser::FEATURE_TOOLBAR,
            Browser::FEATURE_LOCATIONBAR,
            Browser::FEATURE_BOOKMARKBAR,
            Browser::FEATURE_DOWNLOADSHELF,
        });
    return base::Contains(*supported_features, feature);
  }
  gfx::NativeView GetHostView() const override { return nullptr; }
  bool BrowserIsTypeNormal() const override { return true; }
  bool HasFindBarController() const override { return false; }
  void MoveWindowForFindBarIfNecessary() const override {}

 private:
  bool tab_strip_visible_ = true;
  bool toolbar_visible_ = true;
  bool bookmark_bar_visible_ = true;
  bool content_separator_enabled_ = true;
  bool download_shelf_needs_layout_ = false;
  bool top_controls_slide_enabled_ = false;
  float top_controls_shown_ratio_ = 1.f;

  DISALLOW_COPY_AND_ASSIGN(MockBrowserViewLayoutDelegate);
};

///////////////////////////////////////////////////////////////////////////////

views::View* CreateFixedSizeView(const gfx::Size& size) {
  auto* view = new views::View;
  view->SetPreferredSize(size);
  view->SizeToPreferredSize();
  return view;
}

///////////////////////////////////////////////////////////////////////////////

class MockImmersiveModeController : public ImmersiveModeController {
 public:
  // ImmersiveModeController overrides:
  void Init(BrowserView* browser_view) override {}
  void SetEnabled(bool enabled) override {}
  bool IsEnabled() const override { return false; }
  bool ShouldHideTopViews() const override { return false; }
  bool IsRevealed() const override { return false; }
  int GetTopContainerVerticalOffset(
      const gfx::Size& top_container_size) const override {
    return 0;
  }
  ImmersiveRevealedLock* GetRevealedLock(AnimateReveal animate_reveal) override
      WARN_UNUSED_RESULT {
    return nullptr;
  }
  void OnFindBarVisibleBoundsChanged(
      const gfx::Rect& new_visible_bounds) override {}
  bool ShouldStayImmersiveAfterExitingFullscreen() override { return true; }
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override {}
};

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// Tests of BrowserViewLayout. Runs tests without constructing a BrowserView.
class BrowserViewLayoutTest : public ChromeViewsTestBase {
 public:
  BrowserViewLayoutTest()
      : delegate_(nullptr),
        top_container_(nullptr),
        tab_strip_(nullptr),
        toolbar_(nullptr),
        infobar_container_(nullptr),
        contents_container_(nullptr),
        contents_web_view_(nullptr),
        devtools_web_view_(nullptr) {}
  ~BrowserViewLayoutTest() override {}

  BrowserViewLayout* layout() { return layout_.get(); }
  MockBrowserViewLayoutDelegate* delegate() { return delegate_; }
  views::View* root_view() { return root_view_.get(); }
  views::View* top_container() { return top_container_; }
  TabStrip* tab_strip() { return tab_strip_; }
  views::View* webui_tab_strip() { return webui_tab_strip_; }
  views::View* toolbar() { return toolbar_; }
  views::View* separator() { return separator_; }
  InfoBarContainerView* infobar_container() { return infobar_container_; }
  views::View* contents_container() { return contents_container_; }

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    root_view_.reset(CreateFixedSizeView(gfx::Size(800, 600)));

    immersive_mode_controller_ =
        std::make_unique<MockImmersiveModeController>();

    top_container_ = CreateFixedSizeView(gfx::Size(800, 60));
    views::View* tab_strip_region_view = new TabStripRegionView();
    tab_strip_ = new TabStrip(std::make_unique<FakeBaseTabStripController>());
    top_container_->AddChildView(tab_strip_region_view);
    tab_strip_region_view->AddChildView(tab_strip_);
    webui_tab_strip_ = CreateFixedSizeView(gfx::Size(800, 200));
    webui_tab_strip_->SetVisible(false);
    top_container_->AddChildView(webui_tab_strip_);
    toolbar_ = CreateFixedSizeView(gfx::Size(800, kToolbarHeight));
    top_container_->AddChildView(toolbar_);
    separator_ =
        top_container_->AddChildView(std::make_unique<views::Separator>());
    root_view_->AddChildView(top_container_);

    infobar_container_ = new InfoBarContainerView(nullptr);
    root_view_->AddChildView(infobar_container_);

    contents_web_view_ = CreateFixedSizeView(gfx::Size(800, 600));
    devtools_web_view_ = CreateFixedSizeView(gfx::Size(800, 600));
    devtools_web_view_->SetVisible(false);

    contents_container_ = CreateFixedSizeView(gfx::Size(800, 600));
    contents_container_->AddChildView(devtools_web_view_);
    contents_container_->AddChildView(contents_web_view_);
    contents_container_->SetLayoutManager(
        std::make_unique<ContentsLayoutManager>(devtools_web_view_,
                                                contents_web_view_));

    root_view_->AddChildView(contents_container_);

    // TODO(jamescook): Attach |layout_| to |root_view_|?
    delegate_ = new MockBrowserViewLayoutDelegate();
    layout_ = std::make_unique<BrowserViewLayout>(
        std::unique_ptr<BrowserViewLayoutDelegate>(delegate_),
        nullptr,  // NativeView.
        nullptr,  // BrowserView.
        top_container_, tab_strip_region_view, tab_strip_, toolbar_,
        infobar_container_, contents_container_,
        immersive_mode_controller_.get(), nullptr, separator_);
    layout_->set_webui_tab_strip(webui_tab_strip());
  }

 private:
  std::unique_ptr<BrowserViewLayout> layout_;
  MockBrowserViewLayoutDelegate* delegate_;  // Owned by |layout_|.
  std::unique_ptr<views::View> root_view_;

  // Views owned by |root_view_|.
  views::View* top_container_;
  TabStrip* tab_strip_;
  views::View* webui_tab_strip_;
  views::View* toolbar_;
  views::Separator* separator_;
  InfoBarContainerView* infobar_container_;
  views::View* contents_container_;
  views::View* contents_web_view_;
  views::View* devtools_web_view_;

  std::unique_ptr<MockImmersiveModeController> immersive_mode_controller_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewLayoutTest);
};

// Test basic construction and initialization.
TEST_F(BrowserViewLayoutTest, BrowserViewLayout) {
  EXPECT_TRUE(layout()->GetWebContentsModalDialogHost());
  EXPECT_FALSE(layout()->IsInfobarVisible());
}

// Test the core layout functions.
TEST_F(BrowserViewLayoutTest, Layout) {
  // Simulate a window with no interesting UI.
  delegate()->set_tab_strip_visible(false);
  delegate()->set_toolbar_visible(false);
  delegate()->set_bookmark_bar_visible(false);
  layout()->Layout(root_view());

  // Top views are zero-height.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), tab_strip()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 0), toolbar()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 0), infobar_container()->bounds());
  // Contents split fills the window.
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), contents_container()->bounds());

  // Turn on the toolbar, like in a pop-up window.
  delegate()->set_toolbar_visible(true);
  layout()->Layout(root_view());

  // Now the toolbar has bounds and other views shift down.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), tab_strip()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, kToolbarHeight), toolbar()->bounds());
  EXPECT_EQ(gfx::Rect(0, kToolbarHeight, 800, views::Separator::kThickness),
            separator()->bounds());
  EXPECT_EQ(gfx::Rect(0, 30, 800, 0), infobar_container()->bounds());
  EXPECT_EQ(gfx::Rect(0, 30, 800, 570), contents_container()->bounds());

  // Disable the contents separator.
  delegate()->set_content_separator_enabled(false);
  layout()->Layout(root_view());

  // Now the separator is not visible and the content grows vertically.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), tab_strip()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, kToolbarHeight), toolbar()->bounds());
  EXPECT_FALSE(separator()->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 29, 800, 0), infobar_container()->bounds());
  EXPECT_EQ(gfx::Rect(0, 29, 800, 571), contents_container()->bounds());

  // TODO(jamescook): Tab strip and bookmark bar.
}

TEST_F(BrowserViewLayoutTest, LayoutDownloadShelf) {
  std::unique_ptr<views::View> download_shelf(
      CreateFixedSizeView(gfx::Size(800, 50)));
  layout()->set_download_shelf(download_shelf.get());

  // If download shelf doesn't need layout, it doesn't move the bottom edge.
  delegate()->set_download_shelf_needs_layout(false);
  const int kBottom = 500;
  EXPECT_EQ(kBottom, layout()->LayoutDownloadShelf(kBottom));

  // Download shelf layout moves up the bottom edge and sets visibility.
  delegate()->set_download_shelf_needs_layout(true);
  download_shelf->SetVisible(false);
  EXPECT_EQ(450, layout()->LayoutDownloadShelf(kBottom));
  EXPECT_TRUE(download_shelf->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 450, 0, 50), download_shelf->bounds());
}

TEST_F(BrowserViewLayoutTest, LayoutContentsWithTopControlsSlideBehavior) {
  // Top controls are fully shown.
  delegate()->set_tab_strip_visible(false);
  delegate()->set_toolbar_visible(true);
  delegate()->set_top_controls_slide_enabled(true);
  delegate()->set_top_controls_shown_ratio(1.f);
  layout()->Layout(root_view());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 30), top_container()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, kToolbarHeight), toolbar()->bounds());
  EXPECT_EQ(gfx::Rect(0, kToolbarHeight, 800, views::Separator::kThickness),
            separator()->bounds());
  EXPECT_EQ(gfx::Rect(0, 30, 800, 570), contents_container()->bounds());

  // Top controls are half shown, half hidden.
  delegate()->set_top_controls_shown_ratio(0.5f);
  layout()->Layout(root_view());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 30), top_container()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, kToolbarHeight), toolbar()->bounds());
  EXPECT_EQ(gfx::Rect(0, kToolbarHeight, 800, views::Separator::kThickness),
            separator()->bounds());
  EXPECT_EQ(gfx::Rect(0, 30, 800, 570), contents_container()->bounds());

  // Top controls are fully hidden. the contents are expanded in height by an
  // amount equal to the top controls height.
  delegate()->set_top_controls_shown_ratio(0.f);
  layout()->Layout(root_view());
  EXPECT_EQ(gfx::Rect(0, -30, 800, 30), top_container()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, kToolbarHeight), toolbar()->bounds());
  EXPECT_EQ(gfx::Rect(0, kToolbarHeight, 800, views::Separator::kThickness),
            separator()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), contents_container()->bounds());
}

TEST_F(BrowserViewLayoutTest, WebUITabStripPushesDownContents) {
  delegate()->set_tab_strip_visible(false);
  delegate()->set_toolbar_visible(true);
  webui_tab_strip()->SetVisible(false);
  layout()->Layout(root_view());
  const gfx::Rect original_contents_bounds = contents_container()->bounds();
  EXPECT_EQ(gfx::Size(), webui_tab_strip()->size());

  webui_tab_strip()->SetVisible(true);
  layout()->Layout(root_view());
  EXPECT_LT(0, webui_tab_strip()->size().height());
  EXPECT_EQ(original_contents_bounds.size(), contents_container()->size());
  EXPECT_EQ(webui_tab_strip()->size().height(),
            contents_container()->bounds().y() - original_contents_bounds.y());
}
