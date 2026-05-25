// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"

#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/picture_in_picture_window_options/picture_in_picture_window_options.mojom.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

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

    opener_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    ASSERT_TRUE(opener_web_contents_);

    // Host the opener WebContents inside a test top-level widget so that
    // `opener->GetTopLevelNativeWindow()` returns a real native window. On
    // Aura this is required for `views::Widget::Init` (params.parent ||
    // params.context).
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

  // Creates a DocumentPipHost attached to the opener, returns the host.
  DocumentPipHost* CreateHost() {
    auto child =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    DocumentPipHost::CreateForWebContents(opener(), std::move(child),
                                          MakeDefaultPipOptions());
    auto* host = DocumentPipHost::FromWebContents(opener());
    host->CreatePipWidget();
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
  CreateHost();
  EXPECT_TRUE(DocumentPipHost::FromWebContents(opener()));
}

// Calling CreateForWebContents a second time is a no-op.
TEST_F(DocumentPipHostTest, CreateForWebContents_IdempotentWhenExists) {
  DocumentPipHost* first = CreateHost();
  ASSERT_TRUE(first);

  auto extra_child =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  DocumentPipHost::CreateForWebContents(opener(), std::move(extra_child),
                                        MakeDefaultPipOptions());

  EXPECT_EQ(first, DocumentPipHost::FromWebContents(opener()));
}

// The host should expose the opener, child WebContents, profile, and options.
TEST_F(DocumentPipHostTest, Accessors) {
  DocumentPipHost* host = CreateHost();
  ASSERT_TRUE(host);

  EXPECT_EQ(opener(), host->GetOpenerWebContents());
  EXPECT_TRUE(host->GetChildWebContents());
  EXPECT_NE(opener(), host->GetChildWebContents());
  EXPECT_TRUE(host->GetProfile());
  EXPECT_EQ(400u, host->GetPipOptions().width);
  EXPECT_EQ(300u, host->GetPipOptions().height);
}

// The host creates a placeholder Widget.
TEST_F(DocumentPipHostTest, WidgetIsCreated) {
  DocumentPipHost* host = CreateHost();
  ASSERT_TRUE(host);

  views::Widget* w = host->GetWidget();
  ASSERT_TRUE(w);
  EXPECT_FALSE(w->IsClosed());
}

// GetDisplayMode returns kPictureInPicture for the child WebContents.
TEST_F(DocumentPipHostTest, GetDisplayMode_ReturnsPictureInPicture) {
  DocumentPipHost* host = CreateHost();
  ASSERT_TRUE(host);

  EXPECT_EQ(blink::mojom::DisplayMode::kPictureInPicture,
            host->GetDisplayMode(host->GetChildWebContents()));
}

// When the child requests closure, the host tears down the widget and child
// WebContents but stays alive on the opener.
TEST_F(DocumentPipHostTest, CloseContents_TearsDownWidgetAndChild) {
  DocumentPipHost* host = CreateHost();
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
  CreateHost();
  ASSERT_TRUE(DocumentPipHost::FromWebContents(opener()));

  // Destroy the opener; the host is destroyed as UserData without crashing.
  opener_web_contents_.reset();
}
