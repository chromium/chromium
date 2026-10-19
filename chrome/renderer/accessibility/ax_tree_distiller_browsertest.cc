// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/ax_tree_distiller.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>
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
    // |ui::AXMode::kExtendedProperties| is needed for heading level
    // information.
    const ui::AXMode ax_mode = ui::AXMode::kWebContents | ui::AXMode::kHTML |
                               ui::AXMode::kExtendedProperties;
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

  void DistillAXTree(ui::AXTree& tree,
                     const ui::AXTreeUpdate& snapshot,
                     const std::vector<std::string>& expected_node_contents) {
    expected_node_contents_ = expected_node_contents;
    content::RenderFrame* render_frame =
        content::RenderFrame::FromWebFrame(GetMainFrame());
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
  // Unused trailing entries are empty.
  std::array<std::string_view, 4> expected_node_contents;
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
    // Ensure Gmail thread support.
    {"simple_page_aria_expanded",
     R"HTML(<!doctype html>
      <body>
        <main>
          <p>Main</p>
          <div role='list'>
            <div role='listitem' aria-expanded='true'>Expanded</div>
            <div role='listitem' aria-expanded='false'>Collapsed</div>
          </div>
        </main>
      <body>)HTML",
     {"Main", "Expanded"}},
    /* ----------------------- */
    {"simple_page_list_items",
     R"HTML(<!doctype html>
      <body>
        <main>
          <ul>
            <li><b>Bold</b> lead-in</li>
            <li>Item <i>italic</i> text</li>
          </ul>
        </main>
      <body>)HTML",
     {"\xE2\x80\xA2 Bold lead-in", "\xE2\x80\xA2 Item italic text"}},
    /* ----------------------- */
    // A list item is picked as a single content node, rather than each
    // paragraph inside it.
    {"simple_page_list_item_with_paragraphs",
     R"HTML(<!doctype html>
      <body>
        <main>
          <ul>
            <li>
              <p>P1</p>
              <p>P2</p>
            </li>
          </ul>
        </main>
      <body>)HTML",
     {"\xE2\x80\xA2 P1P2"}},
    /* ----------------------- */
    {"simple_page_aria_list_item_with_paragraphs",
     R"HTML(<!doctype html>
      <body>
        <main>
          <div role='list'>
            <div role='listitem'>
              <p>P1</p>
              <p>P2</p>
            </div>
          </div>
        </main>
      <body>)HTML",
     {"P1P2"}},
    /* ----------------------- */
    // A collapsed list item is not content itself, but its descendants are
    // still searched.
    {"simple_page_collapsed_list_item_with_paragraph",
     R"HTML(<!doctype html>
      <body>
        <main>
          <p>Main</p>
          <div role='list'>
            <div role='listitem' aria-expanded='false'>
              <p>Inside collapsed</p>
            </div>
          </div>
        </main>
      <body>)HTML",
     {"Main", "Inside collapsed"}},
    /* ----------------------- */
    {"simple_page_table",
     R"HTML(<!doctype html>
      <body>
        <main>
          <table>
            <tr><th>H1</th><th>H2</th></tr>
            <tr><td>C1</td><td>C2</td></tr>
          </table>
        </main>
      <body>)HTML",
     {"H1H2C1C2"}},
    /* ----------------------- */
    // A table is picked as a single content node, rather than each paragraph
    // inside it.
    {"simple_page_table_with_paragraphs",
     R"HTML(<!doctype html>
      <body>
        <main>
          <p>Main</p>
          <table>
            <tr><th>H1</th></tr>
            <tr><td><p>P1</p><p>P2</p></td></tr>
          </table>
        </main>
      <body>)HTML",
     {"Main", "H1P1P2"}},
    /* ----------------------- */
    // Layout tables are not content themselves, but their descendants are
    // still searched.
    {"simple_page_layout_table_with_paragraphs",
     R"HTML(<!doctype html>
      <body>
        <main>
          <table>
            <tr><td><p>P1</p><p>P2</p></td></tr>
          </table>
        </main>
      <body>)HTML",
     {"P1", "P2"}},
};

TEST_P(AXTreeDistillerTest, DistillsWebPage) {
  TestCase param = GetParam();
  std::vector<std::string> expected_node_contents;
  for (std::string_view content : param.expected_node_contents) {
    if (!content.empty()) {
      expected_node_contents.emplace_back(content);
    }
  }
  DistillPage(param.html, expected_node_contents);
}

INSTANTIATE_TEST_SUITE_P(/* prefix */,
                         AXTreeDistillerTest,
                         ::testing::ValuesIn(kDistillWebPageTestCases),
                         AXTreeDistillerTest::ParamInfoToString);

