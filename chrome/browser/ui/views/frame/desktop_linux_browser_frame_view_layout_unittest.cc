// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/desktop_linux_browser_frame_view_layout.h"

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/nav_button_provider.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"

namespace {

constexpr int kWindowWidth = 500;

constexpr gfx::Size kCloseButtonSize = gfx::Size(2, 3);
constexpr gfx::Size kMaximizeButtonSize = gfx::Size(5, 7);
constexpr gfx::Size kMinimizeButtonSize = gfx::Size(11, 13);

constexpr gfx::Insets kCloseButtonMargin = gfx::Insets(17, 19, 23, 29);
constexpr gfx::Insets kMaximizeButtonMargin = gfx::Insets(31, 37, 41, 43);
constexpr gfx::Insets kMinimizeButtonMargin = gfx::Insets(47, 53, 59, 61);

constexpr gfx::Insets kTopAreaSpacing = gfx::Insets(67, 71, 73, 79);

constexpr int kInterNavButtonSpacing = 83;

static gfx::ImageSkia GetTestImageForSize(gfx::Size size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size.width(), size.height());
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

class TestLayoutDelegate : public OpaqueBrowserFrameViewLayoutDelegate {
 public:
  TestLayoutDelegate() {}
  ~TestLayoutDelegate() override {}

  // OpaqueBrowserFrameViewLayoutDelegate:
  bool ShouldShowWindowIcon() const override { return false; }
  bool ShouldShowWindowTitle() const override { return false; }
  base::string16 GetWindowTitle() const override { return base::string16(); }
  int GetIconSize() const override { return 17; }
  gfx::Size GetBrowserViewMinimumSize() const override {
    return gfx::Size(168, 64);
  }
  bool ShouldShowCaptionButtons() const override { return true; }
  bool IsRegularOrGuestSession() const override { return true; }
  bool IsMaximized() const override { return false; }
  bool IsMinimized() const override { return false; }
  bool IsTabStripVisible() const override { return true; }
  int GetTabStripHeight() const override {
    return GetLayoutConstant(TAB_HEIGHT);
  }
  bool IsToolbarVisible() const override { return true; }
  gfx::Size GetTabstripPreferredSize() const override {
    return gfx::Size(78, 29);
  }
  int GetTopAreaHeight() const override { return 0; }
  bool UseCustomFrame() const override { return true; }
  bool IsFrameCondensed() const override { return false; }
  bool EverHasVisibleBackgroundTabShapes() const override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLayoutDelegate);
};

class TestNavButtonProvider : public views::NavButtonProvider {
 public:
  TestNavButtonProvider() {}

  ~TestNavButtonProvider() override {}

  void RedrawImages(int top_area_height, bool maximized, bool active) override {
    ASSERT_EQ(false, maximized);  // This only tests the restored state.
  }

  gfx::ImageSkia GetImage(chrome::FrameButtonDisplayType type,
                          views::Button::ButtonState state) const override {
    switch (type) {
      case chrome::FrameButtonDisplayType::kClose:
        return GetTestImageForSize(kCloseButtonSize);
      case chrome::FrameButtonDisplayType::kMaximize:
        return GetTestImageForSize(kMaximizeButtonSize);
      case chrome::FrameButtonDisplayType::kMinimize:
        return GetTestImageForSize(kMinimizeButtonSize);
      default:
        NOTREACHED();
        return gfx::ImageSkia();
    }
  }

  gfx::Insets GetNavButtonMargin(
      chrome::FrameButtonDisplayType type) const override {
    switch (type) {
      case chrome::FrameButtonDisplayType::kClose:
        return kCloseButtonMargin;
      case chrome::FrameButtonDisplayType::kMaximize:
        return kMaximizeButtonMargin;
      case chrome::FrameButtonDisplayType::kMinimize:
        return kMinimizeButtonMargin;
      default:
        NOTREACHED();
        return gfx::Insets();
    }
  }

  gfx::Insets GetTopAreaSpacing() const override { return kTopAreaSpacing; }

  int GetInterNavButtonSpacing() const override {
    return kInterNavButtonSpacing;
  }
};

}  // namespace

