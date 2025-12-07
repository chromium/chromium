// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_extraction/content/browser/inner_text.h"

#include "base/test/test_future.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

using InnerTextBrowserTest = content::ContentBrowserTest;

namespace content_extraction {

IN_PROC_BROWSER_TEST_F(InnerTextBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = shell()->web_contents();
  ASSERT_TRUE(content::NavigateToURL(
      shell(), embedded_test_server()->GetURL("/inner_text/test1.html")));
  base::test::TestFuture<std::unique_ptr<InnerTextResult>> future;
  GetInnerText(*web_contents->GetPrimaryMainFrame(), {}, future.GetCallback());
  std::unique_ptr<InnerTextResult> result = future.Take();
  ASSERT_TRUE(result);
  // Inner-text result is combined as followed:
  // test1 contains "A<a>B C<b>D"
  // <a> is subframe-a, which contains "a"
  // <b> is subframe-b, which contains "b<a>2"
  EXPECT_EQ("AaB Cb a2D", result->inner_text);
}

}  // namespace content_extraction
