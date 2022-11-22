// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/screenshot/screenshot_captured_bubble.h"

#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/image_editor/image_editor_component_info.h"
#include "chrome/browser/share/share_features.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/widget_test.h"

namespace sharing_hub {

namespace {

gfx::Image CreateTestImage() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 14);
  bitmap.eraseColor(SK_ColorYELLOW);
  gfx::ImageSkia img(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
  return gfx::Image(img);
}

bool IsButtonWithLabel(const std::u16string& label, const views::View* v) {
  auto* button = views::Button::AsButton(v);
  if (!button || strcmp(button->GetClassName(), "MdTextButton"))
    return false;
  return static_cast<const views::LabelButton*>(button)->GetText() == label;
}

void ClickButton(views::Button* button) {
  const gfx::Point point(10, 10);
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, point, point,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  button->OnMousePressed(event);
  button->OnMouseReleased(event);
}

}  // namespace

class ScreenshotCapturedBubbleTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    // This simulates the editor being installed for tests, so that the Edit
    // button will show up.
    image_editor::ImageEditorComponentInfo::GetInstance()->SetInstalledPath(
        base::FilePath(base::FilePath::kCurrentDirectory));
    anchor_widget_ = CreateTestWidget();
  }

  void TearDown() override {
    if (bubble_widget_)
      bubble_widget_->CloseNow();
    anchor_widget_->CloseNow();
    image_editor::ImageEditorComponentInfo::GetInstance()->SetInstalledPath({});
    ChromeViewsTestBase::TearDown();
  }

  void ShowBubbleWithEditCallback(
      base::OnceCallback<void(NavigateParams*)> edit) {
    auto bubble = std::make_unique<ScreenshotCapturedBubble>(
        anchor_widget_->GetRootView(), test_web_contents_.get(),
        CreateTestImage(), &profile_, std::move(edit));
    bubble_ = bubble.get();
    bubble_widget_ =
        views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
    bubble_->ShowForReason(LocationBarBubbleDelegateView::USER_GESTURE);
  }

 protected:
  ScreenshotCapturedBubble* bubble() { return bubble_; }
  views::Widget* bubble_widget() { return bubble_widget_; }

 private:
  base::test::ScopedFeatureList features_{
      share::kSharingDesktopScreenshotsEdit};
  TestingProfile profile_;
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> test_web_contents_ =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);

  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<ScreenshotCapturedBubble> bubble_;
  raw_ptr<views::Widget> bubble_widget_;
};

TEST_F(ScreenshotCapturedBubbleTest, EditNavigatesToImageEditorWebUI) {
  base::RunLoop run_loop;

  GURL navigated_url;
  WindowOpenDisposition navigated_disposition;
  NavigateParams::WindowAction navigated_action;

  ShowBubbleWithEditCallback(
      base::BindLambdaForTesting([&](NavigateParams* params) {
        navigated_url = params->url;
        navigated_disposition = params->disposition;
        navigated_action = params->window_action;
        run_loop.Quit();
      }));

  auto* button =
      static_cast<views::Button*>(views::test::AnyViewMatchingPredicate(
          bubble_widget(), base::BindRepeating(&IsButtonWithLabel, u"Edit")));
  ASSERT_TRUE(button);
  ClickButton(button);

  run_loop.Run();

  EXPECT_EQ(chrome::kChromeUIImageEditorURL, navigated_url.spec());
  EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB, navigated_disposition);
  EXPECT_EQ(NavigateParams::SHOW_WINDOW, navigated_action);
}

}  // namespace sharing_hub
