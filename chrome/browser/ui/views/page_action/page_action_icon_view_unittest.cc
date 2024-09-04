// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

#include "base/memory/raw_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget_utils.h"

namespace {

constexpr int kPageActionIconSize = 20;

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
    return nullptr;
  }
  int GetPageActionIconSize() const override { return kPageActionIconSize; }
  bool ShouldHidePageActionIcons() const override {
    return should_hide_page_action_icons_;
  }

  void set_should_hide_page_action_icons(bool should_hide_page_action_icons) {
    should_hide_page_action_icons_ = should_hide_page_action_icons;
  }

 private:
  bool should_hide_page_action_icons_ = false;
};

}  // namespace

namespace {

class TestPageActionIconView : public PageActionIconView {
 public:
  using PageActionIconView::AnimateIn;

  explicit TestPageActionIconView(
      CommandUpdater* command_updater,
      int command_id,
      IconLabelBubbleView::Delegate* parent_delegate,
      PageActionIconView::Delegate* delegate,
      const gfx::FontList& font_list)
      : PageActionIconView(command_updater,
                           command_id,
                           parent_delegate,
                           delegate,
                           "TestName",
                           0,
                           nullptr,
                           true,
                           font_list) {
    SetUpForInOutAnimation();
    GetViewAccessibility().SetName(u"TestTooltip");
  }

  views::BubbleDialogDelegate* GetBubble() const override { return nullptr; }

  bool IsLabelVisible() const { return label()->GetVisible(); }

 protected:
  // PageActionIconView:
  void OnExecuting(ExecuteSource execute_source) override {}
  const gfx::VectorIcon& GetVectorIcon() const override {
    return gfx::kNoneIcon;
  }
  void UpdateImpl() override {}
};

class TestPageActionIconViewWithIconImage : public TestPageActionIconView {
 public:
  using TestPageActionIconView::TestPageActionIconView;

  void SetIconImageColorForTesting(SkColor icon_image_color) {
    icon_image_color_ = icon_image_color;
  }

  void UpdateIconImageForTesting() { UpdateIconImage(); }

  // TestPageActionIconView:
  ui::ImageModel GetSizedIconImage(int size) const override {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(size, size);
    bitmap.eraseColor(icon_image_color_);
    return ui::ImageModel::FromImageSkia(
        gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
  }

 private:
  SkColor icon_image_color_ = gfx::kPlaceholderColor;
};

}  // namespace

class PageActionIconViewTest : public ChromeViewsTestBase {
 protected:
  // ChromeViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    delegate_ = TestPageActionIconDelegate();
    view_ = widget_->SetContentsView(std::make_unique<TestPageActionIconView>(
        /*command_updater=*/nullptr,
        /*command_id=*/0, delegate(), delegate(), gfx::FontList()));

    widget_->Show();
  }
  void TearDown() override {
    ClearView();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void ClearView() { view_ = nullptr; }

  TestPageActionIconView* view() { return view_; }
  views::Widget* widget() { return widget_.get(); }
  TestPageActionIconDelegate* delegate() { return &delegate_; }

 private:
  TestPageActionIconDelegate delegate_;
  raw_ptr<TestPageActionIconView> view_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(PageActionIconViewTest, ShouldResetSlideAnimationWhenHideIcons) {
  view()->AnimateIn(std::nullopt);
  EXPECT_TRUE(view()->IsLabelVisible());
  EXPECT_TRUE(view()->is_animating_label());

  delegate()->set_should_hide_page_action_icons(true);
  view()->Update();
  EXPECT_FALSE(view()->is_animating_label());
  EXPECT_FALSE(view()->ShouldShowLabel());
  EXPECT_FALSE(view()->IsLabelVisible());
}

TEST_F(PageActionIconViewTest, ShouldNotResetSlideAnimationWhenShowIcons) {
  delegate()->set_should_hide_page_action_icons(true);
  view()->AnimateIn(std::nullopt);
  EXPECT_TRUE(view()->IsLabelVisible());
  EXPECT_TRUE(view()->is_animating_label());

  delegate()->set_should_hide_page_action_icons(false);
  view()->Update();
  EXPECT_TRUE(view()->is_animating_label());
  EXPECT_TRUE(view()->ShouldShowLabel());
  EXPECT_TRUE(view()->IsLabelVisible());
}

TEST_F(PageActionIconViewTest, UsesIconImageIfAvailable) {
  auto delegate = TestPageActionIconDelegate();

  // We're about to reset the 'ContentsView' of the Widget. As such
  // we need to clear the reference to |view_| beforehand, otherwise
  // it will become dangling.
  ClearView();
  auto* icon_view = widget()->SetContentsView(
      std::make_unique<TestPageActionIconViewWithIconImage>(
          /*command_updater=*/nullptr,
          /*command_id=*/0, &delegate, &delegate, gfx::FontList()));

  // Get the initial icon view image.
  auto image_previous = icon_view->GetImage(views::Button::STATE_NORMAL);

  // Update the page action icon view to host a green image. This should
  // override the initially set image.
  icon_view->SetIconImageColorForTesting(SK_ColorGREEN);
  icon_view->UpdateIconImageForTesting();
  EXPECT_FALSE(image_previous.BackedBySameObjectAs(
      icon_view->GetImage(views::Button::STATE_NORMAL)));

  // Update the page action icon view to host a red image. This should override
  // the green image.
  image_previous = icon_view->GetImage(views::Button::STATE_NORMAL);
  icon_view->SetIconImageColorForTesting(SK_ColorRED);
  icon_view->UpdateIconImageForTesting();
  EXPECT_FALSE(image_previous.BackedBySameObjectAs(
      icon_view->GetImage(views::Button::STATE_NORMAL)));
}

TEST_F(PageActionIconViewTest, IconViewAccessibleName) {
  EXPECT_EQ(view()->GetViewAccessibility().GetCachedName(),
            view()->GetTextForTooltipAndAccessibleName());
}
