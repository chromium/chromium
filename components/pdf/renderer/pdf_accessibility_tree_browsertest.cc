// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/pdf/renderer/pdf_accessibility_tree.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_accessibility.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdf_accessibility_action_handler.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/strings/grit/blink_accessibility_strings.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace pdf {

namespace {

const chrome_pdf::AccessibilityTextRunInfo kFirstTextRun = {
    15, gfx::RectF(26.0f, 189.0f, 84.0f, 13.0f),
    chrome_pdf::AccessibilityTextDirection::kNone,
    chrome_pdf::AccessibilityTextStyleInfo()};
const chrome_pdf::AccessibilityTextRunInfo kSecondTextRun = {
    15, gfx::RectF(28.0f, 117.0f, 152.0f, 19.0f),
    chrome_pdf::AccessibilityTextDirection::kNone,
    chrome_pdf::AccessibilityTextStyleInfo()};
const chrome_pdf::AccessibilityCharInfo kDummyCharsData[] = {
    {'H', 12}, {'e', 6},  {'l', 5},  {'l', 4},  {'o', 8},  {',', 4},
    {' ', 4},  {'w', 12}, {'o', 6},  {'r', 6},  {'l', 4},  {'d', 9},
    {'!', 4},  {' ', 0},  {' ', 0},  {'G', 16}, {'o', 12}, {'o', 12},
    {'d', 12}, {'b', 10}, {'y', 12}, {'e', 12}, {',', 4},  {' ', 6},
    {'w', 16}, {'o', 12}, {'r', 8},  {'l', 4},  {'d', 12}, {'!', 2},
};
const chrome_pdf::AccessibilityTextRunInfo kFirstRunMultiLine = {
    7, gfx::RectF(26.0f, 189.0f, 84.0f, 13.0f),
    chrome_pdf::AccessibilityTextDirection::kNone,
    chrome_pdf::AccessibilityTextStyleInfo()};
const chrome_pdf::AccessibilityTextRunInfo kSecondRunMultiLine = {
    8, gfx::RectF(26.0f, 189.0f, 84.0f, 13.0f),
    chrome_pdf::AccessibilityTextDirection::kNone,
    chrome_pdf::AccessibilityTextStyleInfo()};
const chrome_pdf::AccessibilityTextRunInfo kThirdRunMultiLine = {
    9, gfx::RectF(26.0f, 189.0f, 84.0f, 13.0f),
    chrome_pdf::AccessibilityTextDirection::kNone,
    chrome_pdf::AccessibilityTextStyleInfo()};
const chrome_pdf::AccessibilityTextRunInfo kFourthRunMultiLine = {
    6, gfx::RectF(26.0f, 189.0f, 84.0f, 13.0f),
    chrome_pdf::AccessibilityTextDirection::kNone,
    chrome_pdf::AccessibilityTextStyleInfo()};

const char kChromiumTestUrl[] = "www.cs.chromium.org";

void CompareRect(const gfx::RectF& expected_rect,
                 const gfx::RectF& actual_rect) {
  EXPECT_FLOAT_EQ(expected_rect.x(), actual_rect.x());
  EXPECT_FLOAT_EQ(expected_rect.y(), actual_rect.y());
  EXPECT_FLOAT_EQ(expected_rect.size().height(), actual_rect.size().height());
  EXPECT_FLOAT_EQ(expected_rect.size().width(), actual_rect.size().width());
}

constexpr uint32_t MakeARGB(unsigned int a,
                            unsigned int r,
                            unsigned int g,
                            unsigned int b) {
  return (a << 24) | (r << 16) | (g << 8) | b;
}

// This class overrides PdfAccessibilityActionHandler to record received
// action data when tests make an accessibility action call.
class TestPdfAccessibilityActionHandler
    : public chrome_pdf::PdfAccessibilityActionHandler {
 public:
  TestPdfAccessibilityActionHandler() = default;
  ~TestPdfAccessibilityActionHandler() override = default;

  // chrome_pdf::PdfAccessibilityActionHandler:
  void EnableAccessibility() override {}
  void HandleAccessibilityAction(
      const chrome_pdf::AccessibilityActionData& action_data) override {
    received_action_data_ = action_data;
  }

  chrome_pdf::AccessibilityActionData received_action_data() {
    return received_action_data_;
  }

 private:
  chrome_pdf::AccessibilityActionData received_action_data_;
};

// Waits for tasks posted to the thread's task runner to complete.
void WaitForThreadTasks() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace

class PdfAccessibilityTreeTest : public content::RenderViewTest {
 public:
  PdfAccessibilityTreeTest() {}
  ~PdfAccessibilityTreeTest() override = default;

  void SetUp() override {
    content::RenderViewTest::SetUp();

    base::FilePath pak_dir;
    base::PathService::Get(base::DIR_ASSETS, &pak_dir);
    base::FilePath pak_file =
        pak_dir.Append(FILE_PATH_LITERAL("components_tests_resources.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_file, ui::kScaleFactorNone);

    viewport_info_.zoom = 1.0;
    viewport_info_.scale = 1.0;
    viewport_info_.scroll = gfx::Point(0, 0);
    viewport_info_.offset = gfx::Point(0, 0);
    viewport_info_.selection_start_page_index = 0;
    viewport_info_.selection_start_char_index = 0;
    viewport_info_.selection_end_page_index = 0;
    viewport_info_.selection_end_char_index = 0;
    doc_info_.page_count = 1;
    page_info_.page_index = 0;
    page_info_.text_run_count = 0;
    page_info_.char_count = 0;
    page_info_.bounds = gfx::Rect(0, 0, 1, 1);
  }

 protected:
  chrome_pdf::AccessibilityViewportInfo viewport_info_;
  chrome_pdf::AccessibilityDocInfo doc_info_;
  chrome_pdf::AccessibilityPageInfo page_info_;
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs_;
  std::vector<chrome_pdf::AccessibilityCharInfo> chars_;
  chrome_pdf::AccessibilityPageObjects page_objects_;
};

TEST_F(PdfAccessibilityTreeTest, TestEmptyPDFPage) {
  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  EXPECT_EQ(ax::mojom::Role::kPdfRoot,
            pdf_accessibility_tree.GetRoot()->GetRole());
}

TEST_F(PdfAccessibilityTreeTest, TestAccessibilityDisabledDuringPDFLoad) {
  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  WaitForThreadTasks();

  // Disable accessibility while the PDF is loading, make sure this
  // doesn't crash.
  render_frame->SetAccessibilityModeForTest(ui::AXMode());

  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();
}

TEST_F(PdfAccessibilityTreeTest, TestPdfAccessibilityTreeReload) {
  content::RenderFrame* render_frame = GetMainRenderFrame();
  ASSERT_TRUE(render_frame);
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  // Make the accessibility tree with a portrait page and then remake with a
  // landscape page.
  gfx::RectF page_bounds = gfx::RectF(1, 2);
  for (size_t i = 1; i <= 2; ++i) {
    if (i == 2)
      page_bounds.Transpose();

    page_info_.bounds = gfx::ToEnclosingRect(page_bounds);
    pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
    pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
    pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
    WaitForThreadTasks();

    ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
    ASSERT_TRUE(root_node);
    EXPECT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());

    // There should only be one page node.
    ASSERT_EQ(1u, root_node->children().size());

    ui::AXNode* page_node = root_node->children()[0];
    ASSERT_TRUE(page_node);
    EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
    EXPECT_EQ(page_bounds, page_node->data().relative_bounds.bounds);
  }
}

TEST_F(PdfAccessibilityTreeTest, TestPdfAccessibilityTreeCreation) {
  static const char kTestAltText[] = "Alternate text for image";

  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    chrome_pdf::AccessibilityLinkInfo link;
    link.bounds = gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f);
    link.url = kChromiumTestUrl;
    link.text_range.index = 0;
    link.text_range.count = 1;
    link.index_in_page = 0;
    page_objects_.links.push_back(std::move(link));
  }

  {
    chrome_pdf::AccessibilityImageInfo image;
    image.bounds = gfx::RectF(8.0f, 9.0f, 2.0f, 1.0f);
    image.alt_text = kTestAltText;
    image.text_run_index = 2;
    page_objects_.images.push_back(std::move(image));
  }

