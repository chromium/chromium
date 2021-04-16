// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/widget_utils.h"

namespace {

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
                           font_list) {
    SetUpForInOutAnimation();
  }

  views::BubbleDialogDelegate* GetBubble() const override { return nullptr; }
  std::u16string GetTextForTooltipAndAccessibleName() const override {
    return u"TestTooltip";
  }

  bool IsLabelVisible() const { return label()->GetVisible(); }

 protected:
  // PageActionIconView:
  void OnExecuting(ExecuteSource execute_source) override {}
  const gfx::VectorIcon& GetVectorIcon() const override {
    return gfx::kNoneIcon;
  }
  void UpdateImpl() override {}
};

}  // namespace

class PageActionIconViewTest : public ChromeViewsTestBase {
 protected:
  // ChromeViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    widget_ = CreateTestWidget();
    delegate_ = TestPageActionIconDelegate();
    view_ = widget_->SetContentsView(std::make_unique<TestPageActionIconView>(
        /*command_updater=*/nullptr,
        /*command_id=*/0, delegate(), delegate(), gfx::FontList()));

    widget_->Show();
  }
  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestPageActionIconView* view() { return view_; }
  TestPageActionIconDelegate* delegate() { return &delegate_; }

 private:
  TestPageActionIconDelegate delegate_;
  TestPageActionIconView* view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(PageActionIconViewTest, ShouldResetSlideAnimationWhenHideIcons) {
  view()->AnimateIn(base::nullopt);
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
  view()->AnimateIn(base::nullopt);
  EXPECT_TRUE(view()->IsLabelVisible());
  EXPECT_TRUE(view()->is_animating_label());

  delegate()->set_should_hide_page_action_icons(false);
  view()->Update();
  EXPECT_TRUE(view()->is_animating_label());
  EXPECT_TRUE(view()->ShouldShowLabel());
  EXPECT_TRUE(view()->IsLabelVisible());
}
