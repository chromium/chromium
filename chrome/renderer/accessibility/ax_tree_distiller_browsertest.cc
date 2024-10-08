// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/ax_tree_distiller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

class AXTreeDistillerTestBase : public ChromeRenderViewTest {
 public:
  AXTreeDistillerTestBase() = default;
  AXTreeDistillerTestBase(const AXTreeDistillerTestBase&) = delete;
  AXTreeDistillerTestBase& operator=(const AXTreeDistillerTestBase&) = delete;
  ~AXTreeDistillerTestBase() override = default;

  void DistillPage(const char* html,
                   const std::vector<std::string>& expected_node_contents) {
    expected_node_contents_ = expected_node_contents;
    LoadHTML(html);
    content::RenderFrame* render_frame =
        content::RenderFrame::FromWebFrame(GetMainFrame());
    ui::AXTreeUpdate snapshot;
    // |ui::AXMode::kHTML| is needed for retrieving the presence of the
    // "aria-expanded" attribute.
    // TODO(crbug.com/366000250): This is a heavy-handed approach as it copies
    // all HTML attributes into the accessibility tree. It should be removed
    // ASAP.
    //
    // |ui::AXMode::kScreenReader| is needed for heading level information.
    const ui::AXMode ax_mode = ui::AXMode::kWebContents | ui::AXMode::kHTML |
                               ui::AXMode::kScreenReader;
    render_frame->CreateAXTreeSnapshotter(ax_mode)->Snapshot(
        /* max_nodes= */ 0,
        /* timeout= */ {}, &snapshot);
    ui::AXTree tree(snapshot);
    distiller_ = std::make_unique<AXTreeDistiller>(
        render_frame,
        base::BindRepeating(&AXTreeDistillerTestBase::OnAXTreeDistilled,
                            base::Unretained(this), &tree));
    distiller_->Distill(tree, snapshot, ukm::kInvalidSourceId);
  }

  void OnAXTreeDistilled(ui::AXTree* tree,
                         const ui::AXTreeID& tree_id,
                         const std::vector<int32_t>& content_node_ids) {
    // Content node IDs list should be the same length as
    // |expected_node_contents_|.
    EXPECT_EQ(content_node_ids.size(), expected_node_contents_.size());
    EXPECT_EQ(tree_id, tree->GetAXTreeID());

    // Iterate through each content node ID from distiller and check that the
    // text value equals the passed-in string from |expected_node_contents_|.
    for (size_t i = 0; i < content_node_ids.size(); i++) {
      ui::AXNode* node = tree->GetFromId(content_node_ids[i]);
      EXPECT_TRUE(node);
      EXPECT_TRUE(node->GetTextContentLengthUTF8());
      EXPECT_EQ(node->GetTextContentUTF8(), expected_node_contents_[i]);
    }
  }

 private:
  std::unique_ptr<AXTreeDistiller> distiller_;
  std::vector<std::string> expected_node_contents_;
};

struct TestCase {
  const char* test_name;
  const char* html;
  std::vector<std::string> expected_node_contents;
};

class AXTreeDistillerTest : public AXTreeDistillerTestBase,
                            public ::testing::WithParamInterface<TestCase> {
 public:
  static std::string ParamInfoToString(
      ::testing::TestParamInfo<TestCase> param_info) {
    return param_info.param.test_name;
  }
};