  {
    chrome_pdf::AccessibilityImageInfo image;
    image.bounds = gfx::RectF(11.0f, 14.0f, 5.0f, 8.0f);
    image.text_run_index = 2;
    page_objects_.images.push_back(std::move(image));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  /*
   * Expected tree structure
   * Document
   * ++ Region
   * ++++ Paragraph
   * ++++++ Link
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++++ Image
   * ++++++ Image
   */

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  EXPECT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());
  ASSERT_EQ(1u, root_node->children().size());

  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  ASSERT_EQ(2u, page_node->children().size());

  ui::AXNode* paragraph_node = page_node->children()[0];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  EXPECT_TRUE(paragraph_node->GetBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject));
  ASSERT_EQ(1u, paragraph_node->children().size());

  ui::AXNode* link_node = paragraph_node->children()[0];
  ASSERT_TRUE(link_node);
  EXPECT_EQ(kChromiumTestUrl,
            link_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
  EXPECT_EQ(ax::mojom::Role::kLink, link_node->GetRole());
  EXPECT_EQ(gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f),
            link_node->data().relative_bounds.bounds);
  ASSERT_EQ(1u, link_node->children().size());

  paragraph_node = page_node->children()[1];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  EXPECT_TRUE(paragraph_node->GetBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject));
  ASSERT_EQ(3u, paragraph_node->children().size());

  ui::AXNode* static_text_node = paragraph_node->children()[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->children().size());

  ui::AXNode* image_node = paragraph_node->children()[1];
  ASSERT_TRUE(image_node);
  EXPECT_EQ(ax::mojom::Role::kImage, image_node->GetRole());
  EXPECT_EQ(gfx::RectF(8.0f, 9.0f, 2.0f, 1.0f),
            image_node->data().relative_bounds.bounds);
  EXPECT_EQ(kTestAltText,
            image_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  image_node = paragraph_node->children()[2];
  ASSERT_TRUE(image_node);
  EXPECT_EQ(ax::mojom::Role::kImage, image_node->GetRole());
  EXPECT_EQ(gfx::RectF(11.0f, 14.0f, 5.0f, 8.0f),
            image_node->data().relative_bounds.bounds);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION),
            image_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
}

TEST_F(PdfAccessibilityTreeTest, TestOverlappingAnnots) {
  text_runs_.emplace_back(kFirstRunMultiLine);
  text_runs_.emplace_back(kSecondRunMultiLine);
  text_runs_.emplace_back(kThirdRunMultiLine);
  text_runs_.emplace_back(kFourthRunMultiLine);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    chrome_pdf::AccessibilityLinkInfo link;
    link.bounds = gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f);
    link.url = kChromiumTestUrl;
    link.text_range.index = 0;
    link.text_range.count = 3;
    link.index_in_page = 0;
    page_objects_.links.push_back(std::move(link));
  }

  {
    chrome_pdf::AccessibilityLinkInfo link;
    link.bounds = gfx::RectF(1.0f, 2.0f, 5.0f, 6.0f);
    link.url = kChromiumTestUrl;
    link.text_range.index = 1;
    link.text_range.count = 2;
    link.index_in_page = 1;
    page_objects_.links.push_back(std::move(link));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  ASSERT_TRUE(render_frame);
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  /*
   * Expected tree structure
   * Document
   * ++ Region
   * ++++ Paragraph
   * ++++++ Link
   * ++++++ Link
   * ++++++ Static Text
   */

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  EXPECT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());
  ASSERT_EQ(1u, root_node->children().size());

  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  ASSERT_EQ(1u, page_node->children().size());

  ui::AXNode* paragraph_node = page_node->children()[0];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  const std::vector<ui::AXNode*>& child_nodes = paragraph_node->children();
  ASSERT_EQ(3u, child_nodes.size());

  ui::AXNode* link_node = child_nodes[0];
  ASSERT_TRUE(link_node);
  EXPECT_EQ(kChromiumTestUrl,
            link_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
  EXPECT_EQ(ax::mojom::Role::kLink, link_node->GetRole());
  EXPECT_EQ(gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f),
            link_node->data().relative_bounds.bounds);
  ASSERT_EQ(1u, link_node->children().size());

  link_node = child_nodes[1];
  ASSERT_TRUE(link_node);
  EXPECT_EQ(kChromiumTestUrl,
            link_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
  EXPECT_EQ(ax::mojom::Role::kLink, link_node->GetRole());
  EXPECT_EQ(gfx::RectF(1.0f, 2.0f, 5.0f, 6.0f),
            link_node->data().relative_bounds.bounds);
  ASSERT_EQ(1u, link_node->children().size());

  ui::AXNode* static_text_node = child_nodes[2];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->children().size());
}

TEST_F(PdfAccessibilityTreeTest, TestHighlightCreation) {
  constexpr uint32_t kHighlightWhiteColor = MakeARGB(255, 255, 255, 255);
  const char kPopupNoteText[] = "Text Note";

  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    chrome_pdf::AccessibilityHighlightInfo highlight;
    highlight.bounds = gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f);
    highlight.text_range.index = 0;
    highlight.text_range.count = 2;
    highlight.index_in_page = 0;
    highlight.color = kHighlightWhiteColor;
    highlight.note_text = kPopupNoteText;
    page_objects_.highlights.push_back(std::move(highlight));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  /*
   * Expected tree structure
   * Document
   * ++ Region
   * ++++ Paragraph
   * ++++++ Highlight
   * ++++++++ Static Text
   * ++++++++ Note
   * ++++++++++ Static Text
   */

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  EXPECT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());
  ASSERT_EQ(1u, root_node->children().size());

  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  ASSERT_EQ(1u, page_node->children().size());

  ui::AXNode* paragraph_node = page_node->children()[0];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  ASSERT_EQ(1u, paragraph_node->children().size());

  ui::AXNode* highlight_node = paragraph_node->children()[0];
  ASSERT_TRUE(highlight_node);
  EXPECT_EQ(ax::mojom::Role::kPdfActionableHighlight,
            highlight_node->GetRole());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_AX_ROLE_DESCRIPTION_PDF_HIGHLIGHT),
            highlight_node->GetStringAttribute(
                ax::mojom::StringAttribute::kRoleDescription));
  EXPECT_EQ(gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f),
            highlight_node->data().relative_bounds.bounds);
  ASSERT_TRUE(highlight_node->HasIntAttribute(
      ax::mojom::IntAttribute::kBackgroundColor));
  EXPECT_EQ(kHighlightWhiteColor,
            static_cast<uint32_t>(highlight_node->GetIntAttribute(
                ax::mojom::IntAttribute::kBackgroundColor)));
  ASSERT_EQ(2u, highlight_node->children().size());

  ui::AXNode* static_text_node = highlight_node->children()[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(2u, static_text_node->children().size());

  ui::AXNode* popup_note_node = highlight_node->children()[1];
  ASSERT_TRUE(popup_note_node);
  EXPECT_EQ(ax::mojom::Role::kNote, popup_note_node->GetRole());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_AX_ROLE_DESCRIPTION_PDF_POPUP_NOTE),
            popup_note_node->GetStringAttribute(
                ax::mojom::StringAttribute::kRoleDescription));
  EXPECT_EQ(gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f),
            popup_note_node->data().relative_bounds.bounds);
  ASSERT_EQ(1u, popup_note_node->children().size());

  ui::AXNode* static_popup_note_text_node = popup_note_node->children()[0];
  ASSERT_TRUE(static_popup_note_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            static_popup_note_text_node->GetRole());
  EXPECT_EQ(ax::mojom::NameFrom::kContents,
            static_popup_note_text_node->GetNameFrom());
  EXPECT_EQ(kPopupNoteText, static_popup_note_text_node->GetStringAttribute(
                                ax::mojom::StringAttribute::kName));
  EXPECT_EQ(gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f),
            static_popup_note_text_node->data().relative_bounds.bounds);
}

