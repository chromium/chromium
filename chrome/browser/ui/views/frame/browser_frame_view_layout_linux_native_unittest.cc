// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux_native.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/font_list.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/linux/window_frame_provider.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/test/views_test_utils.h"

namespace {

constexpr int kWindowWidth = 500;

constexpr gfx::Size kCloseButtonSize = gfx::Size(2, 3);
constexpr gfx::Size kMaximizeButtonSize = gfx::Size(5, 7);
constexpr gfx::Size kMinimizeButtonSize = gfx::Size(11, 13);

constexpr gfx::Insets kCloseButtonMargin = gfx::Insets::TLBR(17, 19, 23, 29);
constexpr gfx::Insets kMaximizeButtonMargin = gfx::Insets::TLBR(31, 37, 41, 43);
constexpr gfx::Insets kMinimizeButtonMargin = gfx::Insets::TLBR(47, 53, 59, 61);

constexpr gfx::Insets kTopAreaSpacing = gfx::Insets::TLBR(67, 71, 73, 79);

constexpr int kInterNavButtonSpacing = 83;

static gfx::ImageSkia GetTestImageForSize(gfx::Size size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size.width(), size.height());
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

class TestLayoutDelegate : public OpaqueBrowserFrameViewLayoutDelegate {
 public:
  TestLayoutDelegate() = default;

  TestLayoutDelegate(const TestLayoutDelegate&) = delete;
  TestLayoutDelegate& operator=(const TestLayoutDelegate&) = delete;

  ~TestLayoutDelegate() override = default;

  // OpaqueBrowserFrameViewLayoutDelegate:
  bool ShouldShowWindowIcon() const override { return show_window_icon_; }
  bool ShouldShowWindowTitle() const override { return show_window_title_; }
  std::u16string GetWindowTitle() const override { return window_title_; }
  int GetIconSize() const override { return 17; }
  gfx::Size GetBrowserViewMinimumSize() const override {
    return gfx::Size(168, 64);
  }
  bool ShouldShowCaptionButtons() const override { return true; }
  bool IsRegularOrGuestSession() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }
  bool IsMaximized() const override { return maximized_; }
  bool IsMinimized() const override { return false; }
  bool IsFullscreen() const override { return false; }
  bool IsTabStripVisible() const override { return tab_strip_visible_; }
  bool GetUnframedModeEnabled() const override { return false; }
  bool IsToolbarVisible() const override { return true; }
  int GetTopAreaHeight() const override { return 0; }
  bool UseCustomFrame() const override { return true; }
  bool IsFrameCondensed() const override { return maximized_; }
  void UpdateWindowControlsOverlay(const gfx::Rect& bounding_rect) override {}
  bool ShouldDrawRestoredFrameShadow() const override { return true; }
#if BUILDFLAG(IS_LINUX)
  bool IsTiled() const override { return tiled_; }
#endif
  int WebAppButtonHeight() const override { return web_app_button_height_; }

  void set_maximized(bool maximized) { maximized_ = maximized; }
  void set_tiled(bool tiled) { tiled_ = tiled; }
  void set_tab_strip_visible(bool visible) { tab_strip_visible_ = visible; }
  void set_show_window_icon(bool show) { show_window_icon_ = show; }
  void set_show_window_title(bool show) { show_window_title_ = show; }
  void set_window_title(std::u16string title) {
    window_title_ = std::move(title);
  }
  void set_web_app_button_height(int height) {
    web_app_button_height_ = height;
  }

 private:
  bool maximized_ = false;
  bool tiled_ = false;
  bool tab_strip_visible_ = true;
  bool show_window_icon_ = false;
  bool show_window_title_ = false;
  std::u16string window_title_;
  int web_app_button_height_ = 0;
};

class TestNavButtonProvider : public ui::NavButtonProvider {
 public:
  TestNavButtonProvider() = default;

  ~TestNavButtonProvider() override = default;

  void RedrawImages(int top_area_height, bool maximized, bool active) override {
    ASSERT_EQ(false, maximized);  // This only tests the restored state.
  }