TEST_F(AXTreeDistillerTestBase, DistillPdfRoot) {
  ui::AXTreeUpdate snapshot;
  ui::AXNodeData pdf_root_node;
  pdf_root_node.id = 1;
  pdf_root_node.role = ax::mojom::Role::kPdfRoot;

  ui::AXNodeData heading_node;
  heading_node.id = 2;
  heading_node.role = ax::mojom::Role::kHeading;

  ui::AXNodeData heading_text_node;
  heading_text_node.id = 4;
  heading_text_node.role = ax::mojom::Role::kStaticText;
  heading_text_node.SetName("Heading");
  heading_text_node.SetNameFrom(ax::mojom::NameFrom::kContents);

  heading_node.child_ids = {heading_text_node.id};

  ui::AXNodeData paragraph_node;
  paragraph_node.id = 3;
  paragraph_node.role = ax::mojom::Role::kParagraph;

  ui::AXNodeData paragraph_text_node;
  paragraph_text_node.id = 5;
  paragraph_text_node.role = ax::mojom::Role::kStaticText;
  paragraph_text_node.SetName("Paragraph");
  paragraph_text_node.SetNameFrom(ax::mojom::NameFrom::kContents);

  paragraph_node.child_ids = {paragraph_text_node.id};

  pdf_root_node.child_ids = {heading_node.id, paragraph_node.id};
  snapshot.root_id = pdf_root_node.id;
  snapshot.nodes = {pdf_root_node, heading_node, heading_text_node,
                    paragraph_node, paragraph_text_node};
  snapshot.has_tree_data = true;
  snapshot.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();

  ui::AXTree tree(snapshot);
  DistillAXTree(tree, snapshot, {"Heading", "Paragraph"});
}

TEST_F(AXTreeDistillerTestBase, DistillPdfListItem) {
  ui::AXTreeUpdate snapshot;
  ui::AXNodeData pdf_root_node;
  pdf_root_node.id = 1;
  pdf_root_node.role = ax::mojom::Role::kPdfRoot;

  ui::AXNodeData list_node;
  list_node.id = 2;
  list_node.role = ax::mojom::Role::kList;

  ui::AXNodeData list_item_node;
  list_item_node.id = 3;
  list_item_node.role = ax::mojom::Role::kListItem;

  // List items in PDFs have static text children directly, without a
  // paragraph in between.
  ui::AXNodeData bold_text_node;
  bold_text_node.id = 4;
  bold_text_node.role = ax::mojom::Role::kStaticText;
  bold_text_node.SetName("Bold");
  bold_text_node.SetNameFrom(ax::mojom::NameFrom::kContents);

  ui::AXNodeData text_node;
  text_node.id = 5;
  text_node.role = ax::mojom::Role::kStaticText;
  text_node.SetName(" lead-in");
  text_node.SetNameFrom(ax::mojom::NameFrom::kContents);

  list_item_node.child_ids = {bold_text_node.id, text_node.id};
  list_node.child_ids = {list_item_node.id};
  pdf_root_node.child_ids = {list_node.id};
  snapshot.root_id = pdf_root_node.id;
  snapshot.nodes = {pdf_root_node, list_node, list_item_node, bold_text_node,
                    text_node};
  snapshot.has_tree_data = true;
  snapshot.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();

  ui::AXTree tree(snapshot);
  DistillAXTree(tree, snapshot, {"Bold lead-in"});
}

TEST_F(AXTreeDistillerTestBase, DistillPdfTable) {
  ui::AXTreeUpdate snapshot;
  ui::AXNodeData pdf_root_node;
  pdf_root_node.id = 1;
  pdf_root_node.role = ax::mojom::Role::kPdfRoot;

  ui::AXNodeData table_node;
  table_node.id = 2;
  table_node.role = ax::mojom::Role::kTable;

  ui::AXNodeData row_node;
  row_node.id = 3;
  row_node.role = ax::mojom::Role::kRow;

  ui::AXNodeData cell_node_1;
  cell_node_1.id = 4;
  cell_node_1.role = ax::mojom::Role::kCell;

  ui::AXNodeData cell_text_node_1;
  cell_text_node_1.id = 5;
  cell_text_node_1.role = ax::mojom::Role::kStaticText;
  cell_text_node_1.SetName("Cell 1");
  cell_text_node_1.SetNameFrom(ax::mojom::NameFrom::kContents);

  ui::AXNodeData cell_node_2;
  cell_node_2.id = 6;
  cell_node_2.role = ax::mojom::Role::kCell;

  ui::AXNodeData cell_text_node_2;
  cell_text_node_2.id = 7;
  cell_text_node_2.role = ax::mojom::Role::kStaticText;
  cell_text_node_2.SetName("Cell 2");
  cell_text_node_2.SetNameFrom(ax::mojom::NameFrom::kContents);

  cell_node_1.child_ids = {cell_text_node_1.id};
  cell_node_2.child_ids = {cell_text_node_2.id};
  row_node.child_ids = {cell_node_1.id, cell_node_2.id};
  table_node.child_ids = {row_node.id};
  pdf_root_node.child_ids = {table_node.id};
  snapshot.root_id = pdf_root_node.id;
  snapshot.nodes = {pdf_root_node,    table_node,  row_node,        cell_node_1,
                    cell_text_node_1, cell_node_2, cell_text_node_2};
  snapshot.has_tree_data = true;
  snapshot.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();

  ui::AXTree tree(snapshot);
  DistillAXTree(tree, snapshot, {"Cell 1Cell 2"});
}