class DesktopLinuxBrowserFrameViewLayoutTest : public ChromeViewsTestBase {
 public:
  DesktopLinuxBrowserFrameViewLayoutTest() {}
  ~DesktopLinuxBrowserFrameViewLayoutTest() override {}

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    delegate_ = std::make_unique<TestLayoutDelegate>();
    nav_button_provider_ = std::make_unique<::TestNavButtonProvider>();
    auto layout = std::make_unique<DesktopLinuxBrowserFrameViewLayout>(
        nav_button_provider_.get());
    layout->set_delegate(delegate_.get());
    layout->set_forced_window_caption_spacing_for_test(0);
    widget_ = new views::Widget;
    widget_->Init(CreateParams(views::Widget::InitParams::TYPE_POPUP));
    root_view_ = widget_->GetRootView();
    root_view_->SetSize(gfx::Size(kWindowWidth, kWindowWidth));
    layout_manager_ = root_view_->SetLayoutManager(std::move(layout));

    minimize_button_ = InitWindowCaptionButton(VIEW_ID_MINIMIZE_BUTTON);
    maximize_button_ = InitWindowCaptionButton(VIEW_ID_MAXIMIZE_BUTTON);
    restore_button_ = InitWindowCaptionButton(VIEW_ID_RESTORE_BUTTON);
    close_button_ = InitWindowCaptionButton(VIEW_ID_CLOSE_BUTTON);
  }

  void TearDown() override {
    widget_->CloseNow();

    ChromeViewsTestBase::TearDown();
  }

 protected:
  views::ImageButton* InitWindowCaptionButton(ViewID view_id) {
    views::ImageButton* button = new views::ImageButton(nullptr);
    button->SetID(view_id);
    root_view_->AddChildView(button);
    return button;
  }

  void ResetNativeNavButtonImagesFromButtonProvider() {
    std::vector<views::ImageButton*> buttons{close_button_, maximize_button_,
                                             minimize_button_};
    std::vector<chrome::FrameButtonDisplayType> button_types{
        chrome::FrameButtonDisplayType::kClose,
        chrome::FrameButtonDisplayType::kMaximize,
        chrome::FrameButtonDisplayType::kMinimize};
    for (size_t i = 0; i < buttons.size(); i++) {
      for (views::Button::ButtonState state :
           {views::Button::STATE_NORMAL, views ::Button::STATE_HOVERED,
            views::Button::STATE_PRESSED}) {
        buttons[i]->SetImage(
            state, nav_button_provider_->GetImage(button_types[i], state));
      }
    }
  }

  int FrameTopThickness() const {
    return static_cast<OpaqueBrowserFrameViewLayout*>(layout_manager_)
        ->FrameTopThickness(false);
  }

  int FrameSideThickness() const {
    return static_cast<OpaqueBrowserFrameViewLayout*>(layout_manager_)
        ->FrameSideThickness(false);
  }

  views::Widget* widget_ = nullptr;
  views::View* root_view_ = nullptr;
  DesktopLinuxBrowserFrameViewLayout* layout_manager_ = nullptr;
  std::unique_ptr<TestLayoutDelegate> delegate_;
  std::unique_ptr<views::NavButtonProvider> nav_button_provider_;

  // Widgets:
  views::ImageButton* minimize_button_ = nullptr;
  views::ImageButton* maximize_button_ = nullptr;
  views::ImageButton* restore_button_ = nullptr;
  views::ImageButton* close_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DesktopLinuxBrowserFrameViewLayoutTest);
};

// Tests layout of native navigation buttons.
TEST_F(DesktopLinuxBrowserFrameViewLayoutTest, NativeNavButtons) {
  std::vector<views::FrameButton> leading_buttons;
  std::vector<views::FrameButton> trailing_buttons;
  leading_buttons.push_back(views::FrameButton::kClose);
  leading_buttons.push_back(views::FrameButton::kMaximize);
  leading_buttons.push_back(views::FrameButton::kMinimize);
  layout_manager_->SetButtonOrdering(leading_buttons, trailing_buttons);
  ResetNativeNavButtonImagesFromButtonProvider();

  root_view_->Layout();

  const int frame_top_thickness = FrameTopThickness();

  int x = FrameSideThickness();

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