  gfx::ImageSkia GetImage(
      ui::NavButtonProvider::FrameButtonDisplayType type,
      ui::NavButtonProvider::ButtonState state) const override {
    switch (type) {
      case ui::NavButtonProvider::FrameButtonDisplayType::kClose:
        return GetTestImageForSize(kCloseButtonSize);
      case ui::NavButtonProvider::FrameButtonDisplayType::kMaximize:
      case ui::NavButtonProvider::FrameButtonDisplayType::kRestore:
        return GetTestImageForSize(kMaximizeButtonSize);
      case ui::NavButtonProvider::FrameButtonDisplayType::kMinimize:
        return GetTestImageForSize(kMinimizeButtonSize);
      default:
        NOTREACHED();
    }
  }

  gfx::Insets GetNavButtonMargin(
      ui::NavButtonProvider::FrameButtonDisplayType type) const override {
    switch (type) {
      case ui::NavButtonProvider::FrameButtonDisplayType::kClose:
        return kCloseButtonMargin;
      case ui::NavButtonProvider::FrameButtonDisplayType::kMaximize:
      case ui::NavButtonProvider::FrameButtonDisplayType::kRestore:
        return kMaximizeButtonMargin;
      case ui::NavButtonProvider::FrameButtonDisplayType::kMinimize:
        return kMinimizeButtonMargin;
      default:
        NOTREACHED();
    }
  }

  gfx::Insets GetTopAreaSpacing() const override { return top_area_spacing_; }

  int GetNavButtonHeight(bool maximized) const override {
    return maximized ? maximized_nav_button_height_ : nav_button_height_;
  }

  int GetInterNavButtonSpacing() const override {
    return kInterNavButtonSpacing;
  }

  void set_top_area_spacing(const gfx::Insets& spacing) {
    top_area_spacing_ = spacing;
  }
  void set_nav_button_height(int height) { nav_button_height_ = height; }
  void set_maximized_nav_button_height(int height) {
    maximized_nav_button_height_ = height;
  }

 private:
  gfx::Insets top_area_spacing_ = kTopAreaSpacing;
  int nav_button_height_ = 0;
  int maximized_nav_button_height_ = 0;
};

class TestFrameProvider : public ui::WindowFrameProvider {
 public:
  TestFrameProvider() = default;

  ~TestFrameProvider() override = default;

  // ui::WindowFrameProvider:
  int GetTopCornerRadiusDip() override { return 0; }
  bool IsTopFrameTranslucent() override { return false; }
  gfx::Insets GetFrameThicknessDip() override { return frame_thickness_; }
  int GetTopAreaMinHeightDip() override { return top_area_min_height_; }
  gfx::Insets GetTopAreaPaddingDip() override { return top_area_padding_; }
  gfx::Insets GetTopAreaBorderDip() override { return top_area_border_; }
  void PaintWindowFrame(gfx::Canvas* canvas,
                        const gfx::Rect& rect,
                        int top_area_height,
                        bool focused,
                        const gfx::Insets& input_insets) override {}

  void set_frame_thickness(const gfx::Insets& frame_thickness) {
    frame_thickness_ = frame_thickness;
  }
  void set_top_area_min_height(int min_height) {
    top_area_min_height_ = min_height;
  }
  void set_top_area_padding(const gfx::Insets& padding) {
    top_area_padding_ = padding;
  }
  void set_top_area_border(const gfx::Insets& border) {
    top_area_border_ = border;
  }

 private:
  gfx::Insets frame_thickness_;
  int top_area_min_height_ = 24;
  gfx::Insets top_area_padding_ = gfx::Insets::VH(6, 0);
  gfx::Insets top_area_border_;
};

}  // namespace

class BrowserFrameViewLayoutLinuxNativeTest : public ChromeViewsTestBase {
 public:
  BrowserFrameViewLayoutLinuxNativeTest() = default;

  BrowserFrameViewLayoutLinuxNativeTest(
      const BrowserFrameViewLayoutLinuxNativeTest&) = delete;
  BrowserFrameViewLayoutLinuxNativeTest& operator=(
      const BrowserFrameViewLayoutLinuxNativeTest&) = delete;

