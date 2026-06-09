// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_contents_view.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_widget_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/picture_in_picture_window_options/picture_in_picture_window_options.mojom.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace {

blink::mojom::PictureInPictureWindowOptions MakeDefaultPipOptions() {
  blink::mojom::PictureInPictureWindowOptions opts;
  opts.width = 400;
  opts.height = 300;
  opts.disallow_return_to_opener = false;
  opts.prefer_initial_window_placement = false;
  return opts;
}

}  // namespace

class DocumentPipHostTest : public ChromeViewsTestBase {
 public:
  DocumentPipHostTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // In production, ChromeViewsDelegate honours
    // WidgetDelegate::use_desktop_widget_override() and selects
    // DesktopNativeWidgetAura for the PiP widget. The test ViewsDelegate
    // doesn't check that flag, so enable its equivalent here.
    test_views_delegate()->set_use_desktop_native_widgets(true);

    opener_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    ASSERT_TRUE(opener_web_contents_);

    // In production the opener always has a SecurityStateTabHelper attached (by
    // TabHelpers). DocumentPipFrameView reads it to render the origin chip's
    // security icon and CHECK()s the invariant, so attach it here to satisfy
    // that invariant and exercise the populated security-state path.
    ChromeSecurityStateTabHelper::CreateForWebContents(
        opener_web_contents_.get());

    // Host the opener WebContents inside a test top-level widget so that
    // `opener->GetTopLevelNativeWindow()` returns a real native window —
    // DesktopNativeWidgetAura uses it as `params.parent` when the PiP widget
    // is initialized.
    opener_host_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    auto* web_view = opener_host_widget_->SetContentsView(
        std::make_unique<views::WebView>(&profile_));
    web_view->SetWebContents(opener_web_contents_.get());
    opener_host_widget_->Show();
  }

  void TearDown() override {
    // Resetting the opener destroys the DocumentPipHost (and its widget)
    // automatically, before views tear-down. Must happen before the host
    // widget that owns the WebView referencing it.
    opener_web_contents_.reset();
    opener_host_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  content::WebContents* opener() { return opener_web_contents_.get(); }

  // Creates a DocumentPipHost attached to the opener with a PiP widget open.
  DocumentPipHost* CreateHostAndOpenPipWindow() {
    DocumentPipHost::CreateForWebContents(opener());
    auto* host = DocumentPipHost::FromWebContents(opener());
    auto child =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    host->CreatePipWidget(std::move(child), MakeDefaultPipOptions());
    return host;
  }

 protected:
  // Must be declared before |profile_| because TestingProfile may post tasks.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  std::unique_ptr<views::Widget> opener_host_widget_;
  std::unique_ptr<content::WebContents> opener_web_contents_;
};

// Verify that CreateForWebContents creates and attaches a host.
TEST_F(DocumentPipHostTest, CreateForWebContents_CreatesHost) {
  EXPECT_FALSE(DocumentPipHost::FromWebContents(opener()));
  CreateHostAndOpenPipWindow();
  EXPECT_TRUE(DocumentPipHost::FromWebContents(opener()));
}

// Calling CreateForWebContents a second time is a no-op.
TEST_F(DocumentPipHostTest, CreateForWebContents_IdempotentWhenExists) {
  DocumentPipHost* first = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(first);

  DocumentPipHost::CreateForWebContents(opener());

  EXPECT_EQ(first, DocumentPipHost::FromWebContents(opener()));
}

// The host should expose the opener, child WebContents, profile, and options.
TEST_F(DocumentPipHostTest, Accessors) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_EQ(opener(), host->GetOpenerWebContents());
  EXPECT_TRUE(host->GetChildWebContents());
  EXPECT_NE(opener(), host->GetChildWebContents());
  EXPECT_TRUE(host->GetProfile());
  EXPECT_EQ(400u, host->GetPipOptions().width);
  EXPECT_EQ(300u, host->GetPipOptions().height);
}

// The host creates a Widget with a DocumentPipWidgetDelegate.
TEST_F(DocumentPipHostTest, WidgetIsCreated) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  views::Widget* w = host->GetWidget();
  ASSERT_TRUE(w);
  EXPECT_FALSE(w->IsClosed());
}

// Regression test for crbug.com/519833771: opening the PiP window builds the
// DocumentPipFrameView, which reads the opener's SecurityStateTabHelper to
// render the origin chip security icon. With the helper attached (the
// production invariant), this populated path must complete without crashing.
TEST_F(DocumentPipHostTest, OpenPipWindow_PopulatesSecurityStateWithoutCrash) {
  ASSERT_TRUE(SecurityStateTabHelper::FromWebContents(opener()));

  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  views::Widget* widget = host->GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->non_client_view()->frame_view());
}