// AXTreeDistillerTest is a parameterized test. Add each test case to the object
// below in the form of struct |TestCase|. Include a dividing line as a comment
// for easy reading.
const TestCase kDistillWebPageTestCases[] = {
    {"simple_page",
     R"HTML(<!doctype html>
      <body role="main">
        <p>Test</p>
      <body>)HTML",
     {"Test"}},
    /* ----------------------- */
    {"simple_page_with_main",
     R"HTML(<!doctype html>
      <body role="main">
        <h1>Heading</h1>
        <p>Test 1</p>
        <p>Test 2</p>
        <div role='header'><h2>Header</h2></div>
      <body>)HTML",
     {"Heading", "Test 1", "Test 2", "Header"}},
    /* ----------------------- */
    {"simple_page_with_main_and_article",
     R"HTML(<!doctype html>
      <body>
        <main>
          <p>Main</p>
        </main>
        <div role="article">
          <p>Article 1</p>
        </div>
        <div role="article">
          <p>Article 2</p>
        </div>
      <body>)HTML",
     {"Main", "Article 1", "Article 2"}},
    /* ----------------------- */
    {"simple_page_no_content",
     R"HTML(<!doctype html>
      <body>
        <main>
          <div role='banner'>Banner</div>
          <div role="navigation'>Navigation</div>
          <audio>Audio</audio>
          <img alt='Image alt'></img>
          <button>Button</button>
          <div aria-label='Label'></div>
          <div role='complementary'>Complementary</div>
          <div role='content'>Content Info</div>
          <footer>Footer</footer>
        </main>
      <body>)HTML",
     {}},
    /* ----------------------- */
    {"simple_page_no_main",
     R"HTML(<!doctype html>
      <body>
        <div tabindex='0'>
          <p>Paragraph</p>
          <p>Paragraph</p>
        </div>
      <body>)HTML",
     {}},
    /* ----------------------- */
    {"include_paragraphs_in_collapsed_nodes",
     R"HTML(<!doctype html>
      <body role="main">
        <p>P1</p>
        <div>
          <p>P2</p>
          <p>P3</p>
        </div>
      <body>)HTML",
     {"P1", "P2", "P3"}},
    /* ----------------------- */
    {"main_may_be_deep_in_tree",
     R"HTML(<!doctype html>
      <body>
        <p>P1</p>
        <main>
          <p>P2</p>
          <p>P3</p>
        </main>
      <body>)HTML",
     {"P2", "P3"}},
    /* ----------------------- */
    {"paragraph_with_bold",
     R"HTML(<!doctype html>
      <body role="main">
        <p>Some <b>bolded</b> text</p>
      <body>)HTML",
     {"Some bolded text"}},
    /* ----------------------- */
    {"simple_page_nested_article",
     R"HTML(<!doctype html>
      <body>
        <div role="main">
          <p>Main</p>
          <div role="article">
            <p>Article 1</p>
          </div>
        </div>
        <div role="article">
          <p>Article 2</p>
          <div role="article">
            <p>Article 3</p>
          </div>
        </div>
      <body>)HTML",
     {"Main", "Article 1", "Article 2", "Article 3"}},
    /* ----------------------- */
    {"simple_page_with_heading_outside_of_main",
     R"HTML(<!doctype html>
      <body>
        <h1>Heading</h1>
        <main>
          <p>Main</p>
        </main>
      <body>)HTML",
     {"Heading", "Main"}},
    /* ----------------------- */
    {"simple_page_with_heading_no_main",
     R"HTML(<!doctype html>
      <body>
        <h1>Heading</h1>
      <body>)HTML",
     {}},
    /* ----------------------- */
    {"simple_page_heading_offscreen",
     R"HTML(<!doctype html>
      <body>
        <h1 style="
        position: absolute;
        left: -10000px;
        top: -10000px;
        width: 1px;
        height: 1px;"
        >
          Heading
        </h1>
        <main>
          <p>Main</p>
        </main>
      <body>)HTML",
     {"Main"}},
    /* ----------------------- */
    {"simple_page_aria_expanded",
     R"HTML(<!doctype html>
      <body>
        <main>
          <p>Main</p>
          <div aria-expanded='true'>Expanded</div>
          <div aria-expanded='false'>Collapsed</div>
        </main>
      <body>)HTML",
     {"Main", "Expanded"}},
};

TEST_P(AXTreeDistillerTest, DistillsWebPage) {
  TestCase param = GetParam();
  DistillPage(param.html, param.expected_node_contents);
}

INSTANTIATE_TEST_SUITE_P(/* prefix */,
                         AXTreeDistillerTest,
                         ::testing::ValuesIn(kDistillWebPageTestCases),
                         AXTreeDistillerTest::ParamInfoToString);