  ~BrowserFrameViewLayoutLinuxNativeTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    delegate_ = std::make_unique<TestLayoutDelegate>();
    nav_button_provider_ = std::make_unique<::TestNavButtonProvider>();
    frame_provider_ = std::make_unique<::TestFrameProvider>();
    maximized_frame_provider_ = std::make_unique<::TestFrameProvider>();
    auto layout = std::make_unique<BrowserFrameViewLayoutLinuxNative>(
        nav_button_provider_.get(),
        base::BindRepeating(
            [](ui::WindowFrameProvider* frame_provider,
               ui::WindowFrameProvider* maximized_frame_provider, bool tiled,
               bool maximized) {
              return maximized ? maximized_frame_provider : frame_provider;
            },
            frame_provider_.get(), maximized_frame_provider_.get()));
    layout->set_delegate(delegate_.get());
    layout->set_forced_window_caption_spacing_for_test(0);
    widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    root_view_ = widget_->GetRootView();
    root_view_->SetSize(gfx::Size(kWindowWidth, kWindowWidth));
    layout_manager_ = root_view_->SetLayoutManager(std::move(layout));

    minimize_button_ = InitWindowCaptionButton(VIEW_ID_MINIMIZE_BUTTON);
    maximize_button_ = InitWindowCaptionButton(VIEW_ID_MAXIMIZE_BUTTON);
    restore_button_ = InitWindowCaptionButton(VIEW_ID_RESTORE_BUTTON);
    close_button_ = InitWindowCaptionButton(VIEW_ID_CLOSE_BUTTON);
  }

  void TearDown() override {
    window_icon_ = nullptr;
    window_title_ = nullptr;
    widget_.reset();

    ChromeViewsTestBase::TearDown();
  }

 protected:
  views::ImageButton* InitWindowCaptionButton(ViewID view_id) {
    auto button = std::make_unique<views::ImageButton>();
    button->SetID(view_id);
    return root_view_->AddChildView(std::move(button));
  }

  void ResetNativeNavButtonImagesFromButtonProvider() {
    struct {
      raw_ptr<views::ImageButton, DanglingUntriaged> button;
      ui::NavButtonProvider::FrameButtonDisplayType type;
    } const kButtons[] = {
        {minimize_button_,
         ui::NavButtonProvider::FrameButtonDisplayType::kMinimize},
        {maximize_button_,
         ui::NavButtonProvider::FrameButtonDisplayType::kMaximize},
        {close_button_, ui::NavButtonProvider::FrameButtonDisplayType::kClose},
    };
    struct {
      views::Button::ButtonState button_state;
      ui::NavButtonProvider::ButtonState nav_button_provider_state;
    } const kStates[] = {
        {views::Button::STATE_NORMAL,
         ui::NavButtonProvider::ButtonState::kNormal},
        {views::Button::STATE_HOVERED,
         ui::NavButtonProvider::ButtonState::kHovered},
        {views::Button::STATE_PRESSED,
         ui::NavButtonProvider::ButtonState::kPressed},
    };

    for (const auto& button : kButtons) {
      for (const auto& state : kStates) {
        button.button->SetImageModel(
            state.button_state,
            ui::ImageModel::FromImageSkia(nav_button_provider_->GetImage(
                button.type, state.nav_button_provider_state)));
      }
    }
  }

  gfx::Insets FrameInsets() const {
    return static_cast<OpaqueBrowserFrameViewLayout*>(layout_manager_)
        ->FrameEdgeInsets(false);
  }

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View, DanglingUntriaged> root_view_ = nullptr;
  raw_ptr<BrowserFrameViewLayoutLinuxNative, DanglingUntriaged>
      layout_manager_ = nullptr;
  std::unique_ptr<TestLayoutDelegate> delegate_;
  std::unique_ptr<::TestNavButtonProvider> nav_button_provider_;
  std::unique_ptr<TestFrameProvider> frame_provider_;
  std::unique_ptr<TestFrameProvider> maximized_frame_provider_;

  // Widgets:
  raw_ptr<views::ImageButton, DanglingUntriaged> minimize_button_ = nullptr;
  raw_ptr<views::ImageButton, DanglingUntriaged> maximize_button_ = nullptr;
  raw_ptr<views::ImageButton, DanglingUntriaged> restore_button_ = nullptr;
  raw_ptr<views::ImageButton, DanglingUntriaged> close_button_ = nullptr;
  raw_ptr<views::View> window_icon_ = nullptr;
  raw_ptr<views::Label> window_title_ = nullptr;

  void AddWindowTitleAndIcon() {
    auto icon = std::make_unique<views::View>();
    icon->SetID(VIEW_ID_WINDOW_ICON);
    window_icon_ = root_view_->AddChildView(std::move(icon));

    auto title = std::make_unique<views::Label>(delegate_->GetWindowTitle());
    title->SetVisible(delegate_->ShouldShowWindowTitle());
    title->SetSubpixelRenderingEnabled(false);
    title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title->SetID(VIEW_ID_WINDOW_TITLE);
    window_title_ = root_view_->AddChildView(std::move(title));
  }
};

