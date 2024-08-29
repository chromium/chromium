// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view_layout.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
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
#include "ui/views/test/views_test_utils.h"

namespace {

// Save space for the separator.
constexpr int kToolbarHeight = 30 - views::Separator::kThickness;

class MockBrowserViewLayoutDelegate : public BrowserViewLayoutDelegate {
 public:
  MockBrowserViewLayoutDelegate() = default;

  MockBrowserViewLayoutDelegate(const MockBrowserViewLayoutDelegate&) = delete;
  MockBrowserViewLayoutDelegate& operator=(
      const MockBrowserViewLayoutDelegate&) = delete;

  ~MockBrowserViewLayoutDelegate() override = default;

  void set_should_draw_tab_strip(bool visible) {
    should_draw_tab_strip_ = visible;
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
  bool ShouldDrawTabStrip() const override { return should_draw_tab_strip_; }
  bool GetBorderlessModeEnabled() const override { return false; }
  gfx::Rect GetBoundsForTabStripRegionInBrowserView() const override {
    return gfx::Rect();
  }
  gfx::Rect GetBoundsForWebAppFrameToolbarInBrowserView() const override {
    return gfx::Rect();
  }
  void LayoutWebAppWindowTitle(
      const gfx::Rect& available_space,
      views::Label& window_title_label) const override {}
  int GetTopInsetInBrowserView() const override { return 0; }
  bool IsToolbarVisible() const override { return toolbar_visible_; }
  bool IsBookmarkBarVisible() const override { return bookmark_bar_visible_; }
  bool IsContentsSeparatorEnabled() const override {
    return content_separator_enabled_;
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
        supported_features{{
            Browser::FEATURE_TABSTRIP,
            Browser::FEATURE_TOOLBAR,
            Browser::FEATURE_LOCATIONBAR,
            Browser::FEATURE_BOOKMARKBAR,
        }};
    return base::Contains(*supported_features, feature);
  }
  gfx::NativeView GetHostView() const override { return gfx::NativeView(); }
  gfx::NativeView GetHostViewForAnchoring() const override {
    return gfx::NativeView();
  }
  bool BrowserIsSystemWebApp() const override { return false; }
  bool BrowserIsWebApp() const override { return false; }
  bool BrowserIsTypeApp() const override { return false; }
  bool BrowserIsTypeNormal() const override { return true; }
  bool HasFindBarController() const override { return false; }
  void MoveWindowForFindBarIfNecessary() const override {}
  bool IsWindowControlsOverlayEnabled() const override { return false; }
  void UpdateWindowControlsOverlay(const gfx::Rect& rect) override {}
  bool ShouldLayoutTabStrip() const override { return true; }
  int GetExtraInfobarOffset() const override { return 0; }

 private:
  bool should_draw_tab_strip_ = true;
  bool toolbar_visible_ = true;
  bool bookmark_bar_visible_ = true;
  bool content_separator_enabled_ = true;
  bool top_controls_slide_enabled_ = false;
  float top_controls_shown_ratio_ = 1.f;
};

///////////////////////////////////////////////////////////////////////////////

std::unique_ptr<views::View> CreateFixedSizeView(const gfx::Size& size) {
  auto view = std::make_unique<views::View>();
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
  std::unique_ptr<ImmersiveRevealedLock> GetRevealedLock(
      AnimateReveal animate_reveal) override {
    return nullptr;
  }
  void OnFindBarVisibleBoundsChanged(
      const gfx::Rect& new_visible_bounds) override {}
  bool ShouldStayImmersiveAfterExitingFullscreen() override { return true; }
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override {}
  int GetMinimumContentOffset() const override { return 0; }
  int GetExtraInfobarOffset() const override { return 0; }
  void OnContentFullscreenChanged(bool is_content_fullscreen) override {}
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

  BrowserViewLayoutTest(const BrowserViewLayoutTest&) = delete;
  BrowserViewLayoutTest& operator=(const BrowserViewLayoutTest&) = delete;

  ~BrowserViewLayoutTest() override {}

  BrowserViewLayout* layout() { return layout_; }
  MockBrowserViewLayoutDelegate* delegate() { return delegate_; }
  views::View* browser_view() { return browser_view_.get(); }
  views::View* top_container() { return top_container_; }
  TabStrip* tab_strip() { return tab_strip_; }
  views::View* webui_tab_strip() { return webui_tab_strip_; }
  views::View* toolbar() { return toolbar_; }
  views::View* separator() { return separator_; }
  InfoBarContainerView* infobar_container() { return infobar_container_; }
  views::View* contents_container() { return contents_container_; }

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    browser_view_ = CreateFixedSizeView(gfx::Size(800, 600));

    immersive_mode_controller_ =
        std::make_unique<MockImmersiveModeController>();

    top_container_ =
        browser_view_->AddChildView(CreateFixedSizeView(gfx::Size(800, 60)));
    auto tab_strip = std::make_unique<TabStrip>(
        std::make_unique<FakeBaseTabStripController>());
    tab_strip_ = tab_strip.get();
    TabStripRegionView* tab_strip_region_view = top_container_->AddChildView(
        std::make_unique<TabStripRegionView>(std::move(tab_strip)));
    webui_tab_strip_ =
        top_container_->AddChildView(CreateFixedSizeView(gfx::Size(800, 200)));
    webui_tab_strip_->SetVisible(false);
    toolbar_ = top_container_->AddChildView(
        CreateFixedSizeView(gfx::Size(800, kToolbarHeight)));
    separator_ =
        top_container_->AddChildView(std::make_unique<views::Separator>());

    infobar_container_ = browser_view_->AddChildView(
        std::make_unique<InfoBarContainerView>(nullptr));

    side_panel_rounded_corner_ =
        browser_view_->AddChildView(CreateFixedSizeView(gfx::Size(16, 16)));

    contents_container_ =
        browser_view_->AddChildView(CreateFixedSizeView(gfx::Size(800, 600)));
    devtools_web_view_ = contents_container_->AddChildView(
        CreateFixedSizeView(gfx::Size(800, 600)));
    devtools_web_view_->SetVisible(false);
    contents_web_view_ = contents_container_->AddChildView(
        CreateFixedSizeView(gfx::Size(800, 600)));
    contents_container_->SetLayoutManager(
        std::make_unique<ContentsLayoutManager>(devtools_web_view_,
                                                contents_web_view_));

    auto delegate = std::make_unique<MockBrowserViewLayoutDelegate>();
    delegate_ = delegate.get();
    auto layout = std::make_unique<BrowserViewLayout>(
        std::move(delegate),
        /*browser_view=*/nullptr, top_container_,
        /*web_app_frame_toolbar=*/nullptr,
        /*web_app_window_title=*/nullptr, tab_strip_region_view, tab_strip_,
        toolbar_, infobar_container_, contents_container_,
        /*left_aligned_side_panel_separator=*/nullptr,
        /*unified_side_panel=*/nullptr,
        /*right_aligned_side_panel_separator=*/nullptr,
        side_panel_rounded_corner_, immersive_mode_controller_.get(),
        separator_);
    layout->set_webui_tab_strip(webui_tab_strip());
    layout_ = layout.get();
    browser_view_->SetLayoutManager(std::move(layout));
  }

