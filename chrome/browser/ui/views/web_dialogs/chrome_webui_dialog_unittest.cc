// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_dialogs/chrome_webui_dialog.h"

#include <memory>

#include "base/run_loop.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/constrained_window/constrained_window_views_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace webui_dialog {

namespace {

constexpr int kMinSize = 100;
constexpr int kMaxSize = 500;
constexpr int kResizeSize = 200;

class TestWebUIContentsWrapper : public WebUIContentsWrapper {
 public:
  explicit TestWebUIContentsWrapper(Profile* profile)
      : WebUIContentsWrapper(GURL(""), profile, 0, true, true, true, "Test") {}
  ~TestWebUIContentsWrapper() override = default;

  void ReloadWebContents() override {}
  base::WeakPtr<WebUIContentsWrapper> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestWebUIContentsWrapper> weak_ptr_factory_{this};
};

class ChromeWebUIDialogTest : public ChromeViewsTestBase {
 protected:
  ChromeWebUIDialogTest() = default;
  ~ChromeWebUIDialogTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());
    profile_ = std::make_unique<TestingProfile>();
  }

  void TearDown() override {
    constrained_window::SetConstrainedWindowViewsClient(nullptr);
    base::RunLoop().RunUntilIdle();
    profile_.reset();
    base::RunLoop().RunUntilIdle();
    ChromeViewsTestBase::TearDown();
  }

  TestingProfile* profile() { return profile_.get(); }

  // Helper to create a dialog widget with a test WebContents.
  std::unique_ptr<views::Widget> CreateDialogWidget(const WebDialogSpec& spec) {
    auto contents_wrapper =
        std::make_unique<TestWebUIContentsWrapper>(profile());
    auto test_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    contents_wrapper->SetWebContentsForTesting(std::move(test_contents));

    return ChromeWebUIDialog::Show(GetContext(), std::move(contents_wrapper),
                                   spec);
  }

 private:
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(ChromeWebUIDialogTest, ShowDialog) {
  WebDialogSpec spec;
  spec.min_size = gfx::Size(kMinSize, kMinSize);
  spec.max_size = gfx::Size(kMaxSize, kMaxSize);
  spec.wait_for_explicit_show = false;

  std::unique_ptr<views::Widget> widget = CreateDialogWidget(spec);
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
}

TEST_F(ChromeWebUIDialogTest, WaitForExplicitShow) {
  WebDialogSpec spec;
  spec.min_size = gfx::Size(kMinSize, kMinSize);
  spec.max_size = gfx::Size(kMaxSize, kMaxSize);
  spec.wait_for_explicit_show = true;

  std::unique_ptr<views::Widget> widget = CreateDialogWidget(spec);
  ASSERT_TRUE(widget);
  EXPECT_FALSE(widget->IsVisible());

  auto* delegate = static_cast<ChromeWebUIDialog*>(widget->widget_delegate());
  delegate->ShowUI();
  EXPECT_TRUE(widget->IsVisible());
}

TEST_F(ChromeWebUIDialogTest, ShowUIRedundantCallsAreSafe) {
  WebDialogSpec spec;
  spec.min_size = gfx::Size(kMinSize, kMinSize);
  spec.max_size = gfx::Size(kMaxSize, kMaxSize);
  spec.wait_for_explicit_show = true;

  std::unique_ptr<views::Widget> widget = CreateDialogWidget(spec);
  ASSERT_TRUE(widget);

  auto* delegate = static_cast<ChromeWebUIDialog*>(widget->widget_delegate());

  delegate->ShowUI();
  delegate->ShowUI();
  delegate->ShowUI();

  EXPECT_TRUE(widget->IsVisible());
}

TEST_F(ChromeWebUIDialogTest, CloseUIClosesWidget) {
  WebDialogSpec spec;
  spec.min_size = gfx::Size(kMinSize, kMinSize);
  spec.max_size = gfx::Size(kMaxSize, kMaxSize);

  std::unique_ptr<views::Widget> widget = CreateDialogWidget(spec);
  ASSERT_TRUE(widget);

  auto* delegate = static_cast<ChromeWebUIDialog*>(widget->widget_delegate());
  EXPECT_FALSE(widget->IsClosed());

  delegate->CloseUI();

  EXPECT_TRUE(widget->IsClosed());
}

TEST_F(ChromeWebUIDialogTest, AutoResizeWithinBounds) {
  WebDialogSpec spec;
  spec.min_size = gfx::Size(kMinSize, kMinSize);
  spec.max_size = gfx::Size(kMaxSize, kMaxSize);

  std::unique_ptr<views::Widget> widget = CreateDialogWidget(spec);
  auto* delegate = static_cast<ChromeWebUIDialog*>(widget->widget_delegate());

  // Simulate auto-resize signal from WebUI.
  delegate->ResizeDueToAutoResize(nullptr, gfx::Size(kResizeSize, kResizeSize));

  EXPECT_EQ(delegate->web_view()->GetPreferredSize(),
            gfx::Size(kResizeSize, kResizeSize));
}

TEST_F(ChromeWebUIDialogTest, AutoResizeClampsToMinSize) {
  WebDialogSpec spec;
  spec.min_size = gfx::Size(200, 200);
  spec.max_size = gfx::Size(kMaxSize, kMaxSize);

  std::unique_ptr<views::Widget> widget = CreateDialogWidget(spec);
  auto* delegate = static_cast<ChromeWebUIDialog*>(widget->widget_delegate());

  // Attempt to resize to 50x50, which is smaller than min_size (200x200).
  delegate->ResizeDueToAutoResize(nullptr, gfx::Size(50, 50));

  EXPECT_EQ(delegate->web_view()->GetPreferredSize(), gfx::Size(200, 200));
}

