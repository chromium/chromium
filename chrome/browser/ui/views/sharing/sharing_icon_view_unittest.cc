// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/sharing_icon_view.h"

#include "base/memory/raw_ptr.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/sharing_ui_controller.h"
#include "chrome/browser/sharing/sms/sms_remote_fetcher_ui_controller.h"
#include "chrome/browser/ui/views/sharing/sharing_dialog_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget_utils.h"

constexpr int kPageActionIconSize = 20;

class TestPageActionIconDelegate : public IconLabelBubbleView::Delegate,
                                   public PageActionIconView::Delegate {
 public:
  explicit TestPageActionIconDelegate(
      raw_ptr<content::WebContents> web_contents)
      : web_contents_(web_contents) {}
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
  int GetPageActionIconSize() const override { return kPageActionIconSize; }
  bool ShouldHidePageActionIcons() const override {
    return should_hide_page_action_icons_;
  }

  void set_should_hide_page_action_icons(bool should_hide_page_action_icons) {
    should_hide_page_action_icons_ = should_hide_page_action_icons;
  }

  void ClearWebContents() { web_contents_ = nullptr; }

 private:
  bool should_hide_page_action_icons_ = false;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

class TestSharingIconView : public SharingIconView {
 public:
  using SharingIconView::AnimateIn;

  explicit TestSharingIconView(IconLabelBubbleView::Delegate* parent_delegate,
                               PageActionIconView::Delegate* delegate,
                               GetControllerCallback get_controller_callback,
                               GetBubbleCallback get_bubble_callback)
      : SharingIconView(parent_delegate,
                        delegate,
                        get_controller_callback,
                        get_bubble_callback) {
    SetUpForInOutAnimation();
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

class SharingIconViewTest : public ChromeViewsTestBase {
 protected:
  // ChromeViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    test_web_contents_->Resize(
        gfx::Rect(/*x=*/0, /*y=*/0, /*width=*/1000, /*height=*/1000));
    delegate_ = TestPageActionIconDelegate(test_web_contents_.get());
    view_ = widget_->SetContentsView(std::make_unique<TestSharingIconView>(
        delegate(), delegate(),
        base::BindRepeating([](content::WebContents* contents) {
          return static_cast<SharingUiController*>(
              ClickToCallUiController::GetOrCreateFromWebContents(contents));
        }),
        base::BindRepeating(SharingDialogView::GetAsBubbleForClickToCall)));

    widget_->Show();
  }

  void TearDown() override {
    ClearView();
    widget_.reset();
    delegate_.ClearWebContents();
    ChromeViewsTestBase::TearDown();
  }

  void ClearView() { view_ = nullptr; }

  void InitialiseViewWithInaccessibleUi() {
    ClearView();
    view_ = widget_->SetContentsView(std::make_unique<TestSharingIconView>(
        delegate(), delegate(),
        base::BindRepeating([](content::WebContents* contents) {
          return static_cast<SharingUiController*>(
              SmsRemoteFetcherUiController::GetOrCreateFromWebContents(
                  contents));
        }),
        base::BindRepeating(SharingDialogView::GetAsBubbleForClickToCall)));
  }

  TestSharingIconView* view() { return view_; }
  views::Widget* widget() { return widget_.get(); }
  TestPageActionIconDelegate* delegate() { return &delegate_; }

 private:
  TestPageActionIconDelegate delegate_;
  raw_ptr<TestSharingIconView> view_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;
};

TEST_F(SharingIconViewTest, IgnoredAccessibleState) {
  ui::AXNodeData node_data;
  view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kIgnored));

  InitialiseViewWithInaccessibleUi();
  node_data = ui::AXNodeData();
  view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kIgnored));
}