// Tests layout of native navigation buttons.
TEST_F(BrowserFrameViewLayoutLinuxNativeTest, NativeNavButtons) {
  std::vector<views::FrameButton> leading_buttons;
  std::vector<views::FrameButton> trailing_buttons;
  leading_buttons.push_back(views::FrameButton::kClose);
  leading_buttons.push_back(views::FrameButton::kMaximize);
  leading_buttons.push_back(views::FrameButton::kMinimize);
  layout_manager_->SetButtonOrdering(leading_buttons, trailing_buttons);
  ResetNativeNavButtonImagesFromButtonProvider();

  views::test::RunScheduledLayout(root_view_);

  const int frame_top_thickness = FrameInsets().top();

  int x = FrameInsets().left();

  // Close button.
  EXPECT_EQ(kCloseButtonSize, close_button_->size());
  x += kTopAreaSpacing.left() + kCloseButtonMargin.left();
  EXPECT_EQ(x, close_button_->x());
  EXPECT_EQ(kCloseButtonMargin.top() + frame_top_thickness, close_button_->y());

  // Maximize button.
  EXPECT_EQ(kMaximizeButtonSize, maximize_button_->size());
  x += kCloseButtonSize.width() + kCloseButtonMargin.right() +
       kInterNavButtonSpacing + kMaximizeButtonMargin.left();
  EXPECT_EQ(x, maximize_button_->x());
  EXPECT_EQ(kMaximizeButtonMargin.top() + frame_top_thickness,
            maximize_button_->y());

  // Minimize button.
  EXPECT_EQ(kMinimizeButtonSize, minimize_button_->size());
  x += kMaximizeButtonSize.width() + kMaximizeButtonMargin.right() +
       kInterNavButtonSpacing + kMinimizeButtonMargin.left();
  EXPECT_EQ(x, minimize_button_->x());
  EXPECT_EQ(kMinimizeButtonMargin.top() + frame_top_thickness,
            minimize_button_->y());
}

// The restored frame border insets are used to convert between the window
// geometry and the window bounds for the restored state, which may happen
// while the window is still maximized (e.g. when the compositor unmaximizes
// the window). They must not depend on the current window state.
TEST_F(BrowserFrameViewLayoutLinuxNativeTest,
       RestoredFrameBorderInsetsIndependentOfMaximizedState) {
  // The top thickness is smaller than the resize border, so the top inset
  // comes from the input insets.
  frame_provider_->set_frame_thickness(gfx::Insets::TLBR(4, 12, 15, 12));
  const gfx::Insets kExpectedInsets = gfx::Insets::TLBR(10, 12, 15, 12);

  EXPECT_EQ(kExpectedInsets,
            layout_manager_->RestoredMirroredFrameBorderInsets());

  delegate_->set_maximized(true);
  EXPECT_EQ(kExpectedInsets,
            layout_manager_->RestoredMirroredFrameBorderInsets());
}