TEST_F(ChromeWebUIDialogTest, AutoResizeClampsToMaxSize) {
  WebDialogSpec spec;
  spec.min_size = gfx::Size(kMinSize, kMinSize);
  spec.max_size = gfx::Size(300, 300);

  std::unique_ptr<views::Widget> widget = CreateDialogWidget(spec);
  auto* delegate = static_cast<ChromeWebUIDialog*>(widget->widget_delegate());

  // Attempt to resize to 800x800, which is larger than max_size (300x300).
  delegate->ResizeDueToAutoResize(nullptr, gfx::Size(800, 800));

  EXPECT_EQ(delegate->web_view()->GetPreferredSize(), gfx::Size(300, 300));
}

TEST_F(ChromeWebUIDialogTest, CornerRadiusClipping) {
  WebDialogSpec spec;
  spec.min_size = gfx::Size(kMinSize, kMinSize);
  spec.max_size = gfx::Size(kMaxSize, kMaxSize);
  spec.corner_radius = 12;

  std::unique_ptr<views::Widget> widget = CreateDialogWidget(spec);
  ASSERT_TRUE(widget);

  auto* delegate = static_cast<ChromeWebUIDialog*>(widget->widget_delegate());
  auto* web_view = delegate->web_view();

  // The holder is created and manages the NativeView.
  EXPECT_TRUE(web_view->holder());
}

TEST_F(ChromeWebUIDialogTest, ModalityBrowserModal) {
  WebDialogSpec spec;
  spec.min_size = gfx::Size(kMinSize, kMinSize);
  spec.max_size = gfx::Size(kMaxSize, kMaxSize);
  spec.modal_type = ui::mojom::ModalType::kWindow;

  std::unique_ptr<views::Widget> widget = CreateDialogWidget(spec);
  ASSERT_TRUE(widget);

  EXPECT_EQ(widget->widget_delegate()->GetModalType(),
            ui::mojom::ModalType::kWindow);
}

TEST_F(ChromeWebUIDialogTest, ModalityModeless) {
  WebDialogSpec spec;
  spec.min_size = gfx::Size(kMinSize, kMinSize);
  spec.max_size = gfx::Size(kMaxSize, kMaxSize);
  spec.modal_type = ui::mojom::ModalType::kNone;

  std::unique_ptr<views::Widget> widget = CreateDialogWidget(spec);
  ASSERT_TRUE(widget);

  EXPECT_EQ(widget->widget_delegate()->GetModalType(),
            ui::mojom::ModalType::kNone);
}

TEST_F(ChromeWebUIDialogTest, DialogButtons) {
  WebDialogSpec spec;
  spec.min_size = gfx::Size(kMinSize, kMinSize);
  spec.max_size = gfx::Size(kMaxSize, kMaxSize);
  // Specify OK and Cancel buttons.
  spec.buttons = static_cast<int>(ui::mojom::DialogButton::kOk) |
                 static_cast<int>(ui::mojom::DialogButton::kCancel);

  std::unique_ptr<views::Widget> widget = CreateDialogWidget(spec);
  ASSERT_TRUE(widget);

  EXPECT_EQ(widget->widget_delegate()->AsDialogDelegate()->buttons(),
            spec.buttons);
}

TEST_F(ChromeWebUIDialogTest, ShowCloseButton) {
  WebDialogSpec spec;
  spec.min_size = gfx::Size(kMinSize, kMinSize);
  spec.max_size = gfx::Size(kMaxSize, kMaxSize);
  spec.show_close_button = true;

  std::unique_ptr<views::Widget> widget = CreateDialogWidget(spec);
  ASSERT_TRUE(widget);

  EXPECT_TRUE(widget->widget_delegate()->ShouldShowCloseButton());
}

TEST_F(ChromeWebUIDialogTest, ElementIdentifierSet) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId);

  WebDialogSpec spec;
  spec.min_size = gfx::Size(kMinSize, kMinSize);
  spec.max_size = gfx::Size(kMaxSize, kMaxSize);
  spec.element_identifier = kTestElementId;

  std::unique_ptr<views::Widget> widget = CreateDialogWidget(spec);
  ASSERT_TRUE(widget);

  auto* delegate = static_cast<ChromeWebUIDialog*>(widget->widget_delegate());
  views::WebView* web_view = delegate->web_view();

  EXPECT_EQ(web_view->GetProperty(views::kElementIdentifierKey),
            spec.element_identifier);
}

TEST_F(ChromeWebUIDialogTest, InitiallyFocusedViewIsWebView) {
  WebDialogSpec spec;
  spec.min_size = gfx::Size(kMinSize, kMinSize);
  spec.max_size = gfx::Size(kMaxSize, kMaxSize);

  std::unique_ptr<views::Widget> widget = CreateDialogWidget(spec);
  ASSERT_TRUE(widget);

  auto* delegate = static_cast<ChromeWebUIDialog*>(widget->widget_delegate());

  EXPECT_EQ(widget->widget_delegate()->GetInitiallyFocusedView(),
            delegate->web_view());
}

}  // namespace
}  // namespace webui_dialog