TEST_F(PdfAccessibilityTreeTest, TestTextFieldNodeCreation) {
  // Enable feature flag
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chrome_pdf::features::kAccessiblePDFForm);
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    chrome_pdf::AccessibilityTextFieldInfo text_field;
    text_field.bounds = gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f);
    text_field.index_in_page = 0;
    text_field.text_run_index = 2;
    text_field.name = "Text Box";
    text_field.value = "Text";
    text_field.is_read_only = false;
    text_field.is_required = false;
    text_field.is_password = false;
    page_objects_.form_fields.text_fields.push_back(std::move(text_field));
  }

  {
    chrome_pdf::AccessibilityTextFieldInfo text_field;
    text_field.bounds = gfx::RectF(1.0f, 10.0f, 5.0f, 6.0f);
    text_field.index_in_page = 1;
    text_field.text_run_index = 2;
    text_field.name = "Text Box 2";
    text_field.value = "Text 2";
    text_field.is_read_only = true;
    text_field.is_required = true;
    text_field.is_password = true;
    page_objects_.form_fields.text_fields.push_back(std::move(text_field));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  ASSERT_TRUE(render_frame);
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  /*
   * Expected tree structure
   * Document
   * ++ Region
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++++ Text Field
   * ++++++ Text Field
   */

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  EXPECT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());
  ASSERT_EQ(1u, root_node->children().size());

  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  ASSERT_EQ(2u, page_node->children().size());

  ui::AXNode* paragraph_node = page_node->children()[0];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  ASSERT_EQ(1u, paragraph_node->children().size());

  ui::AXNode* static_text_node = paragraph_node->children()[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->children().size());

  paragraph_node = page_node->children()[1];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  const std::vector<ui::AXNode*>& child_nodes = paragraph_node->children();
  ASSERT_EQ(3u, child_nodes.size());

  static_text_node = child_nodes[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->children().size());

  ui::AXNode* text_field_node = child_nodes[1];
  ASSERT_TRUE(text_field_node);
  EXPECT_EQ(ax::mojom::Role::kTextField, text_field_node->GetRole());
  EXPECT_EQ("Text Box", text_field_node->GetStringAttribute(
                            ax::mojom::StringAttribute::kName));
  EXPECT_EQ("Text", text_field_node->GetStringAttribute(
                        ax::mojom::StringAttribute::kValue));
  EXPECT_FALSE(text_field_node->HasState(ax::mojom::State::kRequired));
  EXPECT_FALSE(text_field_node->HasState(ax::mojom::State::kProtected));
  EXPECT_NE(ax::mojom::Restriction::kReadOnly,
            text_field_node->data().GetRestriction());
  EXPECT_EQ(gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f),
            text_field_node->data().relative_bounds.bounds);
  EXPECT_EQ(0u, text_field_node->children().size());

  text_field_node = child_nodes[2];
  ASSERT_TRUE(text_field_node);
  EXPECT_EQ(ax::mojom::Role::kTextField, text_field_node->GetRole());
  EXPECT_EQ("Text Box 2", text_field_node->GetStringAttribute(
                              ax::mojom::StringAttribute::kName));
  EXPECT_EQ("Text 2", text_field_node->GetStringAttribute(
                          ax::mojom::StringAttribute::kValue));
  EXPECT_TRUE(text_field_node->HasState(ax::mojom::State::kRequired));
  EXPECT_TRUE(text_field_node->HasState(ax::mojom::State::kProtected));
  EXPECT_EQ(ax::mojom::Restriction::kReadOnly,
            text_field_node->data().GetRestriction());
  EXPECT_EQ(gfx::RectF(1.0f, 10.0f, 5.0f, 6.0f),
            text_field_node->data().relative_bounds.bounds);
  EXPECT_EQ(0u, text_field_node->children().size());
}