// After CreatePipWidget(), the child WebContents is hosted in the
// DocumentPipContentsView (a views::WebView).
TEST_F(DocumentPipHostTest, ChildWebContentsInWebView) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  auto* delegate = static_cast<DocumentPipWidgetDelegate*>(
      host->GetWidget()->widget_delegate());
  ASSERT_TRUE(delegate);

  auto* contents_view = delegate->GetDocumentPipContentsView();
  ASSERT_TRUE(contents_view);
  EXPECT_EQ(host->GetChildWebContents(), contents_view->web_contents());
}

// The widget delegate has the correct properties.
TEST_F(DocumentPipHostTest, WidgetDelegateProperties) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  auto* delegate = host->GetWidget()->widget_delegate();
  ASSERT_TRUE(delegate);

  EXPECT_TRUE(delegate->CanResize());
  EXPECT_FALSE(delegate->CanMaximize());
  EXPECT_FALSE(delegate->CanMinimize());
  EXPECT_FALSE(delegate->CanFullscreen());
}

// GetDisplayMode returns kPictureInPicture for the child WebContents.
TEST_F(DocumentPipHostTest, GetDisplayMode_ReturnsPictureInPicture) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  EXPECT_EQ(blink::mojom::DisplayMode::kPictureInPicture,
            host->GetDisplayMode(host->GetChildWebContents()));
}

// When the child requests closure, the host tears down the widget (and with it
// the child WebContents) but stays alive on the opener.
TEST_F(DocumentPipHostTest, CloseContents_TearsDownWidgetAndChild) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);
  content::WebContents* child = host->GetChildWebContents();
  content::WebContentsDestroyedWatcher child_destroyed_watcher(child);

  // Simulate the child requesting its own closure.
  host->CloseContents(child);

  // The host remains attached to the opener.
  EXPECT_EQ(host, DocumentPipHost::FromWebContents(opener()));
  // But the widget and child WebContents are gone.
  EXPECT_EQ(nullptr, host->GetWidget());
  EXPECT_EQ(nullptr, host->GetChildWebContents());
  EXPECT_TRUE(child_destroyed_watcher.IsDestroyed());
}

// Destroying the opener WebContents destroys the host cleanly.
// DocumentPipHost is a WebContentsUserData, so it is automatically destroyed
// when the opener WebContents is destroyed. The destructor calls
// ClosePipWindow().
TEST_F(DocumentPipHostTest, OpenerDestroyed_HostClosedCleanly) {
  CreateHostAndOpenPipWindow();
  ASSERT_TRUE(DocumentPipHost::FromWebContents(opener()));

  // Destroy the opener; the host is destroyed as UserData without crashing.
  opener_web_contents_.reset();
}

// After the PiP window is closed, CreatePipWidget() can be called again with a
// new child WebContents to re-open the PiP window.
TEST_F(DocumentPipHostTest, ReopenAfterClose) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  // Close the first PiP window.
  host->CloseContents(host->GetChildWebContents());
  EXPECT_EQ(nullptr, host->GetWidget());
  EXPECT_EQ(nullptr, host->GetChildWebContents());

  // Re-open with a new child WebContents.
  auto new_child =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  host->CreatePipWidget(std::move(new_child), MakeDefaultPipOptions());

  EXPECT_TRUE(host->GetWidget());
  EXPECT_TRUE(host->GetChildWebContents());
}

// Regression test for a leak of the WidgetDelegate: with CLIENT_OWNS_WIDGET
// and without SetOwnedByWidget(), the Widget never deletes its delegate, so
// the host must own and destroy it. Verify the delegate is actually destroyed
// (not just unlinked) both when the PiP window is closed and when the host
// itself goes away.
TEST_F(DocumentPipHostTest, ClosePipWindow_DestroysWidgetDelegate) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  auto* delegate = static_cast<DocumentPipWidgetDelegate*>(
      host->GetWidget()->widget_delegate());
  ASSERT_TRUE(delegate);
  base::WeakPtr<DocumentPipWidgetDelegate> delegate_weak =
      delegate->GetWeakPtr();

  host->CloseContents(host->GetChildWebContents());

  EXPECT_FALSE(delegate_weak);
}

TEST_F(DocumentPipHostTest, OpenerDestroyed_DestroysWidgetDelegate) {
  DocumentPipHost* host = CreateHostAndOpenPipWindow();
  ASSERT_TRUE(host);

  auto* delegate = static_cast<DocumentPipWidgetDelegate*>(
      host->GetWidget()->widget_delegate());
  ASSERT_TRUE(delegate);
  base::WeakPtr<DocumentPipWidgetDelegate> delegate_weak =
      delegate->GetWeakPtr();

  // Destroying the opener WebContents destroys the host (it is UserData),
  // which in turn must destroy the delegate.
  opener_web_contents_.reset();

  EXPECT_FALSE(delegate_weak);
}
