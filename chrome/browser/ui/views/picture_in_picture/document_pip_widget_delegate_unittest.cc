// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_widget_delegate.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/picture_in_picture/document_pip_contents_view.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

class DocumentPipWidgetDelegateTest : public ChromeViewsTestBase {
 protected:
  std::unique_ptr<content::WebContents> CreateChildWebContents() {
    return content::WebContentsTester::CreateTestWebContents(&profile_,
                                                             nullptr);
  }

  content::WebContents* opener() {
    if (!opener_web_contents_) {
      opener_web_contents_ =
          content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
      DocumentPipHost::CreateForWebContents(opener_web_contents_.get());
    }
    return opener_web_contents_.get();
  }

  DocumentPipHost* host() { return DocumentPipHost::FromWebContents(opener()); }

  // Must be declared before |profile_| because TestingProfile may post tasks.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> opener_web_contents_;
};

// The delegate installs a DocumentPipContentsView via WidgetDelegate's
// SetContentsView(), and GetDocumentPipContentsView() returns the same pointer.
TEST_F(DocumentPipWidgetDelegateTest,
       GetContentsViewIsDocumentPipContentsView) {
  DocumentPipWidgetDelegate delegate(host(), CreateChildWebContents());

  DocumentPipContentsView* typed = delegate.GetDocumentPipContentsView();
  ASSERT_TRUE(typed);
  EXPECT_EQ(static_cast<views::View*>(typed), delegate.GetContentsView());
}

// The child WebContents passed to the delegate ends up hosted in the
// contents view (a views::WebView).
TEST_F(DocumentPipWidgetDelegateTest, ChildWebContentsHostedInWebView) {
  auto child = CreateChildWebContents();
  content::WebContents* child_raw = child.get();

  DocumentPipWidgetDelegate delegate(host(), std::move(child));

  DocumentPipContentsView* contents_view =
      delegate.GetDocumentPipContentsView();
  ASSERT_TRUE(contents_view);
  EXPECT_EQ(child_raw, contents_view->web_contents());
}

// Widget capability flags are locked in by the constructor: resizable but not
// maximizable, minimizable, or fullscreen-capable.
TEST_F(DocumentPipWidgetDelegateTest, WidgetCapabilities) {
  DocumentPipWidgetDelegate delegate(host(), CreateChildWebContents());

  EXPECT_TRUE(delegate.CanResize());
  EXPECT_FALSE(delegate.CanMaximize());
  EXPECT_FALSE(delegate.CanMinimize());
  EXPECT_FALSE(delegate.CanFullscreen());
}

// use_desktop_widget_override() must be true so that ChromeViewsDelegate
// selects DesktopNativeWidgetAura without requiring params.context.
TEST_F(DocumentPipWidgetDelegateTest, UseDesktopWidgetOverrideIsTrue) {
  DocumentPipWidgetDelegate delegate(host(), CreateChildWebContents());

  EXPECT_TRUE(delegate.use_desktop_widget_override());
}
