// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"

namespace content_capture {

static constexpr char kTestUrl[] = "/secure_embed/embed_tag.html";

class SecureEmbedBrowserTest : public content::ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    const GURL test_url = embedded_test_server()->GetURL(kTestUrl);
    ASSERT_TRUE(NavigateToURL(web_contents(), test_url));
  }

  void TearDownOnMainThread() override {
    content::ContentBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }
};

IN_PROC_BROWSER_TEST_F(SecureEmbedBrowserTest, EmbedTagCreatesPlugin) {
  // TODO: add expectations of the creation of SecureEmbedHost and other browser
  // side objects.
}

}  // namespace content_capture
