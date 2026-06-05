// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/markdown_builder.h"

#include "base/strings/string_util.h"
#include "base/test/test_future.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace ttc {

namespace {

class MarkdownBuilderBrowserTest : public InProcessBrowserTest {
 public:
  std::string GetMarkdownFromHtml(const std::string& html) {
    return GetMarkdownFromHtmlAndUrl(html, GURL("data:text/html," + html));
  }

  std::string GetMarkdownFromHtmlAndUrl(const std::string& html,
                                        const GURL& page_url) {
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveTab(this)->GetContents();
    GURL url("data:text/html," + html);
    EXPECT_TRUE(content::NavigateToURL(web_contents, url));

    base::test::TestFuture<optimization_guide::AIPageContentResultOrError>
        future;
    optimization_guide::GetAIPageContent(
        web_contents, optimization_guide::DefaultAIPageContentOptions(true),
        future.GetCallback());

    auto result = future.Take();
    EXPECT_TRUE(result.has_value()) << result.error();
    if (!result.has_value()) {
      return "";
    }

    MarkdownBuilder builder(result->proto, page_url);
    return builder.Build();
  }

  void RunTest(const std::string& html, const std::string& expected) {
    RunTestWithUrl(html, GURL("data:text/html," + html), expected);
  }

  void RunTestWithUrl(const std::string& html,
                      const GURL& page_url,
                      const std::string& expected) {
    std::string trimmed_html;
    base::TrimWhitespaceASCII(html, base::TRIM_ALL, &trimmed_html);
    std::string trimmed_expected;
    base::TrimWhitespaceASCII(expected, base::TRIM_ALL, &trimmed_expected);
    std::string actual = GetMarkdownFromHtmlAndUrl(trimmed_html, page_url);
    re2::RE2::GlobalReplace(&actual, "\\{#\\d+\\}", "");
    EXPECT_EQ(actual, trimmed_expected);
  }
};

IN_PROC_BROWSER_TEST_F(MarkdownBuilderBrowserTest, SimpleText) {
  // clang-format off
  const std::string html = R"HTML(
Hello World
)HTML";
  const std::string expected = R"EXPECTED(
Hello World
)EXPECTED";
  // clang-format on
  RunTest(html, expected);
}

IN_PROC_BROWSER_TEST_F(MarkdownBuilderBrowserTest, HeadingAndParagraph) {
  // clang-format off
  const std::string html = R"HTML(
<h1>Title</h1>
<p>This is a paragraph.</p>
)HTML";
  const std::string expected = R"EXPECTED(
# Title
This is a paragraph.
)EXPECTED";
  // clang-format on
  RunTest(html, expected);
}

IN_PROC_BROWSER_TEST_F(MarkdownBuilderBrowserTest, UnorderedList) {
  // clang-format off
  const std::string html = R"HTML(
<ul>
  <li>Item 1</li>
  <li>Item 2</li>
</ul>
)HTML";
  const std::string expected = R"EXPECTED(
- Item 1
- Item 2
)EXPECTED";
  // clang-format on
  RunTest(html, expected);
}

IN_PROC_BROWSER_TEST_F(MarkdownBuilderBrowserTest, Table) {
  // Note: caption text may be duplicated if it's both in table_name and a child
  // node. Also table headers are often emphasized due to default styling.
  // clang-format off
  const std::string html = R"HTML(
<table>
  <caption>My Table</caption>
  <thead>
    <tr>
      <th>Col 1</th>
      <th>Col 2</th>
    </tr>
  </thead>
</table>
)HTML";
  const std::string expected = R"EXPECTED(
My Table My Table
| *Col 1* | *Col 2* |
| ----- | ----- |
)EXPECTED";
  // clang-format on
  RunTest(html, expected);
}

IN_PROC_BROWSER_TEST_F(MarkdownBuilderBrowserTest, Redaction) {
  // clang-format off
  const std::string html = R"HTML(
<input type="password" value="secret123">
<input type="password" value="secret456">
)HTML";
  const std::string expected = R"EXPECTED(
___<redacted password>
___<redacted password>
)EXPECTED";
  // clang-format on

  // Password fields should be redacted.
  RunTest(html, expected);
}

IN_PROC_BROWSER_TEST_F(MarkdownBuilderBrowserTest, NestedContent) {
  // clang-format off
  const std::string html = R"HTML(
<div>
  <h1>Title 1</h1>
  <p>P1.1</p>
  <p>P1.2</p>
  <div>
    <h2>Title 2</h2>
    <p>P2.1</p>
    <div>
      <p>P3.1</p>
      <h3>Title 3</h3>
      <p>P3.2</p>
    </div>
  </div>
</div>
)HTML";
  const std::string expected = R"EXPECTED(
# Title 1
P1.1
P1.2
## Title 2
P2.1
P3.1
## Title 3
P3.2
)EXPECTED";
  // clang-format on
  RunTest(html, expected);
}

IN_PROC_BROWSER_TEST_F(MarkdownBuilderBrowserTest, Links) {
  // clang-format off
  const std::string html = R"HTML(
<p>
  <a href="https://google.com/">Simple Link</a>
</p>
<p>
  <a href="https://example.com/">Link with <b>nested</b> DOM</a>
</p>
<p>
  <a href="https://chromium.org/"><img alt="Chromium Logo"></a>
</p>
<div>
  <a href="https://parent.com/">
    Parent Link
    <span>
      <a href="https://child.com/">Child Link</a>
    </span>
  </a>
</div>
)HTML";
  // Current behavior:
  // 1. Emphasis is suppressed inside anchors.
  // 2. Images inside anchors only output their caption/alt text.
  // 3. Nested anchors: are correctly handled as separate links.
  const std::string expected = R"EXPECTED(
[Simple Link (google.com)]
[Link with nested DOM (example.com)]
[Chromium Logo (chromium.org)]
[Parent Link (parent.com)] [Child Link (child.com)]
)EXPECTED";
  // clang-format on
  RunTest(html, expected);
}

IN_PROC_BROWSER_TEST_F(MarkdownBuilderBrowserTest, OrderedList) {
  // clang-format off
  const std::string html = R"HTML(
<ol>
  <li>First</li>
  <li>Second</li>
</ol>
)HTML";
  const std::string expected = R"EXPECTED(
1. First
2. Second
)EXPECTED";
  // clang-format on
  RunTest(html, expected);
}

IN_PROC_BROWSER_TEST_F(MarkdownBuilderBrowserTest, LinksSameAndDifferentHost) {
  // clang-format off
  const std::string html = R"HTML(
<p>
  <a href="https://google.com/page1">Same Domain</a>
  <a href="https://example.com/page2">Different Domain</a>
</p>
)HTML";
  // clang-format on
  RunTestWithUrl(html, GURL("https://google.com/home"),
                 "[Same Domain] [Different Domain (example.com)]");
}

}  // namespace

}  // namespace ttc