  // For the purposes of this test, boolean values are directly set on a
  // BrowserViewLayoutDelegate which are checked during layout or child view
  // visibility is directly changed. These calls do not schedule a layout and we
  // need to manually invalidate layout.
  void InvalidateAndRunScheduledLayoutOnBrowserView() {
    browser_view()->InvalidateLayout();
    views::test::RunScheduledLayout(browser_view());
  }

 private:
  raw_ptr<BrowserViewLayout, DanglingUntriaged> layout_;
  raw_ptr<MockBrowserViewLayoutDelegate, DanglingUntriaged>
      delegate_;  // Owned by |layout_|.
  std::unique_ptr<views::View> browser_view_;

  // Views owned by |browser_view_|.
  raw_ptr<views::View> top_container_;
  raw_ptr<TabStrip> tab_strip_;
  raw_ptr<views::View> webui_tab_strip_;
  raw_ptr<views::View> toolbar_;
  raw_ptr<views::Separator> separator_;
  raw_ptr<InfoBarContainerView> infobar_container_;
  raw_ptr<views::View> side_panel_rounded_corner_;
  raw_ptr<views::View> contents_container_;
  raw_ptr<views::View> contents_web_view_;
  raw_ptr<views::View> devtools_web_view_;

  std::unique_ptr<MockImmersiveModeController> immersive_mode_controller_;
};

// Test basic construction and initialization.
TEST_F(BrowserViewLayoutTest, BrowserViewLayout) {
  EXPECT_TRUE(layout()->GetWebContentsModalDialogHost());
  EXPECT_FALSE(layout()->IsInfobarVisible());
}

// Test the core layout functions.
TEST_F(BrowserViewLayoutTest, Layout) {
  // Simulate a window with no interesting UI.
  delegate()->set_should_draw_tab_strip(false);
  delegate()->set_toolbar_visible(false);
  delegate()->set_bookmark_bar_visible(false);
  InvalidateAndRunScheduledLayoutOnBrowserView();

  // Top views are zero-height.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), tab_strip()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 0), toolbar()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 0), infobar_container()->bounds());
  // Contents split fills the window.
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), contents_container()->bounds());

  // Turn on the toolbar, like in a pop-up window.
  delegate()->set_toolbar_visible(true);
  InvalidateAndRunScheduledLayoutOnBrowserView();

  // Now the toolbar has bounds and other views shift down.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), tab_strip()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, kToolbarHeight), toolbar()->bounds());
  EXPECT_EQ(gfx::Rect(0, kToolbarHeight, 800, views::Separator::kThickness),
            separator()->bounds());
  EXPECT_EQ(gfx::Rect(0, 30, 800, 0), infobar_container()->bounds());
  EXPECT_EQ(gfx::Rect(0, 30, 800, 570), contents_container()->bounds());

  // Disable the contents separator.
  delegate()->set_content_separator_enabled(false);
  InvalidateAndRunScheduledLayoutOnBrowserView();

  // Now the separator is not visible and the content grows vertically.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), tab_strip()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, kToolbarHeight), toolbar()->bounds());
  EXPECT_FALSE(separator()->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 29, 800, 0), infobar_container()->bounds());
  EXPECT_EQ(gfx::Rect(0, 29, 800, 571), contents_container()->bounds());

  // TODO(jamescook): Tab strip and bookmark bar.
}

