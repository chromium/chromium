// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/content/content_infobar_manager.h"

#include <memory>
#include <string>
#include <optional>

#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace infobars {
namespace {

class TestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  TestWebContentsDelegate() = default;
  ~TestWebContentsDelegate() override = default;

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    last_params_ = params;
    return source;
  }

  const std::optional<content::OpenURLParams>& last_params() const {
    return last_params_;
  }

  void reset() { last_params_.reset(); }

 private:
  std::optional<content::OpenURLParams> last_params_;
};

class ContentInfoBarManagerTest : public content::RenderViewHostTestHarness {
 protected:
  ContentInfoBarManagerTest() = default;
  ~ContentInfoBarManagerTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ContentInfoBarManager::CreateForWebContents(web_contents());
    web_contents()->SetDelegate(&delegate_);
  }

  void TearDown() override {
    web_contents()->SetDelegate(nullptr);
    content::RenderViewHostTestHarness::TearDown();
  }

  ContentInfoBarManager* manager() {
    return ContentInfoBarManager::FromWebContents(web_contents());
  }

  TestWebContentsDelegate& delegate() { return delegate_; }

 private:
  TestWebContentsDelegate delegate_;
};

TEST_F(ContentInfoBarManagerTest, OpenURLWithTextFragment) {
  GURL url("https://example.com");
  std::string text_fragment = "text=hello";

  // Trigger OpenURL with a text fragment.
  manager()->OpenURL(url, WindowOpenDisposition::CURRENT_TAB, text_fragment);

  ASSERT_TRUE(delegate().last_params().has_value());
  EXPECT_EQ(url, delegate().last_params()->url);
  // A CURRENT_TAB disposition gets converted to NEW_FOREGROUND_TAB in OpenURL.
  EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB,
            delegate().last_params()->disposition);
  EXPECT_TRUE(
      delegate().last_params()->internal_scroll_to_text_fragment.has_value());
  EXPECT_EQ(text_fragment,
            *delegate().last_params()->internal_scroll_to_text_fragment);

  delegate().reset();

  // Trigger OpenURL with an empty text fragment.
  manager()->OpenURL(url, WindowOpenDisposition::NEW_FOREGROUND_TAB, "");

  ASSERT_TRUE(delegate().last_params().has_value());
  EXPECT_EQ(url, delegate().last_params()->url);
  EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB,
            delegate().last_params()->disposition);
  EXPECT_FALSE(
      delegate().last_params()->internal_scroll_to_text_fragment.has_value());
}

}  // namespace
}  // namespace infobars