TEST_F(PdfAccessibilityTreeTest, TestButtonNodeCreation) {
  // Enable feature flag
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chrome_pdf::features::kAccessiblePDFForm);
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    chrome_pdf::AccessibilityButtonInfo check_box;
    check_box.bounds = gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f);
    check_box.index_in_page = 0;
    check_box.text_run_index = 2;
    check_box.name = "Read Only Checkbox";
    check_box.value = "Yes";
    check_box.is_read_only = true;
    check_box.is_checked = true;
    check_box.control_count = 1;
    check_box.control_index = 0;
    check_box.type = chrome_pdf::ButtonType::kCheckBox;
    page_objects_.form_fields.buttons.push_back(std::move(check_box));
  }

  {
    chrome_pdf::AccessibilityButtonInfo radio_button;
    radio_button.bounds = gfx::RectF(1.0f, 2.0f, 5.0f, 6.0f);
    radio_button.index_in_page = 1;
    radio_button.text_run_index = 2;
    radio_button.name = "Radio Button";
    radio_button.value = "value 1";
    radio_button.is_read_only = false;
    radio_button.is_checked = false;
    radio_button.control_count = 2;
    radio_button.control_index = 0;
    radio_button.type = chrome_pdf::ButtonType::kRadioButton;
    page_objects_.form_fields.buttons.push_back(std::move(radio_button));
  }

  {
    chrome_pdf::AccessibilityButtonInfo radio_button;
    radio_button.bounds = gfx::RectF(1.0f, 3.0f, 5.0f, 6.0f);
    radio_button.index_in_page = 2;
    radio_button.text_run_index = 2;
    radio_button.name = "Radio Button";
    radio_button.value = "value 2";
    radio_button.is_read_only = false;
    radio_button.is_checked = true;
    radio_button.control_count = 2;
    radio_button.control_index = 1;
    radio_button.type = chrome_pdf::ButtonType::kRadioButton;
    page_objects_.form_fields.buttons.push_back(std::move(radio_button));
  }

  {
    chrome_pdf::AccessibilityButtonInfo push_button;
    push_button.bounds = gfx::RectF(1.0f, 4.0f, 5.0f, 6.0f);
    push_button.index_in_page = 3;
    push_button.text_run_index = 2;
    push_button.name = "Push Button";
    push_button.is_read_only = false;
    push_button.type = chrome_pdf::ButtonType::kPushButton;
    page_objects_.form_fields.buttons.push_back(std::move(push_button));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  ASSERT_TRUE(render_frame);
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  /*
   * Expected tree structure
   * Document
   * ++ Region
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++++ Check Box
   * ++++++ Radio Button
   * ++++++ Radio Button
   * ++++++ Button
   */

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  EXPECT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());
  ASSERT_EQ(1u, root_node->children().size());

  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  ASSERT_EQ(2u, page_node->children().size());

  ui::AXNode* paragraph_node = page_node->children()[0];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  ASSERT_EQ(1u, paragraph_node->children().size());

  ui::AXNode* static_text_node = paragraph_node->children()[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->children().size());

  paragraph_node = page_node->children()[1];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  const std::vector<ui::AXNode*>& child_nodes = paragraph_node->children();
  ASSERT_EQ(5u, child_nodes.size());

  static_text_node = child_nodes[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->children().size());

  ui::AXNode* check_box_node = child_nodes[1];
  ASSERT_TRUE(check_box_node);
  EXPECT_EQ(ax::mojom::Role::kCheckBox, check_box_node->GetRole());
  EXPECT_EQ("Read Only Checkbox", check_box_node->GetStringAttribute(
                                      ax::mojom::StringAttribute::kName));
  EXPECT_EQ("Yes", check_box_node->GetStringAttribute(
                       ax::mojom::StringAttribute::kValue));
  EXPECT_EQ(ax::mojom::CheckedState::kTrue,
            check_box_node->data().GetCheckedState());
  EXPECT_EQ(1,
            check_box_node->GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(
      1, check_box_node->GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(ax::mojom::Restriction::kReadOnly,
            check_box_node->data().GetRestriction());
  EXPECT_EQ(gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f),
            check_box_node->data().relative_bounds.bounds);
  EXPECT_EQ(0u, check_box_node->children().size());

  ui::AXNode* radio_button_node = child_nodes[2];
  ASSERT_TRUE(radio_button_node);
  EXPECT_EQ(ax::mojom::Role::kRadioButton, radio_button_node->GetRole());
  EXPECT_EQ("Radio Button", radio_button_node->GetStringAttribute(
                                ax::mojom::StringAttribute::kName));
  EXPECT_EQ("value 1", radio_button_node->GetStringAttribute(
                           ax::mojom::StringAttribute::kValue));
  EXPECT_EQ(ax::mojom::CheckedState::kNone,
            radio_button_node->data().GetCheckedState());
  EXPECT_EQ(
      2, radio_button_node->GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(1, radio_button_node->GetIntAttribute(
                   ax::mojom::IntAttribute::kPosInSet));
  EXPECT_NE(ax::mojom::Restriction::kReadOnly,
            radio_button_node->data().GetRestriction());
  EXPECT_EQ(gfx::RectF(1.0f, 2.0f, 5.0f, 6.0f),
            radio_button_node->data().relative_bounds.bounds);
  EXPECT_EQ(0u, radio_button_node->children().size());

  radio_button_node = child_nodes[3];
  ASSERT_TRUE(radio_button_node);
  EXPECT_EQ(ax::mojom::Role::kRadioButton, radio_button_node->GetRole());
  EXPECT_EQ("Radio Button", radio_button_node->GetStringAttribute(
                                ax::mojom::StringAttribute::kName));
  EXPECT_EQ("value 2", radio_button_node->GetStringAttribute(
                           ax::mojom::StringAttribute::kValue));
  EXPECT_EQ(ax::mojom::CheckedState::kTrue,
            radio_button_node->data().GetCheckedState());
  EXPECT_EQ(
      2, radio_button_node->GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(2, radio_button_node->GetIntAttribute(
                   ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            radio_button_node->data().GetRestriction());
  EXPECT_EQ(gfx::RectF(1.0f, 3.0f, 5.0f, 6.0f),
            radio_button_node->data().relative_bounds.bounds);
  EXPECT_EQ(0u, radio_button_node->children().size());

  ui::AXNode* push_button_node = child_nodes[4];
  ASSERT_TRUE(push_button_node);
  EXPECT_EQ(ax::mojom::Role::kButton, push_button_node->GetRole());
  EXPECT_EQ("Push Button", push_button_node->GetStringAttribute(
                               ax::mojom::StringAttribute::kName));
  EXPECT_EQ(gfx::RectF(1.0f, 4.0f, 5.0f, 6.0f),
            push_button_node->data().relative_bounds.bounds);
  EXPECT_EQ(0u, push_button_node->children().size());
}

TEST_F(PdfAccessibilityTreeTest, TestListboxNodeCreation) {
  // Enable feature flag
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chrome_pdf::features::kAccessiblePDFForm);
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  struct ListboxOptionInfo {
    std::string name;
    bool is_selected;
  };

  const ListboxOptionInfo kExpectedOptions[][3] = {
      {{"Alpha", false}, {"Beta", true}, {"Gamma", true}},
      {{"Foo", false}, {"Bar", true}, {"Qux", false}}};

  const gfx::RectF kExpectedBounds[] = {{1.0f, 1.0f, 5.0f, 6.0f},
                                        {1.0f, 10.0f, 5.0f, 6.0f}};

  {
    chrome_pdf::AccessibilityChoiceFieldInfo choice_field;
    choice_field.bounds = gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f);
    choice_field.index_in_page = 0;
    choice_field.text_run_index = 2;
    choice_field.type = chrome_pdf::ChoiceFieldType::kListBox;
    choice_field.name = "List Box";
    choice_field.is_read_only = false;
    choice_field.is_multi_select = true;
    choice_field.has_editable_text_box = false;
    for (const ListboxOptionInfo& expected_option : kExpectedOptions[0]) {
      chrome_pdf::AccessibilityChoiceFieldOptionInfo choice_field_option;
      choice_field_option.name = expected_option.name;
      choice_field_option.is_selected = expected_option.is_selected;
      choice_field.options.push_back(std::move(choice_field_option));
    }
    page_objects_.form_fields.choice_fields.push_back(std::move(choice_field));
  }

  {
    chrome_pdf::AccessibilityChoiceFieldInfo choice_field;
    choice_field.bounds = gfx::RectF(1.0f, 10.0f, 5.0f, 6.0f);
    choice_field.index_in_page = 1;
    choice_field.text_run_index = 2;
    choice_field.type = chrome_pdf::ChoiceFieldType::kListBox;
    choice_field.name = "Read Only List Box";
    choice_field.is_read_only = true;
    choice_field.is_multi_select = false;
    choice_field.has_editable_text_box = false;
    for (const ListboxOptionInfo& expected_option : kExpectedOptions[1]) {
      chrome_pdf::AccessibilityChoiceFieldOptionInfo choice_field_option;
      choice_field_option.name = expected_option.name;
      choice_field_option.is_selected = expected_option.is_selected;
      choice_field.options.push_back(std::move(choice_field_option));
    }
    page_objects_.form_fields.choice_fields.push_back(std::move(choice_field));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  ASSERT_TRUE(render_frame);
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  /*
   * Expected tree structure
   * Document
   * ++ Region
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++++ Listbox
   * ++++++++ Listbox Option
   * ++++++++ Listbox Option
   * ++++++++ Listbox Option
   * ++++++ Listbox
   * ++++++++ Listbox Option
   * ++++++++ Listbox Option
   * ++++++++ Listbox Option
   */

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  EXPECT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());
  ASSERT_EQ(1u, root_node->children().size());

  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  ASSERT_EQ(2u, page_node->children().size());

  ui::AXNode* paragraph_node = page_node->children()[0];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  ASSERT_EQ(1u, paragraph_node->children().size());

  ui::AXNode* static_text_node = paragraph_node->children()[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->children().size());

  paragraph_node = page_node->children()[1];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  const std::vector<ui::AXNode*>& child_nodes = paragraph_node->children();
  ASSERT_EQ(3u, child_nodes.size());

  static_text_node = child_nodes[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->children().size());

  {
    ui::AXNode* listbox_node = child_nodes[1];
    ASSERT_TRUE(listbox_node);
    EXPECT_EQ(ax::mojom::Role::kListBox, listbox_node->GetRole());
    EXPECT_NE(ax::mojom::Restriction::kReadOnly,
              listbox_node->data().GetRestriction());
    EXPECT_EQ("List Box", listbox_node->GetStringAttribute(
                              ax::mojom::StringAttribute::kName));
    EXPECT_TRUE(listbox_node->HasState(ax::mojom::State::kMultiselectable));
    EXPECT_TRUE(listbox_node->HasState(ax::mojom::State::kFocusable));
    EXPECT_EQ(kExpectedBounds[0], listbox_node->data().relative_bounds.bounds);
    ASSERT_EQ(std::size(kExpectedOptions[0]), listbox_node->children().size());
    const std::vector<ui::AXNode*>& listbox_child_nodes =
        listbox_node->children();
    for (size_t i = 0; i < listbox_child_nodes.size(); i++) {
      EXPECT_EQ(ax::mojom::Role::kListBoxOption,
                listbox_child_nodes[i]->GetRole());
      EXPECT_NE(ax::mojom::Restriction::kReadOnly,
                listbox_child_nodes[i]->data().GetRestriction());
      EXPECT_EQ(kExpectedOptions[0][i].name,
                listbox_child_nodes[i]->GetStringAttribute(
                    ax::mojom::StringAttribute::kName));
      EXPECT_EQ(kExpectedOptions[0][i].is_selected,
                listbox_child_nodes[i]->GetBoolAttribute(
                    ax::mojom::BoolAttribute::kSelected));
      EXPECT_TRUE(
          listbox_child_nodes[i]->HasState(ax::mojom::State::kFocusable));
      EXPECT_EQ(kExpectedBounds[0],
                listbox_child_nodes[i]->data().relative_bounds.bounds);
    }
  }

  {
    ui::AXNode* listbox_node = child_nodes[2];
    ASSERT_TRUE(listbox_node);
    EXPECT_EQ(ax::mojom::Role::kListBox, listbox_node->GetRole());
    EXPECT_EQ(ax::mojom::Restriction::kReadOnly,
              listbox_node->data().GetRestriction());
    EXPECT_EQ("Read Only List Box", listbox_node->GetStringAttribute(
                                        ax::mojom::StringAttribute::kName));
    EXPECT_FALSE(listbox_node->HasState(ax::mojom::State::kMultiselectable));
    EXPECT_TRUE(listbox_node->HasState(ax::mojom::State::kFocusable));
    EXPECT_EQ(kExpectedBounds[1], listbox_node->data().relative_bounds.bounds);
    ASSERT_EQ(std::size(kExpectedOptions[1]), listbox_node->children().size());
    const std::vector<ui::AXNode*>& listbox_child_nodes =
        listbox_node->children();
    for (size_t i = 0; i < listbox_child_nodes.size(); i++) {
      EXPECT_EQ(ax::mojom::Role::kListBoxOption,
                listbox_child_nodes[i]->GetRole());
      EXPECT_EQ(ax::mojom::Restriction::kReadOnly,
                listbox_child_nodes[i]->data().GetRestriction());
      EXPECT_EQ(kExpectedOptions[1][i].name,
                listbox_child_nodes[i]->GetStringAttribute(
                    ax::mojom::StringAttribute::kName));
      EXPECT_EQ(kExpectedOptions[1][i].is_selected,
                listbox_child_nodes[i]->GetBoolAttribute(
                    ax::mojom::BoolAttribute::kSelected));
      EXPECT_TRUE(
          listbox_child_nodes[i]->HasState(ax::mojom::State::kFocusable));
      EXPECT_EQ(kExpectedBounds[1],
                listbox_child_nodes[i]->data().relative_bounds.bounds);
    }
  }
}

TEST_F(PdfAccessibilityTreeTest, TestComboboxNodeCreation) {
  // Enable feature flag
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chrome_pdf::features::kAccessiblePDFForm);
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  struct ComboboxOptionInfo {
    std::string name;
    bool is_selected;
  };

  const ComboboxOptionInfo kExpectedOptions[][3] = {
      {{"Albania", false}, {"Belgium", true}, {"Croatia", true}},
      {{"Apple", false}, {"Banana", true}, {"Cherry", false}}};

  const gfx::RectF kExpectedBounds[] = {{1.0f, 1.0f, 5.0f, 6.0f},
                                        {1.0f, 10.0f, 5.0f, 6.0f}};

  {
    chrome_pdf::AccessibilityChoiceFieldInfo choice_field;
    choice_field.bounds = gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f);
    choice_field.index_in_page = 0;
    choice_field.text_run_index = 2;
    choice_field.type = chrome_pdf::ChoiceFieldType::kComboBox;
    choice_field.name = "Editable Combo Box";
    choice_field.is_read_only = false;
    choice_field.is_multi_select = true;
    choice_field.has_editable_text_box = true;
    for (const ComboboxOptionInfo& expected_option : kExpectedOptions[0]) {
      chrome_pdf::AccessibilityChoiceFieldOptionInfo choice_field_option;
      choice_field_option.name = expected_option.name;
      choice_field_option.is_selected = expected_option.is_selected;
      choice_field.options.push_back(std::move(choice_field_option));
    }
    page_objects_.form_fields.choice_fields.push_back(std::move(choice_field));
  }

  {
    chrome_pdf::AccessibilityChoiceFieldInfo choice_field;
    choice_field.bounds = gfx::RectF(1.0f, 10.0f, 5.0f, 6.0f);
    choice_field.index_in_page = 1;
    choice_field.text_run_index = 2;
    choice_field.type = chrome_pdf::ChoiceFieldType::kComboBox;
    choice_field.name = "Read Only Combo Box";
    choice_field.is_read_only = true;
    choice_field.is_multi_select = false;
    choice_field.has_editable_text_box = false;
    for (const ComboboxOptionInfo& expected_option : kExpectedOptions[1]) {
      chrome_pdf::AccessibilityChoiceFieldOptionInfo choice_field_option;
      choice_field_option.name = expected_option.name;
      choice_field_option.is_selected = expected_option.is_selected;
      choice_field.options.push_back(std::move(choice_field_option));
    }
    page_objects_.form_fields.choice_fields.push_back(std::move(choice_field));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  ASSERT_TRUE(render_frame);
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  /*
   * Expected tree structure
   * Document
   * ++ Region
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++++ Combobox Grouping
   * ++++++++ Text Field With Combobox
   * ++++++++ Listbox
   * ++++++++++ Listbox Option
   * ++++++++++ Listbox Option
   * ++++++++++ Listbox Option
   * ++++++ Combobox Grouping
   * ++++++++ Combobox Menu Button
   * ++++++++ Listbox
   * ++++++++++ Listbox Option
   * ++++++++++ Listbox Option
   * ++++++++++ Listbox Option
   */

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  EXPECT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());
  ASSERT_EQ(1u, root_node->children().size());

  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  ASSERT_EQ(2u, page_node->children().size());

  ui::AXNode* paragraph_node = page_node->children()[0];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  ASSERT_EQ(1u, paragraph_node->children().size());

  ui::AXNode* static_text_node = paragraph_node->children()[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->children().size());

  paragraph_node = page_node->children()[1];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  const std::vector<ui::AXNode*>& child_nodes = paragraph_node->children();
  ASSERT_EQ(3u, child_nodes.size());

  static_text_node = child_nodes[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->children().size());

  {
    ui::AXNode* combobox_node = child_nodes[1];
    ASSERT_TRUE(combobox_node);
    EXPECT_EQ(ax::mojom::Role::kComboBoxGrouping, combobox_node->GetRole());
    EXPECT_NE(ax::mojom::Restriction::kReadOnly,
              combobox_node->data().GetRestriction());
    EXPECT_TRUE(combobox_node->HasState(ax::mojom::State::kFocusable));
    EXPECT_EQ(kExpectedBounds[0], combobox_node->data().relative_bounds.bounds);
    ASSERT_EQ(2u, combobox_node->children().size());
    const std::vector<ui::AXNode*>& combobox_child_nodes =
        combobox_node->children();

    ui::AXNode* combobox_input_node = combobox_child_nodes[0];
    EXPECT_EQ(ax::mojom::Role::kTextFieldWithComboBox,
              combobox_input_node->GetRole());
    EXPECT_NE(ax::mojom::Restriction::kReadOnly,
              combobox_input_node->data().GetRestriction());
    EXPECT_EQ("Editable Combo Box", combobox_input_node->GetStringAttribute(
                                        ax::mojom::StringAttribute::kName));
    EXPECT_EQ("Belgium", combobox_input_node->GetStringAttribute(
                             ax::mojom::StringAttribute::kValue));
    EXPECT_TRUE(combobox_input_node->HasState(ax::mojom::State::kFocusable));
    EXPECT_EQ(kExpectedBounds[0],
              combobox_input_node->data().relative_bounds.bounds);

    ui::AXNode* combobox_popup_node = combobox_child_nodes[1];
    EXPECT_EQ(ax::mojom::Role::kListBox, combobox_popup_node->GetRole());
    EXPECT_NE(ax::mojom::Restriction::kReadOnly,
              combobox_popup_node->data().GetRestriction());
    EXPECT_TRUE(
        combobox_popup_node->HasState(ax::mojom::State::kMultiselectable));
    EXPECT_EQ(kExpectedBounds[0],
              combobox_popup_node->data().relative_bounds.bounds);
    ASSERT_EQ(std::size(kExpectedOptions[0]),
              combobox_popup_node->children().size());
    const std::vector<ui::AXNode*>& popup_child_nodes =
        combobox_popup_node->children();
    for (size_t i = 0; i < popup_child_nodes.size(); i++) {
      EXPECT_EQ(ax::mojom::Role::kListBoxOption,
                popup_child_nodes[i]->GetRole());
      EXPECT_NE(ax::mojom::Restriction::kReadOnly,
                popup_child_nodes[i]->data().GetRestriction());
      EXPECT_EQ(kExpectedOptions[0][i].name,
                popup_child_nodes[i]->GetStringAttribute(
                    ax::mojom::StringAttribute::kName));
      EXPECT_EQ(kExpectedOptions[0][i].is_selected,
                popup_child_nodes[i]->GetBoolAttribute(
                    ax::mojom::BoolAttribute::kSelected));
      EXPECT_TRUE(popup_child_nodes[i]->HasState(ax::mojom::State::kFocusable));
      EXPECT_EQ(kExpectedBounds[0],
                popup_child_nodes[i]->data().relative_bounds.bounds);
    }
    EXPECT_EQ(popup_child_nodes[1]->data().id,
              combobox_input_node->GetIntAttribute(
                  ax::mojom::IntAttribute::kActivedescendantId));
    const auto& controls_ids = combobox_input_node->GetIntListAttribute(
        ax::mojom::IntListAttribute::kControlsIds);
    ASSERT_EQ(1u, controls_ids.size());
    EXPECT_EQ(controls_ids[0], combobox_popup_node->data().id);
  }

  {
    ui::AXNode* combobox_node = child_nodes[2];
    ASSERT_TRUE(combobox_node);
    EXPECT_EQ(ax::mojom::Role::kComboBoxGrouping, combobox_node->GetRole());
    EXPECT_EQ(ax::mojom::Restriction::kReadOnly,
              combobox_node->data().GetRestriction());
    EXPECT_TRUE(combobox_node->HasState(ax::mojom::State::kFocusable));
    EXPECT_EQ(kExpectedBounds[1], combobox_node->data().relative_bounds.bounds);
    ASSERT_EQ(2u, combobox_node->children().size());
    const std::vector<ui::AXNode*>& combobox_child_nodes =
        combobox_node->children();

    ui::AXNode* combobox_input_node = combobox_child_nodes[0];
    EXPECT_EQ(ax::mojom::Role::kComboBoxMenuButton,
              combobox_input_node->GetRole());
    EXPECT_EQ(ax::mojom::Restriction::kReadOnly,
              combobox_input_node->data().GetRestriction());
    EXPECT_EQ("Read Only Combo Box", combobox_input_node->GetStringAttribute(
                                         ax::mojom::StringAttribute::kName));
    EXPECT_EQ("Banana", combobox_input_node->GetStringAttribute(
                            ax::mojom::StringAttribute::kValue));
    EXPECT_TRUE(combobox_input_node->HasState(ax::mojom::State::kFocusable));
    EXPECT_EQ(kExpectedBounds[1],
              combobox_input_node->data().relative_bounds.bounds);

    ui::AXNode* combobox_popup_node = combobox_child_nodes[1];
    EXPECT_EQ(ax::mojom::Role::kListBox, combobox_popup_node->GetRole());
    EXPECT_EQ(ax::mojom::Restriction::kReadOnly,
              combobox_popup_node->data().GetRestriction());
    EXPECT_EQ(kExpectedBounds[1],
              combobox_popup_node->data().relative_bounds.bounds);
    ASSERT_EQ(std::size(kExpectedOptions[1]),
              combobox_popup_node->children().size());
    const std::vector<ui::AXNode*>& popup_child_nodes =
        combobox_popup_node->children();
    for (size_t i = 0; i < popup_child_nodes.size(); i++) {
      EXPECT_EQ(ax::mojom::Role::kListBoxOption,
                popup_child_nodes[i]->GetRole());
      EXPECT_EQ(ax::mojom::Restriction::kReadOnly,
                popup_child_nodes[i]->data().GetRestriction());
      EXPECT_EQ(kExpectedOptions[1][i].name,
                popup_child_nodes[i]->GetStringAttribute(
                    ax::mojom::StringAttribute::kName));
      EXPECT_EQ(kExpectedOptions[1][i].is_selected,
                popup_child_nodes[i]->GetBoolAttribute(
                    ax::mojom::BoolAttribute::kSelected));
      EXPECT_TRUE(popup_child_nodes[i]->HasState(ax::mojom::State::kFocusable));
      EXPECT_EQ(kExpectedBounds[1],
                popup_child_nodes[i]->data().relative_bounds.bounds);
    }
    EXPECT_EQ(popup_child_nodes[1]->data().id,
              combobox_input_node->GetIntAttribute(
                  ax::mojom::IntAttribute::kActivedescendantId));
    const auto& controls_ids = combobox_input_node->GetIntListAttribute(
        ax::mojom::IntListAttribute::kControlsIds);
    ASSERT_EQ(1u, controls_ids.size());
    EXPECT_EQ(controls_ids[0], combobox_popup_node->data().id);
  }
}

