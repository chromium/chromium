// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux_native.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/base/models/image_model.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/linux/window_frame_provider.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
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
  bool ShouldShowWindowIcon() const override { return false; }
  bool ShouldShowWindowTitle() const override { return false; }
  std::u16string GetWindowTitle() const override { return std::u16string(); }
  int GetIconSize() const override { return 17; }
  gfx::Size GetBrowserViewMinimumSize() const override {
    return gfx::Size(168, 64);
  }
  bool ShouldShowCaptionButtons() const override { return true; }
  bool IsRegularOrGuestSession() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }
  bool IsMaximized() const override { return false; }
  bool IsMinimized() const override { return false; }
  bool IsFullscreen() const override { return false; }
  bool IsTabStripVisible() const override { return true; }
  bool GetBorderlessModeEnabled() const override { return false; }
  int GetTabStripHeight() const override {
    return GetLayoutConstant(TAB_HEIGHT);
  }
  bool IsToolbarVisible() const override { return true; }
  gfx::Size GetTabstripMinimumSize() const override {
    return gfx::Size(78, 29);
  }
  int GetTopAreaHeight() const override { return 0; }
  bool UseCustomFrame() const override { return true; }
  bool IsFrameCondensed() const override { return false; }
  bool EverHasVisibleBackgroundTabShapes() const override { return false; }
  void UpdateWindowControlsOverlay(const gfx::Rect& bounding_rect) override {}
  bool ShouldDrawRestoredFrameShadow() const override { return true; }
#if BUILDFLAG(IS_LINUX)
  bool IsTiled() const override { return false; }
#endif
  int WebAppButtonHeight() const override { return 0; }
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
        return kMaximizeButtonMargin;
      case ui::NavButtonProvider::FrameButtonDisplayType::kMinimize:
        return kMinimizeButtonMargin;
      default:
        NOTREACHED();
    }
  }

  gfx::Insets GetTopAreaSpacing() const override { return kTopAreaSpacing; }

  int GetInterNavButtonSpacing() const override {
    return kInterNavButtonSpacing;
  }
};

class TestFrameProvider : public ui::WindowFrameProvider {
 public:
  TestFrameProvider() = default;

  ~TestFrameProvider() override = default;

  // ui::WindowFrameProvider:
  int GetTopCornerRadiusDip() override { return 0; }
  bool IsTopFrameTranslucent() override { return false; }
  gfx::Insets GetFrameThicknessDip() override { return {}; }
  void PaintWindowFrame(gfx::Canvas* canvas,
                        const gfx::Rect& rect,
                        int top_area_height,
                        bool focused,
                        const gfx::Insets& input_insets) override {}
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
    auto layout = std::make_unique<BrowserFrameViewLayoutLinuxNative>(
        nav_button_provider_.get(),
        base::BindRepeating([](ui::WindowFrameProvider* frame_provider,
                               bool tiled) { return frame_provider; },
                            frame_provider_.get()));
    layout->set_delegate(delegate_.get());
    layout->set_forced_window_caption_spacing_for_test(0);
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    root_view_ = widget_->GetRootView();
    root_view_->SetSize(gfx::Size(kWindowWidth, kWindowWidth));
    layout_manager_ = root_view_->SetLayoutManager(std::move(layout));

    minimize_button_ = InitWindowCaptionButton(VIEW_ID_MINIMIZE_BUTTON);
    maximize_button_ = InitWindowCaptionButton(VIEW_ID_MAXIMIZE_BUTTON);
    restore_button_ = InitWindowCaptionButton(VIEW_ID_RESTORE_BUTTON);
    close_button_ = InitWindowCaptionButton(VIEW_ID_CLOSE_BUTTON);
  }

  void TearDown() override {
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
  std::unique_ptr<ui::NavButtonProvider> nav_button_provider_;
  std::unique_ptr<ui::WindowFrameProvider> frame_provider_;

  // Widgets:
  raw_ptr<views::ImageButton, DanglingUntriaged> minimize_button_ = nullptr;
  raw_ptr<views::ImageButton, DanglingUntriaged> maximize_button_ = nullptr;
  raw_ptr<views::ImageButton, DanglingUntriaged> restore_button_ = nullptr;
  raw_ptr<views::ImageButton, DanglingUntriaged> close_button_ = nullptr;
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