TEST_F(BrowserFrameViewLayoutLinuxNativeTest,
       NonClientTopHeightUsesHeaderBarMetricsForNonTabbedWindow) {
  delegate_->set_tab_strip_visible(false);
  delegate_->set_show_window_title(true);
  frame_provider_->set_frame_thickness(gfx::Insets::TLBR(4, 12, 15, 12));
  frame_provider_->set_top_area_min_height(24);
  frame_provider_->set_top_area_padding(gfx::Insets::VH(6, 6));
  frame_provider_->set_top_area_border(gfx::Insets::TLBR(1, 0, 0, 0));
  nav_button_provider_->set_nav_button_height(34);

  maximized_frame_provider_->set_top_area_min_height(20);
  maximized_frame_provider_->set_top_area_padding(gfx::Insets::VH(4, 4));
  maximized_frame_provider_->set_top_area_border(gfx::Insets());
  nav_button_provider_->set_maximized_nav_button_height(30);

  const int title_font_height = views::TypographyProvider::Get()
                                    .GetWindowTitleFontList()
                                    .DeriveWithWeight(gfx::Font::Weight::BOLD)
                                    .GetHeight();
  // Top inset is expanded to the 10px resize border when restored.
  const int expected_restored =
      10 + 1 + 12 + std::max({title_font_height, 24, 34});
  EXPECT_EQ(expected_restored, layout_manager_->NonClientTopHeight(false));
  EXPECT_EQ(expected_restored, layout_manager_->NonClientTopHeight(true));

  // When min-height exceeds button height, min-height determines content
  // height.
  frame_provider_->set_top_area_min_height(42);
  const int expected_min_height =
      10 + 1 + 12 + std::max({title_font_height, 42, 34});
  EXPECT_EQ(expected_min_height, layout_manager_->NonClientTopHeight(false));

  // Maximized windows drop the frame border inset for NonClientTopHeight(false)
  // while preserving restored metrics for NonClientTopHeight(true).
  delegate_->set_maximized(true);
  const int expected_maximized =
      0 + 0 + 8 + std::max({title_font_height, 20, 30});
  EXPECT_EQ(expected_maximized, layout_manager_->NonClientTopHeight(false));
  EXPECT_EQ(expected_min_height, layout_manager_->NonClientTopHeight(true));

  // Tiled windows also drop the restored frame border inset.
  delegate_->set_maximized(false);
  delegate_->set_tiled(true);
  EXPECT_EQ(expected_min_height - 10,
            layout_manager_->NonClientTopHeight(false));
}

TEST_F(BrowserFrameViewLayoutLinuxNativeTest,
       NonClientTopHeightUnchangedForTabbedAndWebAppWindows) {
  frame_provider_->set_frame_thickness(gfx::Insets::TLBR(4, 12, 15, 12));
  nav_button_provider_->set_nav_button_height(34);

  // Tabbed windows have ShouldShowWindowTitle() == false and only reserve the
  // top frame border thickness above the tabstrip.
  EXPECT_EQ(10, layout_manager_->NonClientTopHeight(false));

  // Installed web apps (WebAppButtonHeight() > 0) keep the base layout height
  // and icon visibility.
  delegate_->set_tab_strip_visible(false);
  delegate_->set_show_window_title(true);
  delegate_->set_show_window_icon(true);
  delegate_->set_window_title(u"PWA");
  delegate_->set_web_app_button_height(32);
  AddWindowTitleAndIcon();
  nav_button_provider_->set_top_area_spacing(gfx::Insets(6));
  ResetNativeNavButtonImagesFromButtonProvider();

  views::test::RunScheduledLayout(root_view_);

  // Base OpaqueBrowserFrameViewLayout height:
  // FrameEdgeInsets.top() (10) + WebAppButtonHeight (32) + kVerticalPadding (2)
  // + kContentEdgeShadowThickness (2) = 46.
  EXPECT_EQ(46, layout_manager_->NonClientTopHeight(false));
  EXPECT_TRUE(window_icon_->GetVisible());
}