TEST_F(PdfAccessibilityTreeTest, TestPreviousNextOnLine) {
  text_runs_.emplace_back(kFirstRunMultiLine);
  text_runs_.emplace_back(kSecondRunMultiLine);
  text_runs_.emplace_back(kThirdRunMultiLine);
  text_runs_.emplace_back(kFourthRunMultiLine);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    chrome_pdf::AccessibilityLinkInfo link;
    link.bounds = gfx::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    link.url = kChromiumTestUrl;
    link.text_range.index = 2;
    link.text_range.count = 2;
    link.index_in_page = 0;
    page_objects_.links.push_back(std::move(link));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  /*
   * Expected tree structure
   * Document
   * ++ Region
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++++++ Inline Text Box
   * ++++++++ Inline Text Box
   * ++++++ Link
   * ++++++++ Static Text
   * ++++++++++ Inline Text Box
   * ++++++++++ Inline Text Box
   */

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  EXPECT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());
  ASSERT_EQ(1u, root_node->children().size());

  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  ASSERT_EQ(1u, page_node->children().size());

  ui::AXNode* paragraph_node = page_node->children()[0];
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  EXPECT_TRUE(paragraph_node->GetBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject));
  ASSERT_EQ(2u, paragraph_node->children().size());

  ui::AXNode* static_text_node = paragraph_node->children()[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  EXPECT_EQ(ax::mojom::NameFrom::kContents, static_text_node->GetNameFrom());
  ASSERT_EQ(2u, static_text_node->children().size());

  ui::AXNode* previous_inline_node = static_text_node->children()[0];
  ASSERT_TRUE(previous_inline_node);
  EXPECT_EQ(ax::mojom::Role::kInlineTextBox, previous_inline_node->GetRole());
  EXPECT_EQ(ax::mojom::NameFrom::kContents,
            previous_inline_node->GetNameFrom());
  ASSERT_FALSE(previous_inline_node->HasIntAttribute(
      ax::mojom::IntAttribute::kPreviousOnLineId));

  ui::AXNode* next_inline_node = static_text_node->children()[1];
  ASSERT_TRUE(next_inline_node);
  EXPECT_EQ(ax::mojom::Role::kInlineTextBox, next_inline_node->GetRole());
  EXPECT_EQ(ax::mojom::NameFrom::kContents, next_inline_node->GetNameFrom());
  ASSERT_TRUE(next_inline_node->HasIntAttribute(
      ax::mojom::IntAttribute::kNextOnLineId));

  ASSERT_EQ(next_inline_node->data().id,
            previous_inline_node->GetIntAttribute(
                ax::mojom::IntAttribute::kNextOnLineId));
  ASSERT_EQ(previous_inline_node->data().id,
            next_inline_node->GetIntAttribute(
                ax::mojom::IntAttribute::kPreviousOnLineId));

  ui::AXNode* link_node = paragraph_node->children()[1];
  ASSERT_TRUE(link_node);
  EXPECT_EQ(kChromiumTestUrl,
            link_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
  EXPECT_EQ(ax::mojom::Role::kLink, link_node->GetRole());
  ASSERT_EQ(1u, link_node->children().size());

  static_text_node = link_node->children()[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  EXPECT_EQ(ax::mojom::NameFrom::kContents, static_text_node->GetNameFrom());
  ASSERT_EQ(2u, static_text_node->children().size());

  previous_inline_node = static_text_node->children()[0];
  ASSERT_TRUE(previous_inline_node);
  EXPECT_EQ(ax::mojom::Role::kInlineTextBox, previous_inline_node->GetRole());
  EXPECT_EQ(ax::mojom::NameFrom::kContents,
            previous_inline_node->GetNameFrom());
  ASSERT_TRUE(previous_inline_node->HasIntAttribute(
      ax::mojom::IntAttribute::kPreviousOnLineId));
  // Test that text and link on the same line are connected.
  ASSERT_EQ(next_inline_node->data().id,
            previous_inline_node->GetIntAttribute(
                ax::mojom::IntAttribute::kPreviousOnLineId));

  next_inline_node = static_text_node->children()[1];
  ASSERT_TRUE(next_inline_node);
  EXPECT_EQ(ax::mojom::Role::kInlineTextBox, next_inline_node->GetRole());
  EXPECT_EQ(ax::mojom::NameFrom::kContents, next_inline_node->GetNameFrom());
  ASSERT_FALSE(next_inline_node->HasIntAttribute(
      ax::mojom::IntAttribute::kNextOnLineId));

  ASSERT_EQ(next_inline_node->data().id,
            previous_inline_node->GetIntAttribute(
                ax::mojom::IntAttribute::kNextOnLineId));
  ASSERT_EQ(previous_inline_node->data().id,
            next_inline_node->GetIntAttribute(
                ax::mojom::IntAttribute::kPreviousOnLineId));
}

TEST_F(PdfAccessibilityTreeTest, TextRunsAndCharsMismatch) {
  // `chars_` and `text_runs_` span over the same page text. They should denote
  // the same page text size, but `text_runs_` is incorrect and only denotes 1
  // of 2 text runs.
  text_runs_.emplace_back(kFirstTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_FALSE(pdf_accessibility_tree.GetRoot());
}

TEST_F(PdfAccessibilityTreeTest, UnsortedLinkVector) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    // Add first link in the vector.
    chrome_pdf::AccessibilityLinkInfo link;
    link.bounds = gfx::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    link.text_range.index = 2;
    link.text_range.count = 0;
    page_objects_.links.push_back(std::move(link));
  }

  {
    // Add second link in the vector.
    chrome_pdf::AccessibilityLinkInfo link;
    link.bounds = gfx::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    link.text_range.index = 0;
    link.text_range.count = 1;
    page_objects_.links.push_back(std::move(link));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_FALSE(pdf_accessibility_tree.GetRoot());
}

TEST_F(PdfAccessibilityTreeTest, OutOfBoundLink) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    chrome_pdf::AccessibilityLinkInfo link;
    link.bounds = gfx::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    link.text_range.index = 3;
    link.index_in_page = 0;
    link.text_range.count = 0;
    page_objects_.links.push_back(std::move(link));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_FALSE(pdf_accessibility_tree.GetRoot());
}

TEST_F(PdfAccessibilityTreeTest, UnsortedImageVector) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    // Add first image to the vector.
    chrome_pdf::AccessibilityImageInfo image;
    image.bounds = gfx::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    image.text_run_index = 1;
    page_objects_.images.push_back(std::move(image));
  }

  {
    // Add second image to the vector.
    chrome_pdf::AccessibilityImageInfo image;
    image.bounds = gfx::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    image.text_run_index = 0;
    page_objects_.images.push_back(std::move(image));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_FALSE(pdf_accessibility_tree.GetRoot());
}