TEST_F(BrowserViewLayoutTest, LayoutDownloadShelf) {
  constexpr int kHeight = 50;
  std::unique_ptr<views::View> download_shelf =
      CreateFixedSizeView(gfx::Size(800, kHeight));
  layout()->set_download_shelf(download_shelf.get());

  // If the download shelf isn't visible, it doesn't move the bottom edge.
  download_shelf->SetVisible(false);
  constexpr int kBottom = 500;
  EXPECT_EQ(kBottom, layout()->LayoutDownloadShelf(kBottom));

  // A visible download shelf moves the bottom edge up.
  download_shelf->SetVisible(true);
  constexpr int kTop = kBottom - kHeight;
  EXPECT_EQ(kTop, layout()->LayoutDownloadShelf(kBottom));
  EXPECT_EQ(gfx::Rect(0, kTop, 0, kHeight), download_shelf->bounds());
}

TEST_F(BrowserViewLayoutTest, LayoutContentsWithTopControlsSlideBehavior) {
  // Top controls are fully shown.
  delegate()->set_should_draw_tab_strip(false);
  delegate()->set_toolbar_visible(true);
  delegate()->set_top_controls_slide_enabled(true);
  delegate()->set_top_controls_shown_ratio(1.f);
  InvalidateAndRunScheduledLayoutOnBrowserView();
  EXPECT_EQ(gfx::Rect(0, 0, 800, 30), top_container()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, kToolbarHeight), toolbar()->bounds());
  EXPECT_EQ(gfx::Rect(0, kToolbarHeight, 800, views::Separator::kThickness),
            separator()->bounds());
  EXPECT_EQ(gfx::Rect(0, 30, 800, 570), contents_container()->bounds());

  // Top controls are half shown, half hidden.
  delegate()->set_top_controls_shown_ratio(0.5f);
  InvalidateAndRunScheduledLayoutOnBrowserView();
  EXPECT_EQ(gfx::Rect(0, 0, 800, 30), top_container()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, kToolbarHeight), toolbar()->bounds());
  EXPECT_EQ(gfx::Rect(0, kToolbarHeight, 800, views::Separator::kThickness),
            separator()->bounds());
  EXPECT_EQ(gfx::Rect(0, 30, 800, 570), contents_container()->bounds());

  // Top controls are fully hidden. the contents are expanded in height by an
  // amount equal to the top controls height.
  delegate()->set_top_controls_shown_ratio(0.f);
  InvalidateAndRunScheduledLayoutOnBrowserView();
  EXPECT_EQ(gfx::Rect(0, -30, 800, 30), top_container()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, kToolbarHeight), toolbar()->bounds());
  EXPECT_EQ(gfx::Rect(0, kToolbarHeight, 800, views::Separator::kThickness),
            separator()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), contents_container()->bounds());
}

TEST_F(BrowserViewLayoutTest, WebUITabStripPushesDownContents) {
  delegate()->set_should_draw_tab_strip(false);
  delegate()->set_toolbar_visible(true);
  webui_tab_strip()->SetVisible(false);
  InvalidateAndRunScheduledLayoutOnBrowserView();
  const gfx::Rect original_contents_bounds = contents_container()->bounds();
  EXPECT_EQ(gfx::Size(), webui_tab_strip()->size());

  webui_tab_strip()->SetVisible(true);
  InvalidateAndRunScheduledLayoutOnBrowserView();
  EXPECT_LT(0, webui_tab_strip()->size().height());
  EXPECT_EQ(original_contents_bounds.size(), contents_container()->size());
  EXPECT_EQ(webui_tab_strip()->size().height(),
            contents_container()->bounds().y() - original_contents_bounds.y());
}