TEST_F(BrowserFrameViewLayoutLinuxNativeTest,
       NonTabbedTitleCenteredAndIconHidden) {
  delegate_->set_tab_strip_visible(false);
  delegate_->set_show_window_title(true);
  delegate_->set_show_window_icon(true);
  delegate_->set_window_title(u"AppTitle");
  AddWindowTitleAndIcon();

  frame_provider_->set_frame_thickness(gfx::Insets(10));
  frame_provider_->set_top_area_min_height(24);
  frame_provider_->set_top_area_padding(gfx::Insets::VH(6, 6));
  frame_provider_->set_top_area_border(gfx::Insets::TLBR(1, 0, 0, 0));
  nav_button_provider_->set_top_area_spacing(gfx::Insets(6));
  nav_button_provider_->set_nav_button_height(34);
  layout_manager_->SetButtonOrdering({}, {views::FrameButton::kClose});
  ResetNativeNavButtonImagesFromButtonProvider();

  views::test::RunScheduledLayout(root_view_);

  EXPECT_FALSE(window_icon_->GetVisible());
  EXPECT_TRUE(layout_manager_->IconBounds().IsEmpty());

  ASSERT_TRUE(window_title_->GetVisible());
  EXPECT_EQ(u"AppTitle", window_title_->GetText());
  EXPECT_EQ(gfx::Font::Weight::BOLD,
            window_title_->font_list().GetFontWeight());

  // Short title should be centered across the full window width.
  EXPECT_GT(window_title_->width(), 0);
  EXPECT_EQ((kWindowWidth - window_title_->width()) / 2, window_title_->x());

  // Vertically centered within the headerbar content area below top border.
  const int content_top = 10 + 1;
  const int content_height =
      layout_manager_->NonClientTopHeight(false) - content_top;
  EXPECT_EQ(content_top + (content_height - window_title_->height()) / 2,
            window_title_->y());
}

TEST_F(BrowserFrameViewLayoutLinuxNativeTest,
       NonTabbedLongTitleClampedBetweenCaptionButtons) {
  delegate_->set_tab_strip_visible(false);
  delegate_->set_show_window_title(true);
  delegate_->set_show_window_icon(true);
  delegate_->set_window_title(std::u16string(200, u'W'));
  AddWindowTitleAndIcon();

  frame_provider_->set_frame_thickness(gfx::Insets(10));
  nav_button_provider_->set_top_area_spacing(gfx::Insets(6));
  nav_button_provider_->set_nav_button_height(34);
  layout_manager_->SetButtonOrdering(
      {views::FrameButton::kClose},
      {views::FrameButton::kMinimize, views::FrameButton::kMaximize});
  ResetNativeNavButtonImagesFromButtonProvider();

  views::test::RunScheduledLayout(root_view_);

  const int leading_limit = close_button_->bounds().right() +
                            kCloseButtonMargin.right() +
                            OpaqueBrowserFrameViewLayout::kCaptionSpacing;
  const int trailing_limit = minimize_button_->x() -
                             kMinimizeButtonMargin.right() -
                             OpaqueBrowserFrameViewLayout::kCaptionSpacing;
  EXPECT_EQ(leading_limit, window_title_->x());
  EXPECT_EQ(trailing_limit, window_title_->bounds().right());

  // A title that fits in the available space between asymmetric buttons but
  // would collide with the wider trailing button group if centered across the
  // window should shift left without shrinking below its preferred width.
  constexpr int kWideWindowWidth = 1000;
  root_view_->SetSize(gfx::Size(kWideWindowWidth, kWindowWidth));
  layout_manager_->SetButtonOrdering(
      {}, {views::FrameButton::kMinimize, views::FrameButton::kMaximize,
           views::FrameButton::kClose});
  delegate_->set_window_title(std::u16string(22, u'W'));
  views::test::RunScheduledLayout(root_view_);

  const int asymmetric_leading_limit =
      FrameInsets().left() + OpaqueBrowserFrameViewLayout::kCaptionSpacing;
  const int asymmetric_trailing_limit =
      minimize_button_->x() - kMinimizeButtonMargin.right() -
      OpaqueBrowserFrameViewLayout::kCaptionSpacing;
  const int preferred_width = window_title_->GetPreferredSize({}).width();
  ASSERT_LT(preferred_width,
            asymmetric_trailing_limit - asymmetric_leading_limit);
  ASSERT_GT((kWideWindowWidth + preferred_width) / 2,
            asymmetric_trailing_limit);
  EXPECT_EQ(preferred_width, window_title_->width());
  EXPECT_EQ(asymmetric_trailing_limit, window_title_->bounds().right());
  EXPECT_GE(window_title_->x(), asymmetric_leading_limit);
}