TEST_F(PdfAccessibilityTreeTest, OutOfBoundImage) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    chrome_pdf::AccessibilityImageInfo image;
    image.bounds = gfx::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    image.text_run_index = 3;
    page_objects_.images.push_back(std::move(image));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_FALSE(pdf_accessibility_tree.GetRoot());
}

TEST_F(PdfAccessibilityTreeTest, UnsortedHighlightVector) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    // Add first highlight in the vector.
    chrome_pdf::AccessibilityHighlightInfo highlight;
    highlight.bounds = gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f);
    highlight.text_range.index = 2;
    highlight.text_range.count = 0;
    highlight.index_in_page = 0;
    page_objects_.highlights.push_back(std::move(highlight));
  }

  {
    // Add second highlight in the vector.
    chrome_pdf::AccessibilityHighlightInfo highlight;
    highlight.bounds = gfx::RectF(2.0f, 2.0f, 1.0f, 1.0f);
    highlight.text_range.index = 0;
    highlight.text_range.count = 1;
    highlight.index_in_page = 1;
    page_objects_.highlights.push_back(std::move(highlight));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  ASSERT_TRUE(render_frame);
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_FALSE(pdf_accessibility_tree.GetRoot());
}

TEST_F(PdfAccessibilityTreeTest, OutOfBoundHighlight) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    chrome_pdf::AccessibilityHighlightInfo highlight;
    highlight.bounds = gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f);
    highlight.text_range.index = 3;
    highlight.text_range.count = 0;
    highlight.index_in_page = 0;
    page_objects_.highlights.push_back(std::move(highlight));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  ASSERT_TRUE(render_frame);
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_FALSE(pdf_accessibility_tree.GetRoot());
}

