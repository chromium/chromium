// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_contents_view.h"

#include <memory>
#include <utility>

#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class DocumentPipContentsViewTest : public ChromeViewsTestBase {
 protected:
  std::unique_ptr<content::WebContents> CreateChildWebContents() {
    return content::WebContentsTester::CreateTestWebContents(&profile_,
                                                             nullptr);
  }

  // Must be declared before |profile_| because TestingProfile may post tasks.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
};

// The constructor transfers ownership of the child WebContents to the
// DocumentPipContentsView (a views::WebView) and exposes it via
// web_contents().
TEST_F(DocumentPipContentsViewTest, ConstructionTransfersWebContentsToWebView) {
  auto child = CreateChildWebContents();
  content::WebContents* child_raw = child.get();

  DocumentPipContentsView view(&profile_, std::move(child));

  EXPECT_EQ(child_raw, view.web_contents());
}

// Destroying the contents view destroys the child WebContents it owns.
TEST_F(DocumentPipContentsViewTest, DestructionDestroysChildWebContents) {
  auto child = CreateChildWebContents();
  content::WebContentsDestroyedWatcher watcher(child.get());

  auto view =
      std::make_unique<DocumentPipContentsView>(&profile_, std::move(child));
  view.reset();

  EXPECT_TRUE(watcher.IsDestroyed());
}