TEST_F(PdfAccessibilityTreeTest, TestActionDataConversion) {
  // This test verifies the AXActionData conversion to
  // `chrome_pdf::AccessibilityActionData`.
  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      pdf_accessibility_tree.CreateActionTarget(*root_node);
  ASSERT_TRUE(pdf_action_target);
  EXPECT_EQ(ui::AXActionTarget::Type::kPdf, pdf_action_target->GetType());
  EXPECT_TRUE(pdf_action_target->ScrollToMakeVisibleWithSubFocus(
      gfx::Rect(0, 0, 50, 50), ax::mojom::ScrollAlignment::kScrollAlignmentLeft,
      ax::mojom::ScrollAlignment::kScrollAlignmentTop,
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible));
  chrome_pdf::AccessibilityActionData action_data =
      action_handler.received_action_data();
  EXPECT_EQ(chrome_pdf::AccessibilityAction::kScrollToMakeVisible,
            action_data.action);
  EXPECT_EQ(chrome_pdf::AccessibilityScrollAlignment::kLeft,
            action_data.horizontal_scroll_alignment);
  EXPECT_EQ(chrome_pdf::AccessibilityScrollAlignment::kTop,
            action_data.vertical_scroll_alignment);

  EXPECT_TRUE(pdf_action_target->ScrollToMakeVisibleWithSubFocus(
      gfx::Rect(0, 0, 50, 50),
      ax::mojom::ScrollAlignment::kScrollAlignmentRight,
      ax::mojom::ScrollAlignment::kScrollAlignmentTop,
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible));
  action_data = action_handler.received_action_data();
  EXPECT_EQ(chrome_pdf::AccessibilityScrollAlignment::kRight,
            action_data.horizontal_scroll_alignment);

  EXPECT_TRUE(pdf_action_target->ScrollToMakeVisibleWithSubFocus(
      gfx::Rect(0, 0, 50, 50),
      ax::mojom::ScrollAlignment::kScrollAlignmentBottom,
      ax::mojom::ScrollAlignment::kScrollAlignmentBottom,
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible));
  action_data = action_handler.received_action_data();
  EXPECT_EQ(chrome_pdf::AccessibilityScrollAlignment::kBottom,
            action_data.horizontal_scroll_alignment);

  EXPECT_TRUE(pdf_action_target->ScrollToMakeVisibleWithSubFocus(
      gfx::Rect(0, 0, 50, 50),
      ax::mojom::ScrollAlignment::kScrollAlignmentCenter,
      ax::mojom::ScrollAlignment::kScrollAlignmentClosestEdge,
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible));
  action_data = action_handler.received_action_data();
  EXPECT_EQ(chrome_pdf::AccessibilityScrollAlignment::kCenter,
            action_data.horizontal_scroll_alignment);
  EXPECT_EQ(chrome_pdf::AccessibilityScrollAlignment::kClosestToEdge,
            action_data.vertical_scroll_alignment);
  EXPECT_EQ(gfx::Rect({0, 0}, {1, 1}), action_data.target_rect);
}

TEST_F(PdfAccessibilityTreeTest, TestScrollToGlobalPointDataConversion) {
  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      pdf_accessibility_tree.CreateActionTarget(*root_node);
  ASSERT_TRUE(pdf_action_target);
  EXPECT_EQ(ui::AXActionTarget::Type::kPdf, pdf_action_target->GetType());
  {
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kScrollToPoint;
    action_data.target_point = gfx::Point(50, 50);
    EXPECT_TRUE(pdf_action_target->PerformAction(action_data));
  }

  chrome_pdf::AccessibilityActionData action_data =
      action_handler.received_action_data();
  EXPECT_EQ(chrome_pdf::AccessibilityAction::kScrollToGlobalPoint,
            action_data.action);
  EXPECT_EQ(gfx::Point(50, 50), action_data.target_point);
  EXPECT_EQ(gfx::Rect({0, 0}, {1, 1}), action_data.target_rect);
}

TEST_F(PdfAccessibilityTreeTest, TestClickActionDataConversion) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  {
    chrome_pdf::AccessibilityLinkInfo link;
    link.url = kChromiumTestUrl;
    link.text_range.index = 0;
    link.text_range.count = 1;
    link.bounds = {{0, 0}, {10, 10}};
    link.index_in_page = 0;
    page_objects_.links.push_back(std::move(link));
  }

  {
    chrome_pdf::AccessibilityLinkInfo link;
    link.url = kChromiumTestUrl;
    link.text_range.index = 1;
    link.text_range.count = 1;
    link.bounds = {{10, 10}, {10, 10}};
    link.index_in_page = 1;
    page_objects_.links.push_back(std::move(link));
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  const std::vector<ui::AXNode*>& page_nodes = root_node->children();
  ASSERT_EQ(1u, page_nodes.size());
  const std::vector<ui::AXNode*>& para_nodes = page_nodes[0]->children();
  ASSERT_EQ(2u, para_nodes.size());
  const std::vector<ui::AXNode*>& link_nodes = para_nodes[1]->children();
  ASSERT_EQ(1u, link_nodes.size());

  const ui::AXNode* link_node = link_nodes[0];
  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      pdf_accessibility_tree.CreateActionTarget(*link_node);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf, pdf_action_target->GetType());
  {
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kDoDefault;
    pdf_action_target->PerformAction(action_data);
  }
  chrome_pdf::AccessibilityActionData pdf_action_data =
      action_handler.received_action_data();

  EXPECT_EQ(chrome_pdf::AccessibilityAction::kDoDefaultAction,
            pdf_action_data.action);
  EXPECT_EQ(chrome_pdf::AccessibilityScrollAlignment::kNone,
            pdf_action_data.horizontal_scroll_alignment);
  EXPECT_EQ(chrome_pdf::AccessibilityScrollAlignment::kNone,
            pdf_action_data.vertical_scroll_alignment);
  EXPECT_EQ(0u, pdf_action_data.page_index);
  EXPECT_EQ(chrome_pdf::AccessibilityAnnotationType::kLink,
            pdf_action_data.annotation_type);
  EXPECT_EQ(1u, pdf_action_data.annotation_index);
  EXPECT_EQ(gfx::Rect({0, 0}, {0, 0}), pdf_action_data.target_rect);
}

TEST_F(PdfAccessibilityTreeTest, TestEmptyPdfAxActions) {
  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      pdf_accessibility_tree.CreateActionTarget(*root_node);
  ASSERT_TRUE(pdf_action_target);
  gfx::Rect rect = pdf_action_target->GetRelativeBounds();
  EXPECT_TRUE(rect.origin().IsOrigin());
  EXPECT_TRUE(rect.IsEmpty());

  gfx::Point point = pdf_action_target->GetScrollOffset();
  EXPECT_EQ(point.x(), 0);
  EXPECT_EQ(point.y(), 0);

  point = pdf_action_target->MinimumScrollOffset();
  EXPECT_EQ(point.x(), 0);
  EXPECT_EQ(point.y(), 0);

  point = pdf_action_target->MaximumScrollOffset();
  EXPECT_EQ(point.x(), 0);
  EXPECT_EQ(point.y(), 0);

  EXPECT_FALSE(pdf_action_target->SetSelected(true));
  EXPECT_FALSE(pdf_action_target->SetSelected(false));
  EXPECT_FALSE(pdf_action_target->ScrollToMakeVisible());
}

TEST_F(PdfAccessibilityTreeTest, TestZoomAndScaleChanges) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  viewport_info_.zoom = 1.0;
  viewport_info_.scale = 1.0;
  viewport_info_.scroll = gfx::Point(0, -56);
  viewport_info_.offset = gfx::Point(57, 0);

  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  WaitForThreadTasks();

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  ASSERT_EQ(1u, root_node->children().size());
  ui::AXNode* page_node = root_node->children()[0];
  ASSERT_TRUE(page_node);
  ASSERT_EQ(2u, page_node->children().size());
  ui::AXNode* para_node = page_node->children()[0];
  ASSERT_TRUE(para_node);
  gfx::RectF rect = para_node->data().relative_bounds.bounds;
  CompareRect({{26.0f, 189.0f}, {84.0f, 13.0f}}, rect);
  gfx::Transform* transform = root_node->data().relative_bounds.transform.get();
  ASSERT_TRUE(transform);
  CompareRect({{83.0f, 245.0f}, {84.0f, 13.0f}}, transform->MapRect(rect));

  float new_device_scale = 1.5f;
  float new_zoom = 1.5f;
  viewport_info_.zoom = new_zoom;
  viewport_info_.scale = new_device_scale;
  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  WaitForThreadTasks();

  rect = para_node->data().relative_bounds.bounds;
  transform = root_node->data().relative_bounds.transform.get();
  ASSERT_TRUE(transform);
  CompareRect({{186.75f, 509.25f}, {189.00f, 29.25f}},
              transform->MapRect(rect));
}

TEST_F(PdfAccessibilityTreeTest, TestSelectionActionDataConversion) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));
  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();
  content::RenderFrame* render_frame = GetMainRenderFrame();
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());
  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);
  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);
  const std::vector<ui::AXNode*>& page_nodes = root_node->children();
  ASSERT_EQ(1u, page_nodes.size());
  ASSERT_TRUE(page_nodes[0]);
  const std::vector<ui::AXNode*>& para_nodes = page_nodes[0]->children();
  ASSERT_EQ(2u, para_nodes.size());
  ASSERT_TRUE(para_nodes[0]);
  const std::vector<ui::AXNode*>& static_text_nodes1 =
      para_nodes[0]->children();
  ASSERT_EQ(1u, static_text_nodes1.size());
  ASSERT_TRUE(static_text_nodes1[0]);
  const std::vector<ui::AXNode*>& inline_text_nodes1 =
      static_text_nodes1[0]->children();
  ASSERT_TRUE(inline_text_nodes1[0]);
  ASSERT_EQ(1u, inline_text_nodes1.size());
  ASSERT_TRUE(para_nodes[1]);
  const std::vector<ui::AXNode*>& static_text_nodes2 =
      para_nodes[1]->children();
  ASSERT_EQ(1u, static_text_nodes2.size());
  ASSERT_TRUE(static_text_nodes2[0]);
  const std::vector<ui::AXNode*>& inline_text_nodes2 =
      static_text_nodes2[0]->children();
  ASSERT_TRUE(inline_text_nodes2[0]);
  ASSERT_EQ(1u, inline_text_nodes2.size());

  std::unique_ptr<ui::AXActionTarget> pdf_anchor_action_target =
      pdf_accessibility_tree.CreateActionTarget(*inline_text_nodes1[0]);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf,
            pdf_anchor_action_target->GetType());
  std::unique_ptr<ui::AXActionTarget> pdf_focus_action_target =
      pdf_accessibility_tree.CreateActionTarget(*inline_text_nodes2[0]);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf, pdf_focus_action_target->GetType());
  EXPECT_TRUE(pdf_anchor_action_target->SetSelection(
      pdf_anchor_action_target.get(), 1, pdf_focus_action_target.get(), 5));

  chrome_pdf::AccessibilityActionData pdf_action_data =
      action_handler.received_action_data();
  EXPECT_EQ(chrome_pdf::AccessibilityAction::kSetSelection,
            pdf_action_data.action);
  EXPECT_EQ(0u, pdf_action_data.selection_start_index.page_index);
  EXPECT_EQ(1u, pdf_action_data.selection_start_index.char_index);
  EXPECT_EQ(0u, pdf_action_data.selection_end_index.page_index);
  EXPECT_EQ(20u, pdf_action_data.selection_end_index.char_index);

  pdf_anchor_action_target =
      pdf_accessibility_tree.CreateActionTarget(*static_text_nodes1[0]);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf,
            pdf_anchor_action_target->GetType());
  pdf_focus_action_target =
      pdf_accessibility_tree.CreateActionTarget(*inline_text_nodes2[0]);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf, pdf_focus_action_target->GetType());
  EXPECT_TRUE(pdf_anchor_action_target->SetSelection(
      pdf_anchor_action_target.get(), 1, pdf_focus_action_target.get(), 4));

  pdf_action_data = action_handler.received_action_data();
  EXPECT_EQ(chrome_pdf::AccessibilityAction::kSetSelection,
            pdf_action_data.action);
  EXPECT_EQ(0u, pdf_action_data.selection_start_index.page_index);
  EXPECT_EQ(1u, pdf_action_data.selection_start_index.char_index);
  EXPECT_EQ(0u, pdf_action_data.selection_end_index.page_index);
  EXPECT_EQ(19u, pdf_action_data.selection_end_index.char_index);

  pdf_anchor_action_target =
      pdf_accessibility_tree.CreateActionTarget(*para_nodes[0]);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf,
            pdf_anchor_action_target->GetType());
  pdf_focus_action_target =
      pdf_accessibility_tree.CreateActionTarget(*para_nodes[1]);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf, pdf_focus_action_target->GetType());
  EXPECT_FALSE(pdf_anchor_action_target->SetSelection(
      pdf_anchor_action_target.get(), 1, pdf_focus_action_target.get(), 5));
}

TEST_F(PdfAccessibilityTreeTest, TestShowContextMenuAction) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  content::RenderFrame* render_frame = GetMainRenderFrame();
  ASSERT_TRUE(render_frame);
  render_frame->SetAccessibilityModeForTest(ui::AXMode::kWebContents);
  ASSERT_TRUE(render_frame->GetRenderAccessibility());

  TestPdfAccessibilityActionHandler action_handler;
  PdfAccessibilityTree pdf_accessibility_tree(render_frame, &action_handler);
  pdf_accessibility_tree.SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree.SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree.SetAccessibilityPageInfo(page_info_, text_runs_,
                                                  chars_, page_objects_);
  WaitForThreadTasks();

  ui::AXNode* root_node = pdf_accessibility_tree.GetRoot();
  ASSERT_TRUE(root_node);

  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      pdf_accessibility_tree.CreateActionTarget(*root_node);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf, pdf_action_target->GetType());
  {
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kShowContextMenu;
    EXPECT_TRUE(pdf_action_target->PerformAction(action_data));
  }
}

}  // namespace pdf
