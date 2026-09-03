// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_tree.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <ranges>
#include <string_view>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_accessibility.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdf_accessibility_action_handler.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/strings/grit/auto_image_annotation_strings.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/geometry/transform.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace pdf {

namespace {

constexpr size_t kCharsPerWord = 15;

// Colors are in ARGB format (0xAARRGGBB).
constexpr uint32_t kBlack = 0xFF000000;
constexpr uint32_t kRed = 0xFFFF0000;

constexpr float kHeadingFontSize = 24.0f;
constexpr float kBodyFontSize = 10.0f;

constexpr int kNormalFontWeight = 400;
constexpr int kSemiBoldFontWeight = 600;
constexpr int kBoldFontWeight = 700;

constexpr char kRegularFontName[] = "Helvetica-Regular";
constexpr char kBoldFontName[] = "Helvetica-Bold";

const chrome_pdf::AccessibilityTextRunInfo kFirstTextRun = {
    /*start_index=*/0,
    /*len=*/15,
    gfx::RectF(26.0f, 189.0f, 84.0f, 13.0f),
    chrome_pdf::AccessibilityTextDirection::kNone,
    chrome_pdf::AccessibilityTextStyleInfo()};
const chrome_pdf::AccessibilityTextRunInfo kSecondTextRun = {
    /*start_index=*/15,
    /*len=*/15,
    gfx::RectF(28.0f, 117.0f, 152.0f, 19.0f),
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
    /*start_index=*/0,
    /*len=*/7,
    gfx::RectF(26.0f, 189.0f, 84.0f, 13.0f),
    chrome_pdf::AccessibilityTextDirection::kNone,
    chrome_pdf::AccessibilityTextStyleInfo()};
const chrome_pdf::AccessibilityTextRunInfo kSecondRunMultiLine = {
    /*start_index=*/7,
    /*len=*/8,
    gfx::RectF(26.0f, 189.0f, 84.0f, 13.0f),
    chrome_pdf::AccessibilityTextDirection::kNone,
    chrome_pdf::AccessibilityTextStyleInfo()};
const chrome_pdf::AccessibilityTextRunInfo kThirdRunMultiLine = {
    /*start_index=*/15,
    /*len=*/9,
    gfx::RectF(26.0f, 189.0f, 84.0f, 13.0f),
    chrome_pdf::AccessibilityTextDirection::kNone,
    chrome_pdf::AccessibilityTextStyleInfo()};
const chrome_pdf::AccessibilityTextRunInfo kFourthRunMultiLine = {
    /*start_index=*/24,
    /*len=*/6,
    gfx::RectF(26.0f, 189.0f, 84.0f, 13.0f),
    chrome_pdf::AccessibilityTextDirection::kNone,
    chrome_pdf::AccessibilityTextStyleInfo()};

const char kChromiumTestUrl[] = "www.cs.chromium.org";

using testing::Matches;
using testing::PrintToString;
using testing::UnorderedElementsAre;

// `MATCHER_P2` is copied from ui/accessibility/ax_event_generator_unittest.cc.
MATCHER_P2(HasEventAtNode,
           expected_event_type,
           expected_node_id,
           std::string(negation ? "does not have" : "has") + " " +
               PrintToString(expected_event_type) + " on " +
               PrintToString(expected_node_id)) {
  const auto& event = arg;
  return Matches(expected_event_type)(event.event_params->event) &&
         Matches(expected_node_id)(event.node_id);
}

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

void CheckRootAndStatusNodes(const ui::AXNode* root_node,
                             size_t num_child,
                             bool is_pdf_ocr_test,
                             bool is_ocr_completed,
                             bool create_empty_ocr_results) {
  ASSERT_NE(nullptr, root_node);
  EXPECT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());

  // There should be `num_child` + 1 (the status wrapper node).
  ASSERT_EQ(num_child + 1u, root_node->GetChildCount());

  const ui::AXNode* status_wrapper = root_node->GetChildAtIndex(0);
  ASSERT_NE(nullptr, status_wrapper);
  EXPECT_EQ(ax::mojom::Role::kBanner, status_wrapper->GetRole());
  ASSERT_EQ(1u, status_wrapper->GetChildCount());

  const ui::AXNode* status_node = status_wrapper->GetChildAtIndex(0);
  ASSERT_NE(nullptr, status_node);
  EXPECT_EQ(ax::mojom::Role::kStatus, status_node->GetRole());

  if (!is_pdf_ocr_test) {
    return;
  }
  // Following are test steps needed for PDF OCR.
  if (is_ocr_completed) {
    // Note that the string below must be synced with `IDS_PDF_OCR_NO_RESULT`.
    constexpr char kPdfOcrNoResult[] =
        "This PDF is inaccessible. No text extracted";
    // Note that the string below must be synced with `IDS_PDF_OCR_COMPLETED`.
    constexpr char kPdfOcrCompleted[] =
        "This PDF is inaccessible. Text extracted, powered by Google AI";
    ASSERT_EQ(
        create_empty_ocr_results ? kPdfOcrNoResult : kPdfOcrCompleted,
        status_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  } else {
    // Note that the string below must be synced with
    // `IDS_PDF_OCR_FEATURE_ALERT`.
    constexpr char kPdfOcrFeatureAlert[] =
        "This PDF is inaccessible. Couldn't download text extraction files. "
        "Please try again later.";
    ASSERT_EQ(kPdfOcrFeatureAlert, status_node->GetStringAttribute(
                                       ax::mojom::StringAttribute::kName));
  }
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
  void LoadOrReloadAccessibility() override {}

  chrome_pdf::AccessibilityActionData received_action_data() {
    return received_action_data_;
  }

 private:
  chrome_pdf::AccessibilityActionData received_action_data_;
};

struct ImagePosition {
  int32_t page_index;
  int32_t page_object_index;
};

// Waits for tasks posted to the thread's task runner to complete.
void WaitForThreadTasks() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

class TestPdfAccessibilityTree : public PdfAccessibilityTree {
 public:
  TestPdfAccessibilityTree(
      content::RenderFrame* render_frame,
      chrome_pdf::PdfAccessibilityActionHandler* action_handler)
      : PdfAccessibilityTree(render_frame,
                             action_handler,
                             /*plugin_container=*/nullptr) {
    ForcePluginAXObjectForTesting(blink::WebAXObject::FromWebNode(
        render_frame->GetWebFrame()->GetDocument().Body()));
  }

  ~TestPdfAccessibilityTree() override = default;
  TestPdfAccessibilityTree(const TestPdfAccessibilityTree&) = delete;
  TestPdfAccessibilityTree& operator=(const TestPdfAccessibilityTree&) = delete;
};

std::vector<chrome_pdf::AccessibilityCharInfo> MakeCharVector(
    const std::vector<std::string>& words) {
  std::vector<chrome_pdf::AccessibilityCharInfo> chars;
  chars.reserve(words.size() * kCharsPerWord);
  for (const auto& word : words) {
    for (size_t i = 0; i < kCharsPerWord; ++i) {
      chrome_pdf::AccessibilityCharInfo char_info;
      char_info.unicode_character = (i < word.length()) ? word[i] : ' ';
      char_info.char_width = 10.0f;
      chars.push_back(char_info);
    }
  }
  return chars;
}

}  // namespace

class PdfAccessibilityTreeTest : public content::RenderViewTest {
 public:
  PdfAccessibilityTreeTest() = default;
  PdfAccessibilityTreeTest(const PdfAccessibilityTreeTest&) = delete;
  PdfAccessibilityTreeTest& operator=(const PdfAccessibilityTreeTest&) = delete;
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
    page_info_.page_index = 0u;
    page_info_.text_run_count = 0u;
    page_info_.char_count = 0u;
    page_info_.bounds = gfx::Rect(0, 0, 1, 1);
  }

  void TearDown() override {
    // Ensure we clean up the PDF accessibility tree before the page closes
    // since we directly set a plugin container.
    if (!IsSkipped()) {
      pdf_accessibility_tree_->ForcePluginAXObjectForTesting(
          blink::WebAXObject());
    }
    content::RenderViewTest::TearDown();
  }

  void CreatePdfAccessibilityTree() {
    content::RenderFrame* render_frame = GetMainRenderFrame();
    render_frame->SetAccessibilityModeForTest(ui::kAXModeComplete);
    ASSERT_TRUE(render_frame->GetRenderAccessibility());

    pdf_accessibility_tree_ = std::make_unique<TestPdfAccessibilityTree>(
        render_frame, &action_handler_);
    WaitForThreadTasks();
  }

  // Advance time clock in order for tasks posted with delay to run. Then, wait
  // for the delayed tasks posted to the thread's task runner to complete.
  void WaitForThreadDelayedTasks() {
    // `kDelay` must be synced with `kDelayBeforeRemovingStatusNode` in
    // pdf_accessibility_tree.cc.
    constexpr base::TimeDelta kDelay = base::Seconds(1);
    task_environment_.AdvanceClock(kDelay);
    task_environment_.RunUntilIdle();
  }

 protected:
  // Set up accessibility tree for testing heuristics, using a set of font
  // sizes. The number of text runs created is equal to the size of
  // `font_sizes`.
  void SetUpHeuristicAccessibilityTree(const std::vector<float>& font_sizes) {
    SetUpHeuristicAccessibilityTreeDetailed(font_sizes, /*styles=*/{},
                                            /*custom_chars=*/{},
                                            /*bounds=*/{});
  }

  // Detailed setup for heuristics tests that require customizing text runs
  // beyond font sizes, including styles, character contents, and layout bounds.
  // - `styles`: If specified, the style of the text run element at index i
  //   will be customized using `styles[i]`. `styles` does not need to have the
  //   same size as `font_sizes`; if it has fewer elements, only the first few
  //   runs will have custom styles.
  // - `custom_chars`: If specified, custom character data to populate
  //   the page text.
  // - `bounds`: If specified, layout bounds for the text runs. Like
  //   `styles`, it can have fewer elements than `font_sizes`.
  void SetUpHeuristicAccessibilityTreeDetailed(
      const std::vector<float>& font_sizes,
      const std::vector<chrome_pdf::AccessibilityTextStyleInfo>& styles,
      const std::vector<chrome_pdf::AccessibilityCharInfo>& custom_chars,
      const std::vector<gfx::RectF>& bounds = {}) {
    CreatePdfAccessibilityTree();
    CHECK(text_runs_.empty());
    for (size_t i = 0; i < font_sizes.size(); ++i) {
      chrome_pdf::AccessibilityTextRunInfo run =
          (i % 2 == 0) ? kFirstTextRun : kSecondTextRun;
      run.style.font_size = font_sizes[i];
      if (i < styles.size()) {
        run.style.font_name = styles[i].font_name;
        run.style.font_weight = styles[i].font_weight;
        run.style.is_italic = styles[i].is_italic;
        run.style.fill_color = styles[i].fill_color;
      }
      if (i < bounds.size()) {
        run.bounds = bounds[i];
      }
      text_runs_.push_back(run);
    }

    CHECK(chars_.empty());
    if (custom_chars.empty()) {
      size_t total_chars = text_runs_.size() * kCharsPerWord;
      while (chars_.size() < total_chars) {
        std::ranges::copy(kDummyCharsData, std::back_inserter(chars_));
      }
      chars_.resize(total_chars);
    } else {
      chars_ = custom_chars;
    }

    page_info_.text_run_count = text_runs_.size();
    page_info_.char_count = chars_.size();
    pdf_accessibility_tree_->SetAccessibilityDocInfo(
        CreateAccessibilityDocInfo());
    pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);

    pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                      chars_, page_objects_);
    WaitForThreadTasks();
    // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
    WaitForThreadDelayedTasks();
  }

  std::unique_ptr<chrome_pdf::AccessibilityDocInfo> CreateAccessibilityDocInfo()
      const {
    auto doc_info = std::make_unique<chrome_pdf::AccessibilityDocInfo>();
    doc_info->page_count = page_count_;
    doc_info->is_tagged = false;
    doc_info->text_accessible = true;
    doc_info->text_copyable = true;
    return doc_info;
  }

  chrome_pdf::AccessibilityImageInfo CreateMockInaccessibleImage() {
    chrome_pdf::AccessibilityImageInfo image;
    image.alt_text = "";
    image.bounds = gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f);
    image.page_object_index = 0;
    return image;
  }

  ui::AXNode* SetUpAccessibilityTreeForStyleSplitting() {
    auto doc_structure_root =
        std::make_unique<chrome_pdf::AccessibilityStructureElement>();
    doc_structure_root->type = chrome_pdf::PdfTagType::kDocument;

    auto page_structure =
        std::make_unique<chrome_pdf::AccessibilityStructureElement>();
    page_structure->type = chrome_pdf::PdfTagType::kPart;

    auto para = std::make_unique<chrome_pdf::AccessibilityStructureElement>();
    para->type = chrome_pdf::PdfTagType::kP;
    for (auto& run : text_runs_) {
      para->associated_text_runs_if_available.push_back(&run);
    }

    page_structure->children.push_back(std::move(para));
    doc_structure_root->children.push_back(std::move(page_structure));

    std::unique_ptr<chrome_pdf::AccessibilityDocInfo> doc_info =
        CreateAccessibilityDocInfo();
    doc_info->is_tagged = true;
    doc_info->structure_tree_root = std::move(doc_structure_root);

    pdf_accessibility_tree_->SetAccessibilityDocInfo(std::move(doc_info));
    pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
    pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                      chars_, page_objects_);
    WaitForThreadTasks();
    WaitForThreadDelayedTasks();

    ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
    if (!root_node || root_node->GetChildCount() <= 1u) {
      return nullptr;
    }
    ui::AXNode* page_node = root_node->GetChildAtIndex(1);
    if (!page_node || page_node->GetChildCount() != 1u) {
      return nullptr;
    }
    ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
    if (!paragraph_node ||
        paragraph_node->GetRole() != ax::mojom::Role::kParagraph) {
      return nullptr;
    }
    return paragraph_node;
  }

  void SetUpStyleSplittingTestRunsAndChars() {
    // Define three runs that are all on the same line so they get merged into a
    // single paragraph block node, but have style transitions.
    chrome_pdf::AccessibilityTextRunInfo run1;
    run1.start_index = 0;
    run1.len = 5;
    run1.style.font_name = "Arial";
    run1.style.font_weight = 400;
    run1.style.is_italic = false;
    run1.bounds = gfx::RectF(0.0f, 0.0f, 50.0f, 10.0f);

    chrome_pdf::AccessibilityTextRunInfo run2;
    run2.start_index = 5;
    run2.len = 5;
    run2.style.font_name = "Arial";
    run2.style.is_italic = false;
    run2.style.font_weight = 700;
    run2.bounds = gfx::RectF(50.0f, 0.0f, 50.0f, 10.0f);

    chrome_pdf::AccessibilityTextRunInfo run3;
    run3.start_index = 10;
    run3.len = 5;
    run3.style.font_name = "Arial";
    run3.style.font_weight = 400;
    run3.style.is_italic = false;
    run3.bounds = gfx::RectF(100.0f, 0.0f, 50.0f, 10.0f);

    text_runs_ = {run1, run2, run3};

    // 15 dummy characters.
    for (int i = 0; i < 15; ++i) {
      chrome_pdf::AccessibilityCharInfo char_info;
      char_info.unicode_character = 'a' + i;
      char_info.char_width = 10.0f;
      chars_.push_back(char_info);
    }

    page_info_.text_run_count = text_runs_.size();
    page_info_.char_count = chars_.size();
  }

  chrome_pdf::AccessibilityViewportInfo viewport_info_;
  uint32_t page_count_ = 1u;
  chrome_pdf::AccessibilityPageInfo page_info_;
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs_;
  std::vector<chrome_pdf::AccessibilityCharInfo> chars_;
  chrome_pdf::AccessibilityPageObjects page_objects_;
  std::unique_ptr<TestPdfAccessibilityTree> pdf_accessibility_tree_;
  TestPdfAccessibilityActionHandler action_handler_;
};

TEST_F(PdfAccessibilityTreeTest, TestEmptyPDFPage) {
  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  EXPECT_EQ(ax::mojom::Role::kPdfRoot,
            pdf_accessibility_tree_->GetRoot()->GetRole());
}

TEST_F(PdfAccessibilityTreeTest, TestAccessibilityDisabledDuringPDFLoad) {
  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  WaitForThreadTasks();

  // Disable accessibility while the PDF is loading, make sure this
  // doesn't crash.
  GetMainRenderFrame()->SetAccessibilityModeForTest(ui::AXMode());

  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();
  pdf_accessibility_tree_->ForcePluginAXObjectForTesting(blink::WebAXObject());
}

TEST_F(PdfAccessibilityTreeTest, TestPdfAccessibilityTreeReload) {
  CreatePdfAccessibilityTree();

  // Make the accessibility tree with a portrait page and then remake with a
  // landscape page.
  gfx::RectF page_bounds = gfx::RectF(1, 2);
  for (size_t i = 1; i <= 2; ++i) {
    if (i == 2)
      page_bounds.Transpose();

    page_info_.bounds = gfx::ToEnclosingRect(page_bounds);
    pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
    pdf_accessibility_tree_->SetAccessibilityDocInfo(
        CreateAccessibilityDocInfo());
    pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                      chars_, page_objects_);
    WaitForThreadTasks();
    // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
    WaitForThreadDelayedTasks();

    ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
    ASSERT_TRUE(root_node);
    EXPECT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());

    // There should be two nodes; the status node (wrapper) and one page node.
    ASSERT_EQ(2u, root_node->GetChildCount());

    ui::AXNode* page_node = root_node->GetChildAtIndex(1);
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

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

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(root_node->GetChildCount(), 1u);
  ui::AXNode* page_node = root_node->GetChildAtIndex(1);
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  ASSERT_EQ(2u, page_node->GetChildCount());

  ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  EXPECT_TRUE(paragraph_node->GetBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject));
  ASSERT_EQ(1u, paragraph_node->GetChildCount());

  ui::AXNode* link_node = paragraph_node->GetChildAtIndex(0);
  ASSERT_TRUE(link_node);
  EXPECT_EQ(kChromiumTestUrl,
            link_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
  EXPECT_EQ(ax::mojom::Role::kLink, link_node->GetRole());
  EXPECT_EQ(gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f),
            link_node->data().relative_bounds.bounds);
  ASSERT_EQ(1u, link_node->GetChildCount());

  paragraph_node = page_node->GetChildAtIndex(1);
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  EXPECT_TRUE(paragraph_node->GetBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject));
  ASSERT_EQ(3u, paragraph_node->GetChildCount());

  ui::AXNode* static_text_node = paragraph_node->GetChildAtIndex(0);
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->GetChildCount());

  ui::AXNode* image_node = paragraph_node->GetChildAtIndex(1);
  ASSERT_TRUE(image_node);
  EXPECT_EQ(ax::mojom::Role::kImage, image_node->GetRole());
  EXPECT_EQ(gfx::RectF(8.0f, 9.0f, 2.0f, 1.0f),
            image_node->data().relative_bounds.bounds);
  EXPECT_EQ(kTestAltText,
            image_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  image_node = paragraph_node->GetChildAtIndex(2);
  ASSERT_TRUE(image_node);
  EXPECT_EQ(ax::mojom::Role::kImage, image_node->GetRole());
  EXPECT_EQ(gfx::RectF(11.0f, 14.0f, 5.0f, 8.0f),
            image_node->data().relative_bounds.bounds);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION),
            image_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
}

TEST_F(PdfAccessibilityTreeTest, HeuristicStyleSplittingEnabled) {
  SetUpStyleSplittingTestRunsAndChars();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  CreatePdfAccessibilityTree();
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  WaitForThreadDelayedTasks();

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(root_node->GetChildCount(), 1u);
  ui::AXNode* page_node = root_node->GetChildAtIndex(1);
  ASSERT_TRUE(page_node);
  ASSERT_EQ(1u, page_node->GetChildCount());  // One paragraph.

  ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
  ASSERT_TRUE(paragraph_node);
  // When enabled, style splits create 3 static text nodes.
  ASSERT_EQ(3u, paragraph_node->GetChildCount());

  ui::AXNode* child1 = paragraph_node->GetChildAtIndex(0);
  EXPECT_EQ(ax::mojom::Role::kStaticText, child1->GetRole());
  EXPECT_EQ("abcde",
            child1->GetStringAttribute(ax::mojom::StringAttribute::kName));

  ui::AXNode* child2 = paragraph_node->GetChildAtIndex(1);
  EXPECT_EQ(ax::mojom::Role::kStaticText, child2->GetRole());
  EXPECT_EQ("fghij",
            child2->GetStringAttribute(ax::mojom::StringAttribute::kName));
  // Verify the bold text style and font weight attribute are added to the
  // static text node.
  EXPECT_TRUE(child2->data().HasTextStyle(ax::mojom::TextStyle::kBold));
  EXPECT_FLOAT_EQ(700.0f, child2->data().GetFloatAttribute(
                              ax::mojom::FloatAttribute::kFontWeight));

  ui::AXNode* child3 = paragraph_node->GetChildAtIndex(2);
  EXPECT_EQ(ax::mojom::Role::kStaticText, child3->GetRole());
  EXPECT_EQ("klmno",
            child3->GetStringAttribute(ax::mojom::StringAttribute::kName));
}

TEST_F(PdfAccessibilityTreeTest, HeuristicStyleSplittingDisabled) {
  SetUpStyleSplittingTestRunsAndChars();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {::features::kPdfAccessibilityHeuristicEnhancements,
           chrome_pdf::features::kPdfTags});

  CreatePdfAccessibilityTree();
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  WaitForThreadDelayedTasks();

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(root_node->GetChildCount(), 1u);
  ui::AXNode* page_node = root_node->GetChildAtIndex(1);
  ASSERT_TRUE(page_node);
  ASSERT_EQ(1u, page_node->GetChildCount());

  ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
  ASSERT_TRUE(paragraph_node);
  // When disabled, legacy path merges all runs into a single static text node.
  ASSERT_EQ(1u, paragraph_node->GetChildCount());

  ui::AXNode* child = paragraph_node->GetChildAtIndex(0);
  EXPECT_EQ(ax::mojom::Role::kStaticText, child->GetRole());
  EXPECT_EQ("abcdefghijklmno",
            child->GetStringAttribute(ax::mojom::StringAttribute::kName));
}

TEST_F(PdfAccessibilityTreeTest, HeadingsDetectedByHeuristic) {
  base::test::ScopedFeatureList pdf_tags;
  pdf_tags.InitAndDisableFeature(chrome_pdf::features::kPdfTags);

  SetUpHeuristicAccessibilityTree(/*font_sizes=*/{16.0f, 8.0f, 8.0f, 8.0f});

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(pdf_root, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(4u, page->GetChildCount());

  const ui::AXNode* heuristic_heading = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, heuristic_heading);
  EXPECT_EQ(ax::mojom::Role::kHeading, heuristic_heading->GetRole());
  EXPECT_EQ(2, heuristic_heading->GetIntAttribute(
                   ax::mojom::IntAttribute::kHierarchicalLevel));
  EXPECT_EQ("h2", heuristic_heading->GetStringAttribute(
                      ax::mojom::StringAttribute::kHtmlTag));

  const ui::AXNode* paragraph1 = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, paragraph1);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph1->GetRole());

  const ui::AXNode* paragraph2 = page->GetChildAtIndex(2u);
  ASSERT_NE(nullptr, paragraph2);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph2->GetRole());

  const ui::AXNode* paragraph3 = page->GetChildAtIndex(3u);
  ASSERT_NE(nullptr, paragraph3);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph3->GetRole());
}

TEST_F(PdfAccessibilityTreeTest, MultipleHeadingsDetectedByHeuristic) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  // 7 runs: 2 heading candidates, 5 body candidates to establish small median
  // (10.0f)
  SetUpHeuristicAccessibilityTree(
      {24.0f, 10.0f, 18.0f, 10.0f, 10.0f, 10.0f, 10.0f});

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(pdf_root, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(7u, page->GetChildCount());

  // size 24.0f: maps to level 1 (H1)
  const ui::AXNode* heading1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, heading1);
  EXPECT_EQ(ax::mojom::Role::kHeading, heading1->GetRole());
  EXPECT_EQ(1, heading1->GetIntAttribute(
                   ax::mojom::IntAttribute::kHierarchicalLevel));
  EXPECT_EQ("h1",
            heading1->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag));

  // size 10.0f: Paragraph
  const ui::AXNode* paragraph1 = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, paragraph1);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph1->GetRole());

  // size 18.0f: H2
  const ui::AXNode* heading2 = page->GetChildAtIndex(2u);
  ASSERT_NE(nullptr, heading2);
  EXPECT_EQ(ax::mojom::Role::kHeading, heading2->GetRole());
  EXPECT_EQ(2, heading2->GetIntAttribute(
                   ax::mojom::IntAttribute::kHierarchicalLevel));
  EXPECT_EQ("h2",
            heading2->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag));

  // size 10.0f: Paragraph
  for (size_t i = 3; i < 7; ++i) {
    const ui::AXNode* para = page->GetChildAtIndex(i);
    ASSERT_NE(nullptr, para);
    EXPECT_EQ(ax::mojom::Role::kParagraph, para->GetRole());
  }
}

TEST_F(PdfAccessibilityTreeTest, HeadingToBodySizeRatioMetrics) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  // 7 runs: 2 heading candidates (24.0f and 18.0f), 5 body runs (10.0f).
  // Body baseline (median) = 10.0f.
  // Max ratio = 24.0 / 10.0 = 2.4 -> 240
  // Min ratio = 18.0 / 10.0 = 1.8 -> 180
  SetUpHeuristicAccessibilityTree(
      /*font_sizes=*/{24.0f, 10.0f, 18.0f, 10.0f, 10.0f, 10.0f, 10.0f});

  histogram_tester.ExpectUniqueSample(
      "Accessibility.PdfHeuristics.HeadingToBodySizeRatioMax", 240, 1);
  histogram_tester.ExpectUniqueSample(
      "Accessibility.PdfHeuristics.HeadingToBodySizeRatioMin", 180, 1);
}

TEST_F(PdfAccessibilityTreeTest, HeadingClassifierMetrics_FontSize) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo style;
  style.font_weight = kNormalFontWeight;
  style.font_name = kRegularFontName;
  style.fill_color = kBlack;

  // 1 heading candidate (24.0f), 4 body runs (10.0f).
  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{kHeadingFontSize, kBodyFontSize, kBodyFontSize,
                      kBodyFontSize, kBodyFontSize},
      {style, style, style, style, style},
      MakeCharVector({"Heading", "body1", "body2", "body3", "body4"}));

  histogram_tester.ExpectUniqueSample(
      "Accessibility.PdfHeuristics.HeadingClassifier",
      HeadingClassifier::kFontSize, 1);
}

TEST_F(PdfAccessibilityTreeTest, HeadingClassifierMetrics_AllUppercase) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo style;
  style.font_weight = kNormalFontWeight;
  style.font_name = kRegularFontName;
  style.fill_color = kBlack;

  // 1 all-caps heading candidate (10.0f), 4 body runs (10.0f).
  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{kBodyFontSize, kBodyFontSize, kBodyFontSize,
                      kBodyFontSize, kBodyFontSize},
      {style, style, style, style, style},
      MakeCharVector({"ALLCAPS", "body1", "body2", "body3", "body4"}));

  histogram_tester.ExpectUniqueSample(
      "Accessibility.PdfHeuristics.HeadingClassifier",
      HeadingClassifier::kAllUppercase, 1);
}

TEST_F(PdfAccessibilityTreeTest, HeadingClassifierMetrics_BoldStyle) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo bold_style;
  bold_style.font_weight = kBoldFontWeight;
  bold_style.font_name = kRegularFontName;
  bold_style.fill_color = kBlack;

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = kNormalFontWeight;
  normal_style.font_name = kRegularFontName;
  normal_style.fill_color = kBlack;

  // 1 bold heading candidate (weight 700), 4 normal body runs (weight 400).
  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{kBodyFontSize, kBodyFontSize, kBodyFontSize,
                      kBodyFontSize, kBodyFontSize},
      {bold_style, normal_style, normal_style, normal_style, normal_style},
      MakeCharVector({"BoldHeading", "body1", "body2", "body3", "body4"}));

  histogram_tester.ExpectUniqueSample(
      "Accessibility.PdfHeuristics.HeadingClassifier",
      HeadingClassifier::kBoldStyle, 1);
}

TEST_F(PdfAccessibilityTreeTest, HeadingClassifierMetrics_SemiBoldWeight) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo semi_bold_style;
  semi_bold_style.font_weight = kSemiBoldFontWeight;
  semi_bold_style.font_name = kRegularFontName;
  semi_bold_style.fill_color = kBlack;

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = kNormalFontWeight;
  normal_style.font_name = kRegularFontName;
  normal_style.fill_color = kBlack;

  // 1 semi-bold heading candidate (weight 600), 4 normal body runs (weight
  // 400).
  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{kBodyFontSize, kBodyFontSize, kBodyFontSize,
                      kBodyFontSize, kBodyFontSize},
      {semi_bold_style, normal_style, normal_style, normal_style, normal_style},
      MakeCharVector({"SemiBoldHeading", "body1", "body2", "body3", "body4"}));

  histogram_tester.ExpectUniqueSample(
      "Accessibility.PdfHeuristics.HeadingClassifier",
      HeadingClassifier::kSemiBoldWeight, 1);
}

TEST_F(PdfAccessibilityTreeTest, HeadingClassifierMetrics_FontName) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo font_name_style;
  font_name_style.font_weight = 0;
  font_name_style.font_name = kBoldFontName;
  font_name_style.fill_color = kBlack;

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = kNormalFontWeight;
  normal_style.font_name = kRegularFontName;
  normal_style.fill_color = kBlack;

  // 1 font name heading candidate ("Helvetica-Bold"), 4 normal body runs.
  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{kBodyFontSize, kBodyFontSize, kBodyFontSize,
                      kBodyFontSize, kBodyFontSize},
      {font_name_style, normal_style, normal_style, normal_style, normal_style},
      MakeCharVector({"FontNameHeading", "body1", "body2", "body3", "body4"}));

  histogram_tester.ExpectUniqueSample(
      "Accessibility.PdfHeuristics.HeadingClassifier",
      HeadingClassifier::kFontName, 1);
}

TEST_F(PdfAccessibilityTreeTest, HeadingClassifierMetrics_TextColor) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo text_color_style;
  text_color_style.font_weight = kNormalFontWeight;
  text_color_style.font_name = kRegularFontName;
  text_color_style.fill_color = kRed;

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = kNormalFontWeight;
  normal_style.font_name = kRegularFontName;
  normal_style.fill_color = kBlack;

  // 1 red heading candidate (kRed), 4 black body runs (kBlack).
  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{kBodyFontSize, kBodyFontSize, kBodyFontSize,
                      kBodyFontSize, kBodyFontSize},
      {text_color_style, normal_style, normal_style, normal_style,
       normal_style},
      MakeCharVector({"ColorHeading", "body1", "body2", "body3", "body4"}));

  histogram_tester.ExpectUniqueSample(
      "Accessibility.PdfHeuristics.HeadingClassifier",
      HeadingClassifier::kTextColor, 1);
}

TEST_F(PdfAccessibilityTreeTest,
       MultipleHeadingsStartingAtH2DetectedByHeuristic) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  // 7 runs: 2 heading candidates, 5 body candidates to establish small median
  // (10.0f) Largest candidate is 15.0f (< 10.0 * 1.7 = 17.0), so starting
  // heading level is H2.
  SetUpHeuristicAccessibilityTree(
      {15.0f, 10.0f, 12.5f, 10.0f, 10.0f, 10.0f, 10.0f});

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(pdf_root, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(7u, page->GetChildCount());

  // size 15.0f: maps to level 2 (H2)
  const ui::AXNode* heading1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, heading1);
  EXPECT_EQ(ax::mojom::Role::kHeading, heading1->GetRole());
  EXPECT_EQ(2, heading1->GetIntAttribute(
                   ax::mojom::IntAttribute::kHierarchicalLevel));
  EXPECT_EQ("h2",
            heading1->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag));

  // size 10.0f: Paragraph
  const ui::AXNode* paragraph1 = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, paragraph1);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph1->GetRole());

  // size 12.5f: H3
  const ui::AXNode* heading2 = page->GetChildAtIndex(2u);
  ASSERT_NE(nullptr, heading2);
  EXPECT_EQ(ax::mojom::Role::kHeading, heading2->GetRole());
  EXPECT_EQ(3, heading2->GetIntAttribute(
                   ax::mojom::IntAttribute::kHierarchicalLevel));
  EXPECT_EQ("h3",
            heading2->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag));

  // size 10.0f: Paragraph
  for (size_t i = 3; i < 7; ++i) {
    const ui::AXNode* para = page->GetChildAtIndex(i);
    ASSERT_NE(nullptr, para);
    EXPECT_EQ(ax::mojom::Role::kParagraph, para->GetRole());
  }
}

TEST_F(PdfAccessibilityTreeTest, HeadingsOfSameLevelMerged) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  // 6 runs: 2 heading candidates (both level 1), 4 body candidates to establish
  // small median (10.0f)
  SetUpHeuristicAccessibilityTree(
      /*font_sizes=*/{24.0f, 23.5f, 10.0f, 10.0f, 10.0f, 10.0f});

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(pdf_root, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(5u, page->GetChildCount());

  // sizes 24.0f and 23.5f: sequential same-level H1 headings are merged
  const ui::AXNode* heading = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, heading);
  EXPECT_EQ(ax::mojom::Role::kHeading, heading->GetRole());
  EXPECT_EQ(
      1, heading->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel));
  EXPECT_EQ("h1",
            heading->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag));

  // size 10.0f: Paragraph
  for (size_t i = 1; i < 5; ++i) {
    const ui::AXNode* para = page->GetChildAtIndex(i);
    ASSERT_NE(nullptr, para);
    EXPECT_EQ(ax::mojom::Role::kParagraph, para->GetRole());
  }
}

TEST_F(PdfAccessibilityTreeTest, HeuristicBoldHeadingFollowedByNonBoldNewLine) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = 400;
  chrome_pdf::AccessibilityTextStyleInfo bold_style;
  bold_style.font_weight = 700;

  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{10.0f, 10.0f, 10.0f},
      {bold_style, normal_style, normal_style},
      MakeCharVector({"heading", "body", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(3u, page->GetChildCount());

  // Bold run on its own line: promoted to heading
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kHeading, block1->GetRole());
  EXPECT_EQ(
      3, block1->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel));

  // Non-bold run: remains paragraph
  const ui::AXNode* block2 = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, block2);
  EXPECT_EQ(ax::mojom::Role::kParagraph, block2->GetRole());
}

TEST_F(PdfAccessibilityTreeTest,
       HeuristicBoldFollowedByNonBoldSameLineNotHeading) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = 400;
  chrome_pdf::AccessibilityTextStyleInfo bold_style;
  bold_style.font_weight = 700;

  // First two runs are on the same line (y = 0.0f), while the third run is on a
  // different line (y = 30.0f).
  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{10.0f, 10.0f, 10.0f},
      {bold_style, normal_style, normal_style},
      MakeCharVector({"bold", "normal", "end"}),
      /*bounds=*/
      {gfx::RectF(0.0f, 0.0f, 50.0f, 10.0f),
       gfx::RectF(60.0f, 0.0f, 50.0f, 10.0f),
       gfx::RectF(0.0f, 30.0f, 50.0f, 10.0f)});

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(2u, page->GetChildCount());

  // Since bold run is on same line as normal run, it is not promoted and they
  // are grouped in a single paragraph.
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kParagraph, block1->GetRole());
}

TEST_F(PdfAccessibilityTreeTest,
       HeuristicBoldHeadingFollowedByBoldDifferentLines) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = 400;
  normal_style.font_name = "BodyFont";

  chrome_pdf::AccessibilityTextStyleInfo bold_style1;
  bold_style1.font_weight = 700;
  bold_style1.font_name = "BoldFont1";

  chrome_pdf::AccessibilityTextStyleInfo bold_style2;
  bold_style2.font_weight = 700;
  bold_style2.font_name = "BoldFont2";

  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{10.0f, 10.0f, 10.0f},
      {bold_style1, bold_style2, normal_style},
      MakeCharVector({"bold1", "bold2", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);

  // Both runs are bold with distinct fonts on separate lines: both promoted to
  // headings.
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kHeading, block1->GetRole());

  const ui::AXNode* block2 = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, block2);
  EXPECT_EQ(ax::mojom::Role::kHeading, block2->GetRole());
}

TEST_F(PdfAccessibilityTreeTest,
       HeuristicBoldSameStyleContinuousTextNotHeading) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo bold_style;
  bold_style.font_weight = 700;

  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{10.0f, 10.0f, 10.0f},
      {bold_style, bold_style, bold_style},
      MakeCharVector({"bold1", "bold2", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);

  // Bold runs sharing the same style at median font size: remains paragraph.
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kParagraph, block1->GetRole());
}

TEST_F(PdfAccessibilityTreeTest, HeuristicBoldRunSmallerThanMedianNotPromoted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = 400;
  chrome_pdf::AccessibilityTextStyleInfo bold_style;
  bold_style.font_weight = 700;

  // 5 runs: 1 bold run (size 8.0f), 4 normal runs (size 10.0f).
  // Median is 10.0f. Bold run font size 8.0f < median 10.0f.
  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{8.0f, 10.0f, 10.0f, 10.0f, 10.0f},
      {bold_style, normal_style, normal_style, normal_style, normal_style},
      MakeCharVector({"bold", "body1", "body2", "body3", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(5u, page->GetChildCount());

  // Bold run is smaller than median, so it remains a paragraph.
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kParagraph, block1->GetRole());
}

TEST_F(PdfAccessibilityTreeTest,
       HeuristicAllCapsHeadingFollowedByNonAllCapsNewLine) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = 400;
  normal_style.font_name = "BodyFont";

  chrome_pdf::AccessibilityTextStyleInfo heading_style;
  heading_style.font_weight = 700;
  heading_style.font_name = "HeadingFont";

  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{10.0f, 10.0f, 10.0f},
      {heading_style, normal_style, normal_style},
      MakeCharVector({"HEADING", "body", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(3u, page->GetChildCount());

  // All-caps run with a distinct style on its own line: promoted to heading
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kHeading, block1->GetRole());
  EXPECT_EQ(
      3, block1->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel));

  // Non-all-caps run: remains paragraph
  const ui::AXNode* block2 = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, block2);
  EXPECT_EQ(ax::mojom::Role::kParagraph, block2->GetRole());
}

TEST_F(PdfAccessibilityTreeTest,
       HeuristicAllCapsHeadingSameStyleContinuousText) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = 400;

  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{10.0f, 10.0f, 10.0f},
      {normal_style, normal_style, normal_style},
      MakeCharVector({"HEADING", "body", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);

  // All-caps run sharing the same style with the next line: promoted to heading
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kHeading, block1->GetRole());
}

TEST_F(PdfAccessibilityTreeTest,
       HeuristicAllCapsFollowedByNonAllCapsSameLineNotHeading) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = 400;

  // First two runs are on the same line (y = 0.0f), while the third run is on a
  // different line (y = 30.0f).
  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{10.0f, 10.0f, 10.0f},
      {normal_style, normal_style, normal_style},
      MakeCharVector({"HEADING", "normal", "end"}),
      {gfx::RectF(0.0f, 0.0f, 50.0f, 10.0f),
       gfx::RectF(60.0f, 0.0f, 50.0f, 10.0f),
       gfx::RectF(0.0f, 30.0f, 50.0f, 10.0f)});

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);

  // Since all-caps run is on same line as normal run, it is not promoted and
  // they are grouped in a single paragraph.
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kParagraph, block1->GetRole());
}

TEST_F(PdfAccessibilityTreeTest,
       HeuristicAllCapsHeadingFollowedByAllCapsDifferentLines) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo style1;
  style1.font_name = "Font1";

  chrome_pdf::AccessibilityTextStyleInfo style2;
  style2.font_name = "Font2";

  chrome_pdf::AccessibilityTextStyleInfo body_style;
  body_style.font_name = "BodyFont";

  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{10.0f, 10.0f, 10.0f}, {style1, style2, body_style},
      MakeCharVector({"HEADING1", "HEADING2", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);

  // Both runs are all-caps with distinct styles on separate lines: both
  // promoted to headings.
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kHeading, block1->GetRole());

  const ui::AXNode* block2 = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, block2);
  EXPECT_EQ(ax::mojom::Role::kHeading, block2->GetRole());
}

TEST_F(PdfAccessibilityTreeTest,
       HeuristicAllCapsRunSmallerThanMedianNotPromoted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = 400;

  // 5 runs: 1 all-caps run (size 8.0f), 4 normal runs (size 10.0f).
  // Median is 10.0f. All-caps run font size 8.0f < median 10.0f.
  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{8.0f, 10.0f, 10.0f, 10.0f, 10.0f},
      {normal_style, normal_style, normal_style, normal_style, normal_style},
      MakeCharVector({"CAPS", "body1", "body2", "body3", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(5u, page->GetChildCount());

  // All-caps run is smaller than median, so it remains a paragraph.
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kParagraph, block1->GetRole());
}

TEST_F(PdfAccessibilityTreeTest, HeuristicStyledHeadingUsesMappedHeadingLevel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = 400;
  chrome_pdf::AccessibilityTextStyleInfo bold_style;
  bold_style.font_weight = 700;

  // Font sizes (median = 10.0f, heading threshold = 12.0f):
  // - 24.0f: font-size heading (> threshold 12.0f) -> Level 1 (H1)
  // - 16.0f: font-size heading (> threshold 12.0f) -> Level 2 (H2)
  // - 13.0f: font-size heading (> threshold 12.0f) -> Level 3 (H3)
  // - 11.6f (bold): between median & threshold -> Level 4 (H4)
  // - 10.5f (all-caps): between median & threshold -> Level 5 (H5)
  // - 10.0f: median font size body text -> Paragraph
  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{24.0f, 16.0f, 13.0f, 11.6f, 10.5f, 10.0f, 10.0f, 10.0f,
                      10.0f, 10.0f, 10.0f},
      {normal_style, normal_style, normal_style, bold_style, normal_style,
       normal_style, normal_style, normal_style, normal_style, normal_style,
       normal_style},
      MakeCharVector({"Title", "Heading2", "Heading3", "bold116", "CAPS105",
                      "body1", "body2", "body3", "body4", "body5", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(6u, page->GetChildCount());

  // First run (24.0f, normal): font-size heading H1
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kHeading, block1->GetRole());
  EXPECT_EQ(
      1, block1->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel));

  // Second run (16.0f, normal): font-size heading H2
  const ui::AXNode* block2 = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, block2);
  EXPECT_EQ(ax::mojom::Role::kHeading, block2->GetRole());
  EXPECT_EQ(
      2, block2->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel));

  // Third run (13.0f, normal): font-size heading H3
  const ui::AXNode* block3 = page->GetChildAtIndex(2u);
  ASSERT_NE(nullptr, block3);
  EXPECT_EQ(ax::mojom::Role::kHeading, block3->GetRole());
  EXPECT_EQ(
      3, block3->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel));

  // Fourth run (11.6f, bold): styled heading mapped to H4
  const ui::AXNode* block4 = page->GetChildAtIndex(3u);
  ASSERT_NE(nullptr, block4);
  EXPECT_EQ(ax::mojom::Role::kHeading, block4->GetRole());
  EXPECT_EQ(
      4, block4->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel));

  // Fifth run (10.5f, all-caps): styled heading mapped to H5
  const ui::AXNode* block5 = page->GetChildAtIndex(4u);
  ASSERT_NE(nullptr, block5);
  EXPECT_EQ(ax::mojom::Role::kHeading, block5->GetRole());
  EXPECT_EQ(
      5, block5->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel));

  // Sixth run (10.0f, normal): body text paragraph
  const ui::AXNode* block6 = page->GetChildAtIndex(5u);
  ASSERT_NE(nullptr, block6);
  EXPECT_EQ(ax::mojom::Role::kParagraph, block6->GetRole());
}

TEST_F(PdfAccessibilityTreeTest,
       HeuristicSkipsH2WhenGoingDirectlyBelowHeadingThreshold) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = 400;
  chrome_pdf::AccessibilityTextStyleInfo bold_style;
  bold_style.font_weight = 700;

  // Font sizes:
  // - 24.0f: font-size heading (> threshold 12.0f) -> Level 1 (H1)
  // - 11.0f (bold): styled heading below threshold (12.0f).
  //   Heading level H2 is skipped because font size drops directly from
  //   above the heading threshold to below, clamping styled headings to start
  //   at H3.
  // - 10.0f: median font size body text -> Paragraph
  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{24.0f, 11.0f, 10.0f, 10.0f, 10.0f},
      {normal_style, bold_style, normal_style, normal_style, normal_style},
      MakeCharVector({"Title", "StyledHeading", "body1", "body2", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(5u, page->GetChildCount());

  // First run (24.0f, normal): font-size heading level 1 (H1)
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kHeading, block1->GetRole());
  EXPECT_EQ(
      1, block1->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel));

  // Second run (11.0f, bold): styled heading level 3 (H3). Level 2 is skipped.
  const ui::AXNode* block2 = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, block2);
  EXPECT_EQ(ax::mojom::Role::kHeading, block2->GetRole());
  EXPECT_EQ(
      3, block2->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel));

  // Third run (10.0f, normal): body text paragraph
  const ui::AXNode* block3 = page->GetChildAtIndex(2u);
  ASSERT_NE(nullptr, block3);
  EXPECT_EQ(ax::mojom::Role::kParagraph, block3->GetRole());
}

TEST_F(PdfAccessibilityTreeTest, HeuristicTextColorHeading) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo colored_heading_style;
  colored_heading_style.font_weight = kNormalFontWeight;
  colored_heading_style.font_name = kRegularFontName;
  colored_heading_style.fill_color = kRed;

  chrome_pdf::AccessibilityTextStyleInfo normal_body_style;
  normal_body_style.font_weight = kNormalFontWeight;
  normal_body_style.font_name = kRegularFontName;
  normal_body_style.fill_color = kBlack;

  // 3 runs: 1 red run (size 10.0f), 2 black body runs (size 10.0f).
  // Body color is `kBlack`. Red run differs from body color, so it is
  // classified as a heading.
  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{kBodyFontSize, kBodyFontSize, kBodyFontSize},
      {colored_heading_style, normal_body_style, normal_body_style},
      MakeCharVector({"RedHeading", "body", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(3u, page->GetChildCount());

  // First run (red fill_color): promoted to heading based on kTextColor
  // classifier.
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kHeading, block1->GetRole());
}

TEST_F(PdfAccessibilityTreeTest, HeuristicFontWeightHeading) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo weight_style;
  weight_style.font_weight = 600;

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = 400;

  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{10.0f, 10.0f, 10.0f, 10.0f},
      {weight_style, normal_style, normal_style, normal_style},
      MakeCharVector({"WeightHeading", "body1", "body2", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(4u, page->GetChildCount());

  // Valid font weight (< 700) run: promoted to heading
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kHeading, block1->GetRole());
}

TEST_F(PdfAccessibilityTreeTest,
       HeuristicFontWeightSameStyleContinuousTextNotHeading) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo valid_weight_style;
  valid_weight_style.font_weight = 700;

  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{10.0f, 10.0f, 10.0f},
      {valid_weight_style, valid_weight_style, valid_weight_style},
      MakeCharVector({"weight1", "weight2", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);

  // Runs sharing the same font weight style at median font size: remains
  // paragraph.
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kParagraph, block1->GetRole());
}

TEST_F(PdfAccessibilityTreeTest, HeuristicFontNameHeading) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo bold_name_style;
  bold_name_style.font_weight = 0;
  bold_name_style.font_name = "Helvetica-Bold";

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = 400;
  normal_style.font_name = "Helvetica-Regular";

  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{10.0f, 10.0f, 10.0f, 10.0f, 10.0f},
      {bold_name_style, normal_style, normal_style, normal_style, normal_style},
      MakeCharVector({"FontNameHeading", "body1", "body2", "body3", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(5u, page->GetChildCount());

  // Run with "Helvetica-Bold" font name and font_weight == 0: promoted to
  // heading
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kHeading, block1->GetRole());

  // Normal run: remains paragraph
  const ui::AXNode* block2 = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, block2);
  EXPECT_EQ(ax::mojom::Role::kParagraph, block2->GetRole());
}

TEST_F(PdfAccessibilityTreeTest, HeuristicHeadingBreakOnFontNameMismatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo bold_style_font1;
  bold_style_font1.font_weight = 700;
  bold_style_font1.font_name = "Arial";

  chrome_pdf::AccessibilityTextStyleInfo bold_style_font2;
  bold_style_font2.font_weight = 700;
  bold_style_font2.font_name = "TimesNewRoman";

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = 400;
  normal_style.font_name = "Arial";

  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{10.0f, 10.0f, 10.0f, 10.0f, 10.0f},
      {bold_style_font1, bold_style_font2, normal_style, normal_style,
       normal_style},
      MakeCharVector({"HeadingOne", "HeadingTwo", "body1", "body2", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(5u, page->GetChildCount());

  // First bold run (Arial): promoted to heading
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kHeading, block1->GetRole());

  // Second bold run (TimesNewRoman): should break into a separate heading block
  // because font_name differs from run 1.
  const ui::AXNode* block2 = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, block2);
  EXPECT_EQ(ax::mojom::Role::kHeading, block2->GetRole());
  EXPECT_NE(block1, block2);
}

TEST_F(PdfAccessibilityTreeTest, HeuristicBodyTextFontNameChangeDoesNotBreak) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo normal_style_font1;
  normal_style_font1.font_name = "Arial";

  chrome_pdf::AccessibilityTextStyleInfo normal_style_font2;
  normal_style_font2.font_name = "TimesNewRoman";

  // Three body text runs on the same line (y = 0.0f) with font_name change.
  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{10.0f, 10.0f, 10.0f},
      {normal_style_font1, normal_style_font2, normal_style_font1},
      MakeCharVector({"body1", "body2", "body3"}),
      /*bounds=*/
      {gfx::RectF(0.0f, 0.0f, 50.0f, 10.0f),
       gfx::RectF(60.0f, 0.0f, 50.0f, 10.0f),
       gfx::RectF(120.0f, 0.0f, 50.0f, 10.0f)});

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  // Font name change in body text on the same line should NOT break into
  // multiple blocks.
  ASSERT_EQ(1u, page->GetChildCount());

  const ui::AXNode* block = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block);
  EXPECT_EQ(ax::mojom::Role::kParagraph, block->GetRole());
}

TEST_F(PdfAccessibilityTreeTest, HeuristicHeadingBreakOnItalicStyleMismatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kPdfAccessibilityHeuristicEnhancements},
      {chrome_pdf::features::kPdfTags});

  chrome_pdf::AccessibilityTextStyleInfo bold_style;
  bold_style.font_weight = 700;
  bold_style.is_italic = false;

  chrome_pdf::AccessibilityTextStyleInfo bold_italic_style;
  bold_italic_style.font_weight = 700;
  bold_italic_style.is_italic = true;

  chrome_pdf::AccessibilityTextStyleInfo normal_style;
  normal_style.font_weight = 400;

  SetUpHeuristicAccessibilityTreeDetailed(
      /*font_sizes=*/{10.0f, 10.0f, 10.0f, 10.0f, 10.0f},
      {bold_style, bold_italic_style, normal_style, normal_style, normal_style},
      MakeCharVector(
          {"BoldHeading", "BoldItalicHeading", "body1", "body2", "end"}));

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(5u, page->GetChildCount());

  // First heading run (bold): promoted to heading
  const ui::AXNode* block1 = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, block1);
  EXPECT_EQ(ax::mojom::Role::kHeading, block1->GetRole());

  // Second heading run (bold italic): breaks into a separate heading block due
  // to italic style mismatch
  const ui::AXNode* block2 = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, block2);
  EXPECT_EQ(ax::mojom::Role::kHeading, block2->GetRole());
  EXPECT_NE(block1, block2);
}

class PdfAccessibilityTreeStructuredModeTest
    : public PdfAccessibilityTreeTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    PdfAccessibilityTreeTest::SetUp();
    if (UseStructuredMode()) {
      pdf_tags_.InitAndEnableFeature(chrome_pdf::features::kPdfTags);
    }
  }

  bool UseStructuredMode() const { return GetParam(); }

  void BuildAndSetAccessibilityTree() {
    page_count_ = 1;
    std::unique_ptr<chrome_pdf::AccessibilityDocInfo> doc_info =
        CreateAccessibilityDocInfo();

    if (UseStructuredMode()) {
      auto doc_structure_root =
          std::make_unique<chrome_pdf::AccessibilityStructureElement>();
      doc_structure_root->type = chrome_pdf::PdfTagType::kDocument;

      auto page_structure =
          std::make_unique<chrome_pdf::AccessibilityStructureElement>();
      page_structure->type = chrome_pdf::PdfTagType::kPart;

      auto para = std::make_unique<chrome_pdf::AccessibilityStructureElement>();
      para->type = chrome_pdf::PdfTagType::kP;
      for (auto& run : text_runs_) {
        para->associated_text_runs_if_available.push_back(&run);
      }

      page_structure->children.push_back(std::move(para));
      doc_structure_root->children.push_back(std::move(page_structure));

      doc_info->is_tagged = true;
      doc_info->structure_tree_root = std::move(doc_structure_root);
    }

    page_info_.text_run_count = text_runs_.size();
    page_info_.char_count = chars_.size();

    pdf_accessibility_tree_->SetAccessibilityDocInfo(std::move(doc_info));
    pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
    pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                      chars_, page_objects_);

    WaitForThreadTasks();
    WaitForThreadDelayedTasks();
  }

  ui::AXNode* FindFirstStaticTextNode() {
    const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
    if (!pdf_root || pdf_root->GetChildCount() <= 1u) {
      return nullptr;
    }
    const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
    if (!page || page->GetChildCount() == 0u) {
      return nullptr;
    }
    ui::AXNode* static_text = page->GetChildAtIndex(0u);
    while (static_text &&
           static_text->GetRole() != ax::mojom::Role::kStaticText &&
           static_text->GetChildCount() > 0u) {
      static_text = static_text->GetChildAtIndex(0u);
    }
    return static_text;
  }

 private:
  base::test::ScopedFeatureList pdf_tags_;
};

TEST_P(PdfAccessibilityTreeStructuredModeTest,
       TestSelectionConversionViaFindNodeOffset) {
  base::test::ScopedFeatureList pdf_tags;
  if (UseStructuredMode()) {
    pdf_tags.InitAndEnableFeature(chrome_pdf::features::kPdfTags);
  }
  CreatePdfAccessibilityTree();

  constexpr size_t kTextRunLength = 6;

  chrome_pdf::AccessibilityTextRunInfo first_run;
  first_run.start_index = 0;
  first_run.len = kTextRunLength;
  first_run.bounds = gfx::RectF(26.0f, 189.0f, 84.0f, 13.0f);
  first_run.direction = chrome_pdf::AccessibilityTextDirection::kNone;
  text_runs_.push_back(first_run);

  // Different location of second and later text run causes a paragraph
  // break for unstructured/heuristic mode.
  chrome_pdf::AccessibilityTextRunInfo second_run;
  second_run.start_index = kTextRunLength;
  second_run.len = kTextRunLength;
  second_run.bounds = gfx::RectF(26.0f, 210.0f, 84.0f, 13.0f);
  second_run.direction = chrome_pdf::AccessibilityTextDirection::kNone;
  text_runs_.push_back(second_run);

  chrome_pdf::AccessibilityTextRunInfo third_run;
  third_run.start_index = kTextRunLength * 2;
  third_run.len = kTextRunLength;
  third_run.bounds = gfx::RectF(26.0f, 210.0f, 84.0f, 13.0f);
  third_run.direction = chrome_pdf::AccessibilityTextDirection::kNone;
  text_runs_.push_back(third_run);

  chrome_pdf::AccessibilityTextRunInfo fourth_run;
  fourth_run.start_index = kTextRunLength * 3;
  fourth_run.len = kTextRunLength;
  fourth_run.bounds = gfx::RectF(26.0f, 210.0f, 84.0f, 13.0f);
  fourth_run.direction = chrome_pdf::AccessibilityTextDirection::kNone;
  text_runs_.push_back(fourth_run);

  chrome_pdf::AccessibilityTextRunInfo fifth_run;
  fifth_run.start_index = kTextRunLength * 4;
  fifth_run.len = kTextRunLength;
  fifth_run.bounds = gfx::RectF(26.0f, 210.0f, 84.0f, 13.0f);
  fifth_run.direction = chrome_pdf::AccessibilityTextDirection::kNone;
  text_runs_.push_back(fifth_run);

  // Create characters: "aaaaaa" for first run
  for (size_t i = 0; i < kTextRunLength; ++i) {
    chars_.push_back({static_cast<uint32_t>('a'), 10});
  }
  // Create characters: "bbbbbb" for second run
  for (size_t i = 0; i < kTextRunLength; ++i) {
    chars_.push_back({static_cast<uint32_t>('b'), 10});
  }
  // Create characters: "cccccc" for third run
  for (size_t i = 0; i < kTextRunLength; ++i) {
    chars_.push_back({static_cast<uint32_t>('c'), 10});
  }
  // Create characters: "dddddd" for fourth run
  for (size_t i = 0; i < kTextRunLength; ++i) {
    chars_.push_back({static_cast<uint32_t>('d'), 10});
  }
  // Create characters: "eeeeee" for fifth run
  for (size_t i = 0; i < kTextRunLength; ++i) {
    chars_.push_back({static_cast<uint32_t>('e'), 10});
  }

  std::unique_ptr<chrome_pdf::AccessibilityDocInfo> doc_info =
      CreateAccessibilityDocInfo();

  if (UseStructuredMode()) {
    // Build structure tree:
    // Document -> Part -> [Sect -> Sect -> P (first text run), P (second,
    // third, and fourth text run)]
    auto doc_structure_root =
        std::make_unique<chrome_pdf::AccessibilityStructureElement>();
    doc_structure_root->type = chrome_pdf::PdfTagType::kDocument;

    auto page_structure =
        std::make_unique<chrome_pdf::AccessibilityStructureElement>();
    page_structure->type = chrome_pdf::PdfTagType::kPart;

    // First element: Sect -> Sect -> P (first text run)
    auto outer_sect =
        std::make_unique<chrome_pdf::AccessibilityStructureElement>();
    outer_sect->type = chrome_pdf::PdfTagType::kSect;

    auto inner_sect =
        std::make_unique<chrome_pdf::AccessibilityStructureElement>();
    inner_sect->type = chrome_pdf::PdfTagType::kSect;

    auto first_para =
        std::make_unique<chrome_pdf::AccessibilityStructureElement>();
    first_para->type = chrome_pdf::PdfTagType::kP;
    first_para->associated_text_runs_if_available.push_back(&text_runs_[0]);

    inner_sect->children.push_back(std::move(first_para));
    outer_sect->children.push_back(std::move(inner_sect));

    // Second element: P (second, third and fourth text runs)
    auto second_para =
        std::make_unique<chrome_pdf::AccessibilityStructureElement>();
    second_para->type = chrome_pdf::PdfTagType::kP;
    second_para->associated_text_runs_if_available.push_back(&text_runs_[1]);
    second_para->associated_text_runs_if_available.push_back(&text_runs_[2]);
    second_para->associated_text_runs_if_available.push_back(&text_runs_[3]);
    second_para->associated_text_runs_if_available.push_back(&text_runs_[4]);

    page_structure->children.push_back(std::move(outer_sect));
    page_structure->children.push_back(std::move(second_para));
    doc_structure_root->children.push_back(std::move(page_structure));

    doc_info->is_tagged = true;
    doc_info->structure_tree_root = std::move(doc_structure_root);
  }

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  pdf_accessibility_tree_->SetAccessibilityDocInfo(std::move(doc_info));
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);

  WaitForThreadTasks();
  WaitForThreadDelayedTasks();

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(pdf_root, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);

  // Get the first StaticText node:
  // If structured: Page -> Section -> Section -> Paragraph -> StaticText
  // If not structured: Page -> Paragraph -> StaticText
  ui::AXNode* static_text = page->GetChildAtIndex(0u);
  while (static_text->GetRole() != ax::mojom::Role::kStaticText &&
         static_text->GetChildAtIndex(0u)) {
    static_text = static_text->GetChildAtIndex(0u);
  }
  ASSERT_NE(nullptr, static_text);

  // Verify that the calculated selection is at the zeroth index of
  // the first node.
  auto tree_data = pdf_accessibility_tree_->tree_data_for_testing();
  EXPECT_EQ(0, tree_data.sel_anchor_offset);
  EXPECT_EQ(0, tree_data.sel_focus_offset);

  // Verify that the selection anchor node ID matches the first StaticText node.
  EXPECT_EQ(static_text->id(), tree_data.sel_anchor_object_id);
  EXPECT_EQ(static_text->id(), tree_data.sel_focus_object_id);

  int32_t out_node_id = -1;
  int32_t out_node_char_index = 0;

  // Now get the node associated with the 10 character.
  pdf_accessibility_tree_->FindNodeOffsetForTesting(
      /*end_of_selection=*/false, 0, 10, &out_node_id, &out_node_char_index);

  // Get the last StaticText node:
  // Page -> Paragraph (second child of Page) -> StaticText
  const ui::AXNode* second_static_text =
      page->GetChildAtIndex(1u)->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, second_static_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, second_static_text->GetRole());
  EXPECT_EQ(4, out_node_char_index);
  EXPECT_EQ(second_static_text->id(), out_node_id);

  // Get the node and offset associated with the 11 character
  pdf_accessibility_tree_->FindNodeOffsetForTesting(
      /*end_of_selection=*/false, 0, 11, &out_node_id, &out_node_char_index);
  ASSERT_NE(nullptr, second_static_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, second_static_text->GetRole());
  EXPECT_EQ(5, out_node_char_index);
  EXPECT_EQ(second_static_text->id(), out_node_id);

  // Get the node and offset associated with the 22 character
  pdf_accessibility_tree_->FindNodeOffsetForTesting(
      /*end_of_selection=*/false, 0, 22, &out_node_id, &out_node_char_index);
  ASSERT_NE(nullptr, second_static_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, second_static_text->GetRole());
  EXPECT_EQ(16, out_node_char_index);
  EXPECT_EQ(second_static_text->id(), out_node_id);

  // Get the node and offset associated with the 24 character (end of selection)
  pdf_accessibility_tree_->FindNodeOffsetForTesting(
      /*end_of_selection=*/true, 0, 24, &out_node_id, &out_node_char_index);
  ASSERT_NE(nullptr, second_static_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, second_static_text->GetRole());
  EXPECT_EQ(18, out_node_char_index);
  EXPECT_EQ(second_static_text->id(), out_node_id);
}

TEST_P(PdfAccessibilityTreeStructuredModeTest,
       FindCharacterOffset_MultiLineStaticText) {
  CreatePdfAccessibilityTree();

  // Create two text runs representing two lines in a paragraph.
  // Line 1 ("Jellicle songs ") has 15 chars starting at index 0.
  // Line 2 ("for Jellicle cats") has 17 chars starting at index 15.
  chrome_pdf::AccessibilityTextRunInfo run1 = {
      /*start_index=*/0, /*len=*/15, gfx::RectF(0.0f, 0.0f, 100.0f, 10.0f),
      chrome_pdf::AccessibilityTextDirection::kNone,
      chrome_pdf::AccessibilityTextStyleInfo()};
  chrome_pdf::AccessibilityTextRunInfo run2 = {
      /*start_index=*/15, /*len=*/17, gfx::RectF(0.0f, 10.0f, 100.0f, 10.0f),
      chrome_pdf::AccessibilityTextDirection::kNone,
      chrome_pdf::AccessibilityTextStyleInfo()};

  text_runs_ = {run1, run2};

  constexpr std::string_view kText = "Jellicle songs for Jellicle cats";
  for (char c : kText) {
    chars_.push_back({static_cast<uint32_t>(c), 10.0f});
  }

  BuildAndSetAccessibilityTree();

  ui::AXNode* static_text = FindFirstStaticTextNode();
  ASSERT_NE(nullptr, static_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text->GetRole());
  chrome_pdf::PageCharacterIndex page_char_index;

  // Offset 0 (start of Line 1 "Jellicle songs ") -> PDFium char index 0.
  EXPECT_TRUE(pdf_accessibility_tree_->FindCharacterOffset(*static_text, 0,
                                                           page_char_index));
  EXPECT_EQ(0u, page_char_index.char_index);

  // Offset 15 (start of Line 2 "for Jellicle cats") -> PDFium char index 15.
  EXPECT_TRUE(pdf_accessibility_tree_->FindCharacterOffset(*static_text, 15,
                                                           page_char_index));
  EXPECT_EQ(15u, page_char_index.char_index);

  // Offset 16 (second char of Line 2 'o') -> PDFium char index 16.
  EXPECT_TRUE(pdf_accessibility_tree_->FindCharacterOffset(*static_text, 16,
                                                           page_char_index));
  EXPECT_EQ(16u, page_char_index.char_index);
}

TEST_P(PdfAccessibilityTreeStructuredModeTest,
       FindNodeOffset_MultiLineStaticText) {
  CreatePdfAccessibilityTree();

  // Create two text runs representing two lines in a paragraph.
  // Line 1 ("Jellicle songs ") has 15 chars starting at index 0.
  // Line 2 ("for Jellicle cats") has 17 chars starting at index 15.
  chrome_pdf::AccessibilityTextRunInfo run1 = {
      /*start_index=*/0, /*len=*/15, gfx::RectF(0.0f, 0.0f, 100.0f, 10.0f),
      chrome_pdf::AccessibilityTextDirection::kNone,
      chrome_pdf::AccessibilityTextStyleInfo()};
  chrome_pdf::AccessibilityTextRunInfo run2 = {
      /*start_index=*/15, /*len=*/17, gfx::RectF(0.0f, 10.0f, 100.0f, 10.0f),
      chrome_pdf::AccessibilityTextDirection::kNone,
      chrome_pdf::AccessibilityTextStyleInfo()};

  text_runs_ = {run1, run2};

  constexpr std::string_view kText = "Jellicle songs for Jellicle cats";
  for (char c : kText) {
    chars_.push_back({static_cast<uint32_t>(c), 10.0f});
  }

  BuildAndSetAccessibilityTree();

  ui::AXNode* static_text = FindFirstStaticTextNode();
  ASSERT_NE(nullptr, static_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text->GetRole());

  int32_t out_node_id = -1;
  int32_t out_node_char_index = -1;

  // Offset 0 (start of Line 1 "Jellicle songs ") -> static text char index 0.
  pdf_accessibility_tree_->FindNodeOffsetForTesting(
      /*end_of_selection=*/false, 0, 0, &out_node_id, &out_node_char_index);
  EXPECT_EQ(static_text->id(), out_node_id);
  EXPECT_EQ(0, out_node_char_index);

  // Offset 15 (start of Line 2 "for Jellicle cats") -> static text char
  // index 15.
  pdf_accessibility_tree_->FindNodeOffsetForTesting(
      /*end_of_selection=*/false, 0, 15, &out_node_id, &out_node_char_index);
  EXPECT_EQ(static_text->id(), out_node_id);
  EXPECT_EQ(15, out_node_char_index);

  // Offset 15 as end of selection (end of Line 1) -> static text char index 15.
  pdf_accessibility_tree_->FindNodeOffsetForTesting(
      /*end_of_selection=*/true, 0, 15, &out_node_id, &out_node_char_index);
  EXPECT_EQ(static_text->id(), out_node_id);
  EXPECT_EQ(15, out_node_char_index);

  // Offset 16 (second char of Line 2 'o') -> static text char index 16.
  pdf_accessibility_tree_->FindNodeOffsetForTesting(
      /*end_of_selection=*/false, 0, 16, &out_node_id, &out_node_char_index);
  EXPECT_EQ(static_text->id(), out_node_id);
  EXPECT_EQ(16, out_node_char_index);

  // Offset 32 as end of selection (end of Line 2) -> static text char index 32.
  pdf_accessibility_tree_->FindNodeOffsetForTesting(
      /*end_of_selection=*/true, 0, 32, &out_node_id, &out_node_char_index);
  EXPECT_EQ(static_text->id(), out_node_id);
  EXPECT_EQ(32, out_node_char_index);
}

TEST_P(PdfAccessibilityTreeStructuredModeTest,
       FindCharacterOffset_MultiLineStaticTextWithNonAscii) {
  CreatePdfAccessibilityTree();

  // Create two text runs representing two lines in a paragraph.
  // Line 1 contains a multibyte Unicode character (right single quote '’'
  // U+2019). Line 1 ("It’s a cat ") has 11 chars starting at index 0 (13 UTF-8
  // bytes). Line 2 ("named Oliver") has 12 chars starting at index 11.
  chrome_pdf::AccessibilityTextRunInfo run1 = {
      /*start_index=*/0, /*len=*/11, gfx::RectF(0.0f, 0.0f, 100.0f, 10.0f),
      chrome_pdf::AccessibilityTextDirection::kNone,
      chrome_pdf::AccessibilityTextStyleInfo()};
  chrome_pdf::AccessibilityTextRunInfo run2 = {
      /*start_index=*/11, /*len=*/12, gfx::RectF(0.0f, 10.0f, 100.0f, 10.0f),
      chrome_pdf::AccessibilityTextDirection::kNone,
      chrome_pdf::AccessibilityTextStyleInfo()};

  text_runs_ = {run1, run2};

  constexpr std::u16string_view kText = u"It’s a cat named Oliver";
  for (char16_t c : kText) {
    chars_.push_back({static_cast<uint32_t>(c), 10.0f});
  }

  BuildAndSetAccessibilityTree();

  ui::AXNode* static_text = FindFirstStaticTextNode();
  ASSERT_NE(nullptr, static_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text->GetRole());
  chrome_pdf::PageCharacterIndex page_char_index;

  // Offset 0 (start of Line 1 "It’s a cat ") -> PDFium char index 0.
  EXPECT_TRUE(pdf_accessibility_tree_->FindCharacterOffset(*static_text, 0,
                                                           page_char_index));
  EXPECT_EQ(0u, page_char_index.char_index);

  // Offset 11 (start of Line 2 "named Oliver") -> PDFium char index 11.
  EXPECT_TRUE(pdf_accessibility_tree_->FindCharacterOffset(*static_text, 11,
                                                           page_char_index));
  EXPECT_EQ(11u, page_char_index.char_index);

  // Offset 12 (second char of Line 2 'a') -> PDFium char index 12.
  EXPECT_TRUE(pdf_accessibility_tree_->FindCharacterOffset(*static_text, 12,
                                                           page_char_index));
  EXPECT_EQ(12u, page_char_index.char_index);
}

TEST_P(PdfAccessibilityTreeStructuredModeTest,
       FindNodeOffset_MultiLineStaticTextWithNonAscii) {
  CreatePdfAccessibilityTree();

  // Create two text runs representing two lines in a paragraph.
  // Line 1 contains a multibyte Unicode character (right single quote '’'
  // U+2019). Line 1 ("It’s a cat ") has 11 chars starting at index 0 (13 UTF-8
  // bytes). Line 2 ("named Oliver") has 12 chars starting at index 11.
  chrome_pdf::AccessibilityTextRunInfo run1 = {
      /*start_index=*/0, /*len=*/11, gfx::RectF(0.0f, 0.0f, 100.0f, 10.0f),
      chrome_pdf::AccessibilityTextDirection::kNone,
      chrome_pdf::AccessibilityTextStyleInfo()};
  chrome_pdf::AccessibilityTextRunInfo run2 = {
      /*start_index=*/11, /*len=*/12, gfx::RectF(0.0f, 10.0f, 100.0f, 10.0f),
      chrome_pdf::AccessibilityTextDirection::kNone,
      chrome_pdf::AccessibilityTextStyleInfo()};

  text_runs_ = {run1, run2};

  constexpr std::u16string_view kText = u"It’s a cat named Oliver";
  for (char16_t c : kText) {
    chars_.push_back({static_cast<uint32_t>(c), 10.0f});
  }

  BuildAndSetAccessibilityTree();

  ui::AXNode* static_text = FindFirstStaticTextNode();
  ASSERT_NE(nullptr, static_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text->GetRole());

  int32_t out_node_id = -1;
  int32_t out_node_char_index = -1;

  // Offset 0 (start of Line 1 "It’s a cat ") -> static text char index 0.
  pdf_accessibility_tree_->FindNodeOffsetForTesting(
      /*end_of_selection=*/false, 0, 0, &out_node_id, &out_node_char_index);
  EXPECT_EQ(static_text->id(), out_node_id);
  EXPECT_EQ(0, out_node_char_index);

  // Offset 11 (start of Line 2 "named Oliver") -> static text char index 11.
  pdf_accessibility_tree_->FindNodeOffsetForTesting(
      /*end_of_selection=*/false, 0, 11, &out_node_id, &out_node_char_index);
  EXPECT_EQ(static_text->id(), out_node_id);
  EXPECT_EQ(11, out_node_char_index);

  // Offset 11 as end of selection (end of Line 1) -> static text char index 11.
  pdf_accessibility_tree_->FindNodeOffsetForTesting(
      /*end_of_selection=*/true, 0, 11, &out_node_id, &out_node_char_index);
  EXPECT_EQ(static_text->id(), out_node_id);
  EXPECT_EQ(11, out_node_char_index);

  // Offset 12 (second char of Line 2 'a') -> static text char index 12.
  pdf_accessibility_tree_->FindNodeOffsetForTesting(
      /*end_of_selection=*/false, 0, 12, &out_node_id, &out_node_char_index);
  EXPECT_EQ(static_text->id(), out_node_id);
  EXPECT_EQ(12, out_node_char_index);

  // Offset 23 as end of selection (end of Line 2) -> static text char index 23.
  pdf_accessibility_tree_->FindNodeOffsetForTesting(
      /*end_of_selection=*/true, 0, 23, &out_node_id, &out_node_char_index);
  EXPECT_EQ(static_text->id(), out_node_id);
  EXPECT_EQ(23, out_node_char_index);
}

TEST_P(PdfAccessibilityTreeStructuredModeTest,
       CreateInlineTextBoxNode_FiltersControlCharacters) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPdfAccessibilityHeuristicEnhancements);

  CreatePdfAccessibilityTree();

  // "Prac\u0002tical Cats\r\n" -> control char \u0002 should be filtered out,
  // trailing \r\n converted to ' '.
  constexpr std::string_view kRawText = "Prac\u0002tical Cats\r\n";
  chrome_pdf::AccessibilityTextRunInfo run1 = {
      /*start_index=*/0, static_cast<uint32_t>(kRawText.size()),
      gfx::RectF(0.0f, 0.0f, 100.0f, 10.0f),
      chrome_pdf::AccessibilityTextDirection::kNone,
      chrome_pdf::AccessibilityTextStyleInfo()};
  text_runs_ = {run1};

  for (char c : kRawText) {
    chars_.push_back({static_cast<uint32_t>(c), 10.0f});
  }

  BuildAndSetAccessibilityTree();

  ui::AXNode* static_text = FindFirstStaticTextNode();
  ASSERT_NE(nullptr, static_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text->GetRole());
  EXPECT_EQ(static_text->GetStringAttribute(ax::mojom::StringAttribute::kName),
            "Practical Cats ");
}

TEST_P(PdfAccessibilityTreeStructuredModeTest,
       CreateInlineTextBoxNode_ReplacesAndCollapsesNewlines) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPdfAccessibilityHeuristicEnhancements);

  CreatePdfAccessibilityTree();

  // "Dramatical Cats\r\n\r\n" -> trailing \r\n\r\n converted to ' '.
  constexpr std::string_view kRawText = "Dramatical Cats\r\n\r\n";
  chrome_pdf::AccessibilityTextRunInfo run1 = {
      /*start_index=*/0, static_cast<uint32_t>(kRawText.size()),
      gfx::RectF(0.0f, 0.0f, 100.0f, 10.0f),
      chrome_pdf::AccessibilityTextDirection::kNone,
      chrome_pdf::AccessibilityTextStyleInfo()};
  text_runs_ = {run1};

  for (char c : kRawText) {
    chars_.push_back({static_cast<uint32_t>(c), 10.0f});
  }

  BuildAndSetAccessibilityTree();

  ui::AXNode* static_text = FindFirstStaticTextNode();
  ASSERT_NE(nullptr, static_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text->GetRole());
  EXPECT_EQ(static_text->GetStringAttribute(ax::mojom::StringAttribute::kName),
            "Dramatical Cats ");
}

TEST_P(PdfAccessibilityTreeStructuredModeTest,
       CreateInlineTextBoxNode_ReplacesAndCollapsesOtherWhitespace) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPdfAccessibilityHeuristicEnhancements);

  CreatePdfAccessibilityTree();

  // "Pragmatical Cats\t\v\f" -> trailing whitespace converted and collapsed to
  // ' '.
  constexpr std::string_view kRawText = "Pragmatical Cats\t\v\f";
  chrome_pdf::AccessibilityTextRunInfo run1 = {
      /*start_index=*/0, static_cast<uint32_t>(kRawText.size()),
      gfx::RectF(0.0f, 0.0f, 100.0f, 10.0f),
      chrome_pdf::AccessibilityTextDirection::kNone,
      chrome_pdf::AccessibilityTextStyleInfo()};
  text_runs_ = {run1};

  for (char c : kRawText) {
    chars_.push_back({static_cast<uint32_t>(c), 10.0f});
  }

  BuildAndSetAccessibilityTree();

  ui::AXNode* static_text = FindFirstStaticTextNode();
  ASSERT_NE(nullptr, static_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text->GetRole());
  EXPECT_EQ(static_text->GetStringAttribute(ax::mojom::StringAttribute::kName),
            "Pragmatical Cats ");
}

INSTANTIATE_TEST_SUITE_P(All,
                         PdfAccessibilityTreeStructuredModeTest,
                         testing::Bool());

TEST_F(PdfAccessibilityTreeTest, StructureTree) {
  base::test::ScopedFeatureList pdf_tags;
  pdf_tags.InitAndEnableFeature(chrome_pdf::features::kPdfTags);
  CreatePdfAccessibilityTree();

  auto doc_structure_root =
      std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  doc_structure_root->type = chrome_pdf::PdfTagType::kDocument;

  auto page_structure =
      std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  page_structure->type = chrome_pdf::PdfTagType::kPart;

  text_runs_ = {kFirstRunMultiLine, kSecondRunMultiLine, kThirdRunMultiLine,
                kFourthRunMultiLine};
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  auto para = std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  para->type = chrome_pdf::PdfTagType::kP;
  para->associated_text_runs_if_available.push_back(&text_runs_[0]);

  // Add image to paragraph to test elements with both text and image.
  auto para_image = std::make_unique<chrome_pdf::AccessibilityImageInfo>();
  para_image->bounds = gfx::RectF(100.0f, 100.0f, 50.0f, 50.0f);
  para_image->alt_text = "Inline image";
  para_image->page_object_index = 0;
  para->associated_image_if_available = std::move(para_image);

  auto article = std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  article->type = chrome_pdf::PdfTagType::kArt;
  article->associated_text_runs_if_available.push_back(&text_runs_[1]);

  auto blockquote =
      std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  blockquote->type = chrome_pdf::PdfTagType::kBlockQuote;
  blockquote->associated_text_runs_if_available.push_back(&text_runs_[2]);

  auto heading = std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  heading->type = chrome_pdf::PdfTagType::kH1;
  heading->associated_text_runs_if_available.push_back(&text_runs_[3]);

  auto figure = std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  figure->type = chrome_pdf::PdfTagType::kFigure;
  figure->alt_text = "Test Figure";

  auto image = std::make_unique<chrome_pdf::AccessibilityImageInfo>();
  image->bounds = gfx::RectF(10.0f, 10.0f, 50.0f, 50.0f);
  image->page_object_index = 0;
  figure->associated_image_if_available = std::move(image);

  // Test empty semantic container with nested child.
  auto section = std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  section->type = chrome_pdf::PdfTagType::kSect;

  auto nested_heading =
      std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  nested_heading->type = chrome_pdf::PdfTagType::kH2;
  nested_heading->associated_text_runs_if_available.push_back(&text_runs_[3]);

  section->children.push_back(std::move(nested_heading));

  // Test non-Figure image (e.g., clickable image in Link element).
  auto link = std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  link->type = chrome_pdf::PdfTagType::kLink;

  auto link_image = std::make_unique<chrome_pdf::AccessibilityImageInfo>();
  link_image->bounds = gfx::RectF(200.0f, 200.0f, 30.0f, 30.0f);
  link_image->page_object_index = 0;
  link->associated_image_if_available = std::move(link_image);

  page_structure->children.push_back(std::move(para));
  page_structure->children.push_back(std::move(article));
  page_structure->children.push_back(std::move(blockquote));
  page_structure->children.push_back(std::move(heading));
  page_structure->children.push_back(std::move(figure));
  page_structure->children.push_back(std::move(section));
  page_structure->children.push_back(std::move(link));
  doc_structure_root->children.push_back(std::move(page_structure));

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  std::unique_ptr<chrome_pdf::AccessibilityDocInfo> doc_info =
      CreateAccessibilityDocInfo();
  doc_info->is_tagged = true;
  doc_info->structure_tree_root = std::move(doc_structure_root);

  pdf_accessibility_tree_->SetAccessibilityDocInfo(std::move(doc_info));
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);

  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(pdf_root, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  EXPECT_FALSE(
      pdf_root->HasStringAttribute(ax::mojom::StringAttribute::kLanguage));

  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(7u, page->GetChildCount());

  const ui::AXNode* para_node = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, para_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, para_node->GetRole());

  // Verify paragraph has both text and image children.
  ASSERT_GE(para_node->GetChildCount(), 2u)
      << "Paragraph should have at least 2 children (text and image)";
  bool found_text = false;
  bool found_image = false;
  for (size_t i = 0; i < para_node->GetChildCount(); ++i) {
    const ui::AXNode* child = para_node->GetChildAtIndex(i);
    ASSERT_NE(nullptr, child);
    if (child->GetRole() == ax::mojom::Role::kStaticText) {
      found_text = true;
    } else if (child->GetRole() == ax::mojom::Role::kImage) {
      found_image = true;
      EXPECT_EQ("Inline image",
                child->GetStringAttribute(ax::mojom::StringAttribute::kName));
    }
  }
  EXPECT_TRUE(found_text) << "Text node should be present in paragraph";
  EXPECT_TRUE(found_image) << "Image node should be present in paragraph";

  const ui::AXNode* article_node = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, article_node);
  EXPECT_EQ(ax::mojom::Role::kArticle, article_node->GetRole());

  const ui::AXNode* blockquote_node = page->GetChildAtIndex(2u);
  ASSERT_NE(nullptr, blockquote_node);
  EXPECT_EQ(ax::mojom::Role::kBlockquote, blockquote_node->GetRole());

  const ui::AXNode* heading_node = page->GetChildAtIndex(3u);
  ASSERT_NE(nullptr, heading_node);
  EXPECT_EQ(ax::mojom::Role::kHeading, heading_node->GetRole());

  const ui::AXNode* figure_node = page->GetChildAtIndex(4u);
  ASSERT_NE(nullptr, figure_node);
  EXPECT_EQ(ax::mojom::Role::kFigure, figure_node->GetRole());
  EXPECT_EQ("Test Figure",
            figure_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  const ui::AXNode* section_node = page->GetChildAtIndex(5u);
  ASSERT_NE(nullptr, section_node);
  EXPECT_EQ(ax::mojom::Role::kSection, section_node->GetRole());
  ASSERT_EQ(1u, section_node->GetChildCount());

  const ui::AXNode* nested_heading_node = section_node->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, nested_heading_node);
  EXPECT_EQ(ax::mojom::Role::kHeading, nested_heading_node->GetRole());

  const ui::AXNode* link_node = page->GetChildAtIndex(6u);
  ASSERT_NE(nullptr, link_node);
  EXPECT_EQ(ax::mojom::Role::kImage, link_node->GetRole());
}

TEST_F(PdfAccessibilityTreeTest, StructureTreeStyleSplittingEnabled) {
  SetUpStyleSplittingTestRunsAndChars();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {chrome_pdf::features::kPdfTags,
       ::features::kPdfAccessibilityHeuristicEnhancements},
      {});

  CreatePdfAccessibilityTree();
  ui::AXNode* paragraph_node = SetUpAccessibilityTreeForStyleSplitting();
  ASSERT_TRUE(paragraph_node);
  // When enabled, style splits create 3 static text nodes.
  ASSERT_EQ(3u, paragraph_node->GetChildCount());

  ui::AXNode* child1 = paragraph_node->GetChildAtIndex(0);
  EXPECT_EQ(ax::mojom::Role::kStaticText, child1->GetRole());
  EXPECT_EQ("abcde",
            child1->GetStringAttribute(ax::mojom::StringAttribute::kName));

  ui::AXNode* child2 = paragraph_node->GetChildAtIndex(1);
  EXPECT_EQ(ax::mojom::Role::kStaticText, child2->GetRole());
  EXPECT_EQ("fghij",
            child2->GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_TRUE(child2->data().HasTextStyle(ax::mojom::TextStyle::kBold));

  ui::AXNode* child3 = paragraph_node->GetChildAtIndex(2);
  EXPECT_EQ(ax::mojom::Role::kStaticText, child3->GetRole());
  EXPECT_EQ("klmno",
            child3->GetStringAttribute(ax::mojom::StringAttribute::kName));
}

TEST_F(PdfAccessibilityTreeTest, StructureTreeStyleSplittingDisabled) {
  SetUpStyleSplittingTestRunsAndChars();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {chrome_pdf::features::kPdfTags},
      {::features::kPdfAccessibilityHeuristicEnhancements});

  CreatePdfAccessibilityTree();
  ui::AXNode* paragraph_node = SetUpAccessibilityTreeForStyleSplitting();
  ASSERT_TRUE(paragraph_node);
  // When disabled, legacy path merges runs into a single static text node.
  ASSERT_EQ(1u, paragraph_node->GetChildCount());

  ui::AXNode* child = paragraph_node->GetChildAtIndex(0);
  EXPECT_EQ(ax::mojom::Role::kStaticText, child->GetRole());
  EXPECT_EQ("abcdefghijklmno",
            child->GetStringAttribute(ax::mojom::StringAttribute::kName));
}

TEST_F(PdfAccessibilityTreeTest, StructureTreeAbbreviationExpansion) {
  base::test::ScopedFeatureList pdf_tags;
  pdf_tags.InitAndEnableFeature(chrome_pdf::features::kPdfTags);
  CreatePdfAccessibilityTree();

  text_runs_ = {kFirstRunMultiLine, kSecondRunMultiLine, kThirdRunMultiLine,
                kFourthRunMultiLine};
  chars_.insert_range(chars_.end(), kDummyCharsData);

  auto doc_structure_root =
      std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  doc_structure_root->type = chrome_pdf::PdfTagType::kDocument;

  auto page_structure =
      std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  page_structure->type = chrome_pdf::PdfTagType::kPart;

  // Text element with abbreviation_expansion.
  auto span = std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  span->type = chrome_pdf::PdfTagType::kP;
  span->associated_text_runs_if_available.push_back(text_runs_.data());
  span->abbreviation_expansion = "Portable Document Format";

  page_structure->children.push_back(std::move(span));
  doc_structure_root->children.push_back(std::move(page_structure));

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  std::unique_ptr<chrome_pdf::AccessibilityDocInfo> doc_info =
      CreateAccessibilityDocInfo();
  doc_info->is_tagged = true;
  doc_info->structure_tree_root = std::move(doc_structure_root);

  pdf_accessibility_tree_->SetAccessibilityDocInfo(std::move(doc_info));
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  WaitForThreadDelayedTasks();

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(pdf_root, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(1u, page->GetChildCount());

  // abbreviation_expansion maps to kDescription.
  const ui::AXNode* span_node = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, span_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, span_node->GetRole());
  EXPECT_EQ(
      "Portable Document Format",
      span_node->GetStringAttribute(ax::mojom::StringAttribute::kDescription));
}

TEST_F(PdfAccessibilityTreeTest, DocumentLanguageOnRootNode) {
  base::test::ScopedFeatureList pdf_tags;
  pdf_tags.InitAndEnableFeature(chrome_pdf::features::kPdfTags);
  CreatePdfAccessibilityTree();

  auto doc_structure_root =
      std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  doc_structure_root->type = chrome_pdf::PdfTagType::kDocument;
  doc_structure_root->language = "en-US";

  auto page_structure =
      std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  page_structure->type = chrome_pdf::PdfTagType::kPart;

  text_runs_ = {kFirstRunMultiLine, kSecondRunMultiLine, kThirdRunMultiLine,
                kFourthRunMultiLine};
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  auto para = std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  para->type = chrome_pdf::PdfTagType::kP;
  para->associated_text_runs_if_available.push_back(&text_runs_[0]);

  page_structure->children.push_back(std::move(para));
  doc_structure_root->children.push_back(std::move(page_structure));

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  std::unique_ptr<chrome_pdf::AccessibilityDocInfo> doc_info =
      CreateAccessibilityDocInfo();
  doc_info->is_tagged = true;
  doc_info->structure_tree_root = std::move(doc_structure_root);

  pdf_accessibility_tree_->SetAccessibilityDocInfo(std::move(doc_info));
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);

  WaitForThreadTasks();
  WaitForThreadDelayedTasks();

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(pdf_root, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);
  EXPECT_EQ("en-US", pdf_root->GetStringAttribute(
                         ax::mojom::StringAttribute::kLanguage));
}

TEST_F(PdfAccessibilityTreeTest, StructureTreeRootAttributes) {
  base::test::ScopedFeatureList pdf_tags;
  pdf_tags.InitAndEnableFeature(chrome_pdf::features::kPdfTags);
  CreatePdfAccessibilityTree();

  // Structure tree:
  //   kDocument -> kPart -> kDocument(lang="es", alt="...") -> kP -> text.
  auto doc_structure_root =
      std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  doc_structure_root->type = chrome_pdf::PdfTagType::kDocument;

  auto page_structure =
      std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  page_structure->type = chrome_pdf::PdfTagType::kPart;

  auto pdf_doc = std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  pdf_doc->type = chrome_pdf::PdfTagType::kDocument;
  pdf_doc->language = "es";
  pdf_doc->alt_text = "Document description";

  text_runs_ = {kFirstTextRun, kSecondTextRun};
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  auto para = std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  para->type = chrome_pdf::PdfTagType::kP;
  para->associated_text_runs_if_available.push_back(&text_runs_[0]);

  pdf_doc->children.push_back(std::move(para));
  page_structure->children.push_back(std::move(pdf_doc));
  doc_structure_root->children.push_back(std::move(page_structure));

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  std::unique_ptr<chrome_pdf::AccessibilityDocInfo> doc_info =
      CreateAccessibilityDocInfo();
  doc_info->is_tagged = true;
  doc_info->structure_tree_root = std::move(doc_structure_root);

  pdf_accessibility_tree_->SetAccessibilityDocInfo(std::move(doc_info));
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);

  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(pdf_root, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);

  // Tagged PDFs have a /Document element at the root of their structure tree
  // which gets mapped to kGenericContainer to avoid introducing a redundant
  // Document node in the accessibility tree.
  ASSERT_EQ(1u, page->GetChildCount());
  const ui::AXNode* container = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, container);
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, container->GetRole());

  // The container node holds the kDocument attributes.
  EXPECT_EQ("es", container->GetStringAttribute(
                      ax::mojom::StringAttribute::kLanguage));
  EXPECT_EQ(
      "Document description",
      container->GetStringAttribute(ax::mojom::StringAttribute::kDescription));
}

TEST_F(PdfAccessibilityTreeTest, PartiallyTaggedPdfPreservesSemanticStructure) {
  base::test::ScopedFeatureList pdf_tags;
  pdf_tags.InitAndEnableFeature(chrome_pdf::features::kPdfTags);
  CreatePdfAccessibilityTree();

  // Create 5 text runs.
  // text_run[0]: untagged, at beginning - contains "0000000"
  // text_run[1]: tagged, in first list item - contains "1111111"
  // text_run[2]: untagged, after tagged run - contains "2222222"
  // text_run[3]: tagged, in second list item - contains "3333333"
  // text_run[4]: tagged, in second list item - contains "4444444"
  constexpr size_t kTotalRuns = 5;
  for (size_t i = 0; i < kTotalRuns; ++i) {
    text_runs_.push_back(kFirstRunMultiLine);
    text_runs_.back().start_index = i * kFirstRunMultiLine.len;
  }
  // Create characters with each text run having its index repeated.
  for (size_t i = 0; i < kTotalRuns * kFirstRunMultiLine.len; ++i) {
    char digit = '0' + (i / kFirstRunMultiLine.len);
    chars_.push_back({static_cast<uint32_t>(digit), 10});
  }

  // Build structure tree with a list containing 2 list items.
  auto doc_structure_root =
      std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  doc_structure_root->type = chrome_pdf::PdfTagType::kDocument;

  auto page_structure =
      std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  page_structure->type = chrome_pdf::PdfTagType::kPart;

  // Set unassociated text run ranges for text_run[0] and text_run[2].
  page_structure->unassociated_text_run_ranges_for_page.push_back({0, 0});
  page_structure->unassociated_text_run_ranges_for_page.push_back({2, 2});

  auto list = std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  list->type = chrome_pdf::PdfTagType::kL;

  // First list item contains text_run[1].
  auto list_item1 =
      std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  list_item1->type = chrome_pdf::PdfTagType::kLI;
  list_item1->associated_text_runs_if_available.push_back(&text_runs_[1]);

  // Second list item contains text_run[3] and text_run[4].
  auto list_item2 =
      std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  list_item2->type = chrome_pdf::PdfTagType::kLI;
  list_item2->associated_text_runs_if_available.push_back(&text_runs_[3]);
  list_item2->associated_text_runs_if_available.push_back(&text_runs_[4]);

  // text_run[0] and text_run[2] are intentionally not tagged.

  list->children.push_back(std::move(list_item1));
  list->children.push_back(std::move(list_item2));
  page_structure->children.push_back(std::move(list));
  doc_structure_root->children.push_back(std::move(page_structure));

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  std::unique_ptr<chrome_pdf::AccessibilityDocInfo> doc_info =
      CreateAccessibilityDocInfo();
  doc_info->is_tagged = true;
  doc_info->structure_tree_root = std::move(doc_structure_root);

  pdf_accessibility_tree_->SetAccessibilityDocInfo(std::move(doc_info));
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);

  WaitForThreadTasks();
  WaitForThreadDelayedTasks();

  const ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(pdf_root, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  const ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);

  // Expected structure:
  // Page
  //   - Paragraph (for text_run[0] - untagged)
  //   - List
  //     - ListItem 1 (contains text_run[1] and text_run[2] - untagged)
  //     - ListItem 2 (contains text_run[3] and text_run[4])
  ASSERT_EQ(2u, page->GetChildCount());

  // First child: paragraph containing text_run[0].
  const ui::AXNode* first_para = page->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, first_para);
  EXPECT_EQ(ax::mojom::Role::kParagraph, first_para->GetRole());
  ASSERT_EQ(1u, first_para->GetChildCount());
  const ui::AXNode* first_para_static_text = first_para->GetChildAtIndex(0u);
  EXPECT_EQ(ax::mojom::Role::kStaticText, first_para_static_text->GetRole());
  EXPECT_EQ(1u, first_para_static_text->GetChildCount());

  // Second child: list with 2 list items.
  const ui::AXNode* list_node = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, list_node);
  EXPECT_EQ(ax::mojom::Role::kList, list_node->GetRole());
  ASSERT_EQ(2u, list_node->GetChildCount());

  // First child of list: list item 1 containing text_run[1] and text_run[2].
  const ui::AXNode* list_item1_node = list_node->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, list_item1_node);
  EXPECT_EQ(ax::mojom::Role::kListItem, list_item1_node->GetRole());
  ASSERT_EQ(1u, list_item1_node->GetChildCount());
  const ui::AXNode* list_item1_static_text =
      list_item1_node->GetChildAtIndex(0u);
  EXPECT_EQ(ax::mojom::Role::kStaticText, list_item1_static_text->GetRole());
  // List item 1 contains 2 inline text boxes (text_run[1] and text_run[2]).
  EXPECT_EQ(2u, list_item1_static_text->GetChildCount());

  // Second child of list: list item 2 containing text_run[3] and text_run[4].
  const ui::AXNode* list_item2_node = list_node->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, list_item2_node);
  EXPECT_EQ(ax::mojom::Role::kListItem, list_item2_node->GetRole());
  ASSERT_EQ(1u, list_item2_node->GetChildCount());
  const ui::AXNode* list_item2_static_text =
      list_item2_node->GetChildAtIndex(0u);
  EXPECT_EQ(ax::mojom::Role::kStaticText, list_item2_static_text->GetRole());
  // List item 2 contains 2 inline text boxes (text_run[3] and text_run[4]).
  EXPECT_EQ(2u, list_item2_static_text->GetChildCount());

  // Verify total count of inline text boxes equals total runs.
  size_t total_inline_text_box_count = first_para_static_text->GetChildCount() +
                                       list_item1_static_text->GetChildCount() +
                                       list_item2_static_text->GetChildCount();
  EXPECT_EQ(kTotalRuns, total_inline_text_box_count);
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  /*
   * Expected tree structure
   * Document
   * ++ Region
   * ++++ Paragraph
   * ++++++ Link
   * ++++++ Link
   * ++++++ Static Text
   */

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(root_node->GetChildCount(), 1u);
  ui::AXNode* page_node = root_node->GetChildAtIndex(1);
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  ASSERT_EQ(1u, page_node->GetChildCount());

  ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>& child_nodes =
      paragraph_node->GetAllChildren();
  ASSERT_EQ(3u, child_nodes.size());

  ui::AXNode* link_node = child_nodes[0];
  ASSERT_TRUE(link_node);
  EXPECT_EQ(kChromiumTestUrl,
            link_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
  EXPECT_EQ(ax::mojom::Role::kLink, link_node->GetRole());
  EXPECT_EQ(gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f),
            link_node->data().relative_bounds.bounds);
  ASSERT_EQ(1u, link_node->GetChildCount());

  link_node = child_nodes[1];
  ASSERT_TRUE(link_node);
  EXPECT_EQ(kChromiumTestUrl,
            link_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
  EXPECT_EQ(ax::mojom::Role::kLink, link_node->GetRole());
  EXPECT_EQ(gfx::RectF(1.0f, 2.0f, 5.0f, 6.0f),
            link_node->data().relative_bounds.bounds);
  ASSERT_EQ(1u, link_node->GetChildCount());

  ui::AXNode* static_text_node = child_nodes[2];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->GetChildCount());
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

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

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(root_node->GetChildCount(), 1u);
  ui::AXNode* page_node = root_node->GetChildAtIndex(1);
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  ASSERT_EQ(1u, page_node->GetChildCount());

  ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  ASSERT_EQ(1u, paragraph_node->GetChildCount());

  ui::AXNode* highlight_node = paragraph_node->GetChildAtIndex(0);
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
  ASSERT_EQ(2u, highlight_node->GetChildCount());

  ui::AXNode* static_text_node = highlight_node->GetChildAtIndex(0);
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(2u, static_text_node->GetChildCount());

  ui::AXNode* popup_note_node = highlight_node->GetChildAtIndex(1);
  ASSERT_TRUE(popup_note_node);
  EXPECT_EQ(ax::mojom::Role::kNote, popup_note_node->GetRole());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_AX_ROLE_DESCRIPTION_PDF_POPUP_NOTE),
            popup_note_node->GetStringAttribute(
                ax::mojom::StringAttribute::kRoleDescription));
  EXPECT_EQ(gfx::RectF(1.0f, 1.0f, 5.0f, 6.0f),
            popup_note_node->data().relative_bounds.bounds);
  ASSERT_EQ(1u, popup_note_node->GetChildCount());

  ui::AXNode* static_popup_note_text_node = popup_note_node->GetChildAtIndex(0);
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

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

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(root_node->GetChildCount(), 1u);
  ui::AXNode* page_node = root_node->GetChildAtIndex(1);
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  ASSERT_EQ(1u, page_node->GetChildCount());

  ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  EXPECT_TRUE(paragraph_node->GetBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject));
  ASSERT_EQ(2u, paragraph_node->GetChildCount());

  ui::AXNode* static_text_node = paragraph_node->GetChildAtIndex(0);
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  EXPECT_EQ(ax::mojom::NameFrom::kContents, static_text_node->GetNameFrom());
  ASSERT_EQ(2u, static_text_node->GetChildCount());

  ui::AXNode* previous_inline_node = static_text_node->GetChildAtIndex(0);
  ASSERT_TRUE(previous_inline_node);
  EXPECT_EQ(ax::mojom::Role::kInlineTextBox, previous_inline_node->GetRole());
  EXPECT_EQ(ax::mojom::NameFrom::kContents,
            previous_inline_node->GetNameFrom());
  ASSERT_FALSE(previous_inline_node->HasIntAttribute(
      ax::mojom::IntAttribute::kPreviousOnLineId));

  ui::AXNode* next_inline_node = static_text_node->GetChildAtIndex(1);
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

  ui::AXNode* link_node = paragraph_node->GetChildAtIndex(1);
  ASSERT_TRUE(link_node);
  EXPECT_EQ(kChromiumTestUrl,
            link_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
  EXPECT_EQ(ax::mojom::Role::kLink, link_node->GetRole());
  ASSERT_EQ(1u, link_node->GetChildCount());

  static_text_node = link_node->GetChildAtIndex(0);
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  EXPECT_EQ(ax::mojom::NameFrom::kContents, static_text_node->GetNameFrom());
  ASSERT_EQ(2u, static_text_node->GetChildCount());

  previous_inline_node = static_text_node->GetChildAtIndex(0);
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

  next_inline_node = static_text_node->GetChildAtIndex(1);
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_FALSE(pdf_accessibility_tree_->GetRoot());
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_FALSE(pdf_accessibility_tree_->GetRoot());
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_FALSE(pdf_accessibility_tree_->GetRoot());
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_FALSE(pdf_accessibility_tree_->GetRoot());
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_FALSE(pdf_accessibility_tree_->GetRoot());
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_FALSE(pdf_accessibility_tree_->GetRoot());
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  // In case of invalid data, only the initialized data should be in the tree.
  ASSERT_FALSE(pdf_accessibility_tree_->GetRoot());
}

TEST_F(PdfAccessibilityTreeTest, TestActionDataConversion) {
  // This test verifies the AXActionData conversion to
  // `chrome_pdf::AccessibilityActionData`.
  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      pdf_accessibility_tree_->CreateActionTarget(root_node->data().id);
  ASSERT_TRUE(pdf_action_target);
  EXPECT_EQ(ui::AXActionTarget::Type::kPdf, pdf_action_target->GetType());
  EXPECT_TRUE(pdf_action_target->ScrollToMakeVisibleWithSubFocus(
      gfx::Rect(0, 0, 50, 50), ax::mojom::ScrollAlignment::kScrollAlignmentLeft,
      ax::mojom::ScrollAlignment::kScrollAlignmentTop,
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible));
  chrome_pdf::AccessibilityActionData action_data =
      action_handler_.received_action_data();
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
  action_data = action_handler_.received_action_data();
  EXPECT_EQ(chrome_pdf::AccessibilityScrollAlignment::kRight,
            action_data.horizontal_scroll_alignment);

  EXPECT_TRUE(pdf_action_target->ScrollToMakeVisibleWithSubFocus(
      gfx::Rect(0, 0, 50, 50),
      ax::mojom::ScrollAlignment::kScrollAlignmentBottom,
      ax::mojom::ScrollAlignment::kScrollAlignmentBottom,
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible));
  action_data = action_handler_.received_action_data();
  EXPECT_EQ(chrome_pdf::AccessibilityScrollAlignment::kBottom,
            action_data.horizontal_scroll_alignment);

  EXPECT_TRUE(pdf_action_target->ScrollToMakeVisibleWithSubFocus(
      gfx::Rect(0, 0, 50, 50),
      ax::mojom::ScrollAlignment::kScrollAlignmentCenter,
      ax::mojom::ScrollAlignment::kScrollAlignmentClosestEdge,
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible));
  action_data = action_handler_.received_action_data();
  EXPECT_EQ(chrome_pdf::AccessibilityScrollAlignment::kCenter,
            action_data.horizontal_scroll_alignment);
  EXPECT_EQ(chrome_pdf::AccessibilityScrollAlignment::kClosestToEdge,
            action_data.vertical_scroll_alignment);
  EXPECT_EQ(gfx::Rect({0, 0}, {1, 1}), action_data.target_rect);
}

TEST_F(PdfAccessibilityTreeTest, TestScrollToGlobalPointDataConversion) {
  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      pdf_accessibility_tree_->CreateActionTarget(root_node->data().id);
  ASSERT_TRUE(pdf_action_target);
  EXPECT_EQ(ui::AXActionTarget::Type::kPdf, pdf_action_target->GetType());
  {
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kScrollToPoint;
    action_data.target_point = gfx::Point(50, 50);
    EXPECT_TRUE(pdf_action_target->PerformAction(action_data));
  }

  chrome_pdf::AccessibilityActionData action_data =
      action_handler_.received_action_data();
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(root_node->GetChildCount(), 1u);
  ui::AXNode* page_node = root_node->GetChildAtIndex(1);
  ASSERT_NE(nullptr, page_node);
  ASSERT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());

  const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>& para_nodes =
      page_node->GetAllChildren();
  ASSERT_EQ(2u, para_nodes.size());
  const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>& link_nodes =
      para_nodes[1]->GetAllChildren();
  ASSERT_EQ(1u, link_nodes.size());

  const ui::AXNode* link_node = link_nodes[0];
  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      pdf_accessibility_tree_->CreateActionTarget(link_node->data().id);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf, pdf_action_target->GetType());
  {
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kDoDefault;
    pdf_action_target->PerformAction(action_data);
  }
  chrome_pdf::AccessibilityActionData pdf_action_data =
      action_handler_.received_action_data();

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
  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      pdf_accessibility_tree_->CreateActionTarget(root_node->data().id);
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

  EXPECT_FALSE(pdf_action_target->ScrollToMakeVisible());
}

TEST_F(PdfAccessibilityTreeTest, TestZoomAndScaleChanges) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  viewport_info_.zoom = 1.0;
  viewport_info_.scale = 1.0;
  viewport_info_.scroll = gfx::Point(0, -56);
  viewport_info_.offset = gfx::Point(57, 0);

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  WaitForThreadTasks();

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);
  ASSERT_GT(root_node->GetChildCount(), 1u);
  ui::AXNode* page_node = root_node->GetChildAtIndex(1);
  ASSERT_TRUE(page_node);
  ASSERT_EQ(2u, page_node->GetChildCount());
  ui::AXNode* para_node = page_node->GetChildAtIndex(0);
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
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);
  ASSERT_GT(root_node->GetChildCount(), 1u);
  ui::AXNode* page_node = root_node->GetChildAtIndex(1);
  ASSERT_NE(nullptr, page_node);
  ASSERT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>& para_nodes =
      page_node->GetAllChildren();
  ASSERT_EQ(2u, para_nodes.size());
  ASSERT_TRUE(para_nodes[0]);
  const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>&
      static_text_nodes1 = para_nodes[0]->GetAllChildren();
  ASSERT_EQ(1u, static_text_nodes1.size());
  ASSERT_TRUE(static_text_nodes1[0]);
  const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>&
      inline_text_nodes1 = static_text_nodes1[0]->GetAllChildren();
  ASSERT_TRUE(inline_text_nodes1[0]);
  ASSERT_EQ(1u, inline_text_nodes1.size());
  ASSERT_TRUE(para_nodes[1]);
  const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>&
      static_text_nodes2 = para_nodes[1]->GetAllChildren();
  ASSERT_EQ(1u, static_text_nodes2.size());
  ASSERT_TRUE(static_text_nodes2[0]);
  const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>&
      inline_text_nodes2 = static_text_nodes2[0]->GetAllChildren();
  ASSERT_TRUE(inline_text_nodes2[0]);
  ASSERT_EQ(1u, inline_text_nodes2.size());

  std::unique_ptr<ui::AXActionTarget> pdf_anchor_action_target =
      pdf_accessibility_tree_->CreateActionTarget(
          inline_text_nodes1[0]->data().id);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf,
            pdf_anchor_action_target->GetType());
  std::unique_ptr<ui::AXActionTarget> pdf_focus_action_target =
      pdf_accessibility_tree_->CreateActionTarget(
          inline_text_nodes2[0]->data().id);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf, pdf_focus_action_target->GetType());
  EXPECT_TRUE(pdf_anchor_action_target->SetSelection(
      pdf_anchor_action_target.get(), 1, pdf_focus_action_target.get(), 5));

  chrome_pdf::AccessibilityActionData pdf_action_data =
      action_handler_.received_action_data();
  EXPECT_EQ(chrome_pdf::AccessibilityAction::kSetSelection,
            pdf_action_data.action);
  EXPECT_EQ(0u, pdf_action_data.selection_start_index.page_index);
  EXPECT_EQ(1u, pdf_action_data.selection_start_index.char_index);
  EXPECT_EQ(0u, pdf_action_data.selection_end_index.page_index);
  EXPECT_EQ(20u, pdf_action_data.selection_end_index.char_index);

  // Verify selection offsets in tree data.
  ui::AXTreeData tree_data;
  pdf_accessibility_tree_->GetTreeData(&tree_data);
  EXPECT_EQ(static_text_nodes1[0]->id(), tree_data.sel_anchor_object_id);
  EXPECT_EQ(0, tree_data.sel_anchor_offset);
  EXPECT_EQ(static_text_nodes1[0]->id(), tree_data.sel_focus_object_id);
  EXPECT_EQ(0, tree_data.sel_focus_offset);

  pdf_anchor_action_target = pdf_accessibility_tree_->CreateActionTarget(
      static_text_nodes1[0]->data().id);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf,
            pdf_anchor_action_target->GetType());
  pdf_focus_action_target = pdf_accessibility_tree_->CreateActionTarget(
      inline_text_nodes2[0]->data().id);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf, pdf_focus_action_target->GetType());
  EXPECT_TRUE(pdf_anchor_action_target->SetSelection(
      pdf_anchor_action_target.get(), 1, pdf_focus_action_target.get(), 4));

  pdf_action_data = action_handler_.received_action_data();
  EXPECT_EQ(chrome_pdf::AccessibilityAction::kSetSelection,
            pdf_action_data.action);
  EXPECT_EQ(0u, pdf_action_data.selection_start_index.page_index);
  EXPECT_EQ(1u, pdf_action_data.selection_start_index.char_index);
  EXPECT_EQ(0u, pdf_action_data.selection_end_index.page_index);
  EXPECT_EQ(19u, pdf_action_data.selection_end_index.char_index);

  pdf_anchor_action_target =
      pdf_accessibility_tree_->CreateActionTarget(para_nodes[0]->data().id);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf,
            pdf_anchor_action_target->GetType());
  pdf_focus_action_target =
      pdf_accessibility_tree_->CreateActionTarget(para_nodes[1]->data().id);
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  ASSERT_TRUE(root_node);

  std::unique_ptr<ui::AXActionTarget> pdf_action_target =
      pdf_accessibility_tree_->CreateActionTarget(root_node->data().id);
  ASSERT_EQ(ui::AXActionTarget::Type::kPdf, pdf_action_target->GetType());
  {
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kShowContextMenu;

    // This PDF accessibility tree is attached to a body element.
    EXPECT_TRUE(pdf_action_target->PerformAction(action_data));
  }
}

TEST_F(PdfAccessibilityTreeTest, StitchChildTreeAction) {
  CreatePdfAccessibilityTree();
  text_runs_ = {kFirstTextRun, kSecondTextRun};
  chars_ = {std::begin(kDummyCharsData), std::end(kDummyCharsData)};
  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();
  chrome_pdf::AccessibilityImageInfo fake_image = CreateMockInaccessibleImage();
  fake_image.text_run_index = 1u;
  fake_image.page_object_index = 0u;
  page_objects_.images.push_back(fake_image);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);

  ui::AXNode fake_root(&pdf_accessibility_tree_->tree_for_testing(),
                       /*parent=*/nullptr,
                       /*id=*/1,
                       /*index_in_parent=*/0u);
  auto child_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kStitchChildTree;
  action_data.target_tree_id =
      pdf_accessibility_tree_->tree_for_testing().data().tree_id;
  action_data.target_node_id = fake_root.id();
  action_data.child_tree_id = child_tree_id;
  {
    std::unique_ptr<ui::AXActionTarget> pdf_action_target =
        pdf_accessibility_tree_->CreateActionTarget(fake_root.id());

    // This is a fake node, so no action was created.
    ASSERT_EQ(ui::AXActionTarget::Type::kNull, pdf_action_target->GetType());
    ASSERT_EQ(nullptr, pdf_accessibility_tree_->GetRoot());
    EXPECT_FALSE(pdf_action_target->PerformAction(action_data))
        << "PDF must first be fully loaded.";
  }

  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  ui::AXNode* pdf_root = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(pdf_root, page_count_,
                          /*is_pdf_ocr_test=*/false, /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(pdf_root->GetChildCount(), 1u);
  ui::AXNode* page = pdf_root->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, page);
  ASSERT_EQ(2u, page->GetChildCount());

  ui::AXNode* paragraph = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, paragraph);
  ASSERT_EQ(2u, paragraph->GetChildCount());

  ui::AXNode* image = paragraph->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, image);
  ASSERT_EQ(ax::mojom::Role::kImage, image->GetRole());

  std::unique_ptr<ui::AXTreeManager> child_tree_manager;
  {
    //
    // Set up a child tree that will be stitched into the PDF making the above
    // `image` invisible.
    //

    ui::AXNodeData root;
    root.id = 1;
    ui::AXNodeData button;
    button.id = 2;
    ui::AXNodeData static_text;
    static_text.id = 3;
    ui::AXNodeData inline_box;
    inline_box.id = 4;

    root.role = ax::mojom::Role::kRootWebArea;
    root.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                          true);
    root.child_ids = {button.id};

    button.role = ax::mojom::Role::kButton;
    button.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                            true);
    button.SetName("Button");
    // Name is not visible in the tree's text representation, i.e. it may be
    // coming from an aria-label.
    button.SetNameFrom(ax::mojom::NameFrom::kAttribute);
    button.relative_bounds.bounds = gfx::RectF(20, 20, 200, 30);
    button.child_ids = {static_text.id};

    static_text.role = ax::mojom::Role::kStaticText;
    static_text.SetName("Button's visible text");
    static_text.child_ids = {inline_box.id};

    inline_box.role = ax::mojom::Role::kInlineTextBox;
    inline_box.SetName("Button's visible text");

    ui::AXTreeUpdate update;
    update.root_id = root.id;
    update.nodes = {root, button, static_text, inline_box};
    update.has_tree_data = true;
    update.tree_data.tree_id = child_tree_id;
    update.tree_data.parent_tree_id =
        pdf_accessibility_tree_->tree_for_testing().GetAXTreeID();
    update.tree_data.title = "Generated content";

    auto child_tree = std::make_unique<ui::AXTree>(update);
    child_tree_manager =
        std::make_unique<ui::AXTreeManager>(std::move(child_tree));
  }

  action_data.target_node_id = paragraph->id();
  {
    std::unique_ptr<ui::AXActionTarget> pdf_action_target =
        pdf_accessibility_tree_->CreateActionTarget(paragraph->data().id);
    ASSERT_EQ(ui::AXActionTarget::Type::kPdf, pdf_action_target->GetType());
    EXPECT_TRUE(pdf_action_target->PerformAction(action_data));
  }

  // Fetch `paragraph` again since its pointer would have been invalidated.
  paragraph = page->GetChildAtIndex(1u);
  ASSERT_NE(nullptr, paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, paragraph->GetRole());
  EXPECT_EQ(child_tree_id.ToString(),
            paragraph->data().GetStringAttribute(
                ax::mojom::StringAttribute::kChildTreeId));
  EXPECT_EQ(1u, paragraph->GetChildCountCrossingTreeBoundary());

  const ui::AXNode* child_root =
      paragraph->GetChildAtIndexCrossingTreeBoundary(0u);
  ASSERT_NE(nullptr, child_root);
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, child_root->GetRole());
  const ui::AXNode* button = child_root->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, button);
  EXPECT_EQ(ax::mojom::Role::kButton, button->GetRole());
  const ui::AXNode* static_text = button->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, static_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text->GetRole());
  const ui::AXNode* inline_box = static_text->GetChildAtIndex(0u);
  ASSERT_NE(nullptr, inline_box);
  EXPECT_EQ(ax::mojom::Role::kInlineTextBox, inline_box->GetRole());
  EXPECT_EQ(0u, inline_box->GetChildCount());
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
using PdfOcrTest = PdfAccessibilityTreeTest;

TEST_F(PdfOcrTest, CheckLiveRegionPoliteStatus) {
  CreatePdfAccessibilityTree();

  page_objects_.images.push_back(CreateMockInaccessibleImage());

  // Get and use the underlying AXTree to create an AXEventGenerator. This
  // event generator is usually instrumented in the test.
  ui::AXTree& tree = pdf_accessibility_tree_->tree_for_testing();
  ui::AXEventGenerator event_generator(&tree);
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  WaitForThreadTasks();

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  ASSERT_NE(nullptr, root_node);
  EXPECT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());
  ASSERT_EQ(1u, root_node->GetChildCount());

  ui::AXNode* status_wrapper_node = root_node->GetChildAtIndex(0);
  ASSERT_NE(nullptr, status_wrapper_node);
  EXPECT_EQ(ax::mojom::Role::kBanner, status_wrapper_node->GetRole());
  ASSERT_EQ(1u, status_wrapper_node->GetChildCount());

  ui::AXNode* status_node = status_wrapper_node->GetChildAtIndex(0);
  ASSERT_NE(nullptr, status_node);
  EXPECT_EQ(ax::mojom::Role::kStatus, status_node->GetRole());
  EXPECT_EQ(1u, status_node->GetChildCount());
  EXPECT_TRUE(
      status_node->GetBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic));
  constexpr char kDefaultLiveRegionRelevant[] = "additions text";
  EXPECT_EQ(kDefaultLiveRegionRelevant,
            status_node->GetStringAttribute(
                ax::mojom::StringAttribute::kLiveRelevant));
  constexpr char kStatusLiveRegion[] = "polite";
  EXPECT_EQ(kStatusLiveRegion, status_node->GetStringAttribute(
                                   ax::mojom::StringAttribute::kLiveStatus));
  EXPECT_TRUE(status_node->GetBoolAttribute(
      ax::mojom::BoolAttribute::kContainerLiveAtomic));
  EXPECT_EQ(kDefaultLiveRegionRelevant,
            status_node->GetStringAttribute(
                ax::mojom::StringAttribute::kContainerLiveRelevant));
  EXPECT_EQ(kStatusLiveRegion,
            status_node->GetStringAttribute(
                ax::mojom::StringAttribute::kContainerLiveStatus));

  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(ui::AXEventGenerator::Event::SUBTREE_CREATED,
                         root_node->id()),
          HasEventAtNode(ui::AXEventGenerator::Event::LIVE_REGION_CREATED,
                         status_node->id())));

  page_info_.page_index = 0;
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  uint32_t pages_plus_status_node_count = page_count_ + 1u;
  ASSERT_EQ(pages_plus_status_node_count, root_node->GetChildCount());

  ui::AXNode* page_node = root_node->GetChildAtIndex(1);
  ASSERT_NE(nullptr, page_node);
  ASSERT_EQ(1u, page_node->GetChildCount());

  ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
  ASSERT_NE(nullptr, paragraph_node);
  ASSERT_EQ(1u, paragraph_node->GetChildCount());

  ui::AXNode* image_node = paragraph_node->GetChildAtIndex(0);
  ASSERT_NE(nullptr, image_node);

  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(ui::AXEventGenerator::Event::SUBTREE_CREATED,
                         root_node->id()),
          HasEventAtNode(ui::AXEventGenerator::Event::CHILDREN_CHANGED,
                         root_node->id()),
          HasEventAtNode(ui::AXEventGenerator::Event::LIVE_REGION_CREATED,
                         status_node->id()),
          HasEventAtNode(ui::AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED,
                         status_node->id()),
          HasEventAtNode(ui::AXEventGenerator::Event::NAME_CHANGED,
                         status_node->id()),
          HasEventAtNode(ui::AXEventGenerator::Event::NAME_CHANGED,
                         status_node->data().child_ids[0])));
}

TEST_F(PdfOcrTest, CheckLiveRegionNotSetWhenInBackground) {
  CreatePdfAccessibilityTree();
  // Simulate going to the background.
  pdf_accessibility_tree_->WasHidden();

  page_objects_.images.push_back(CreateMockInaccessibleImage());
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  WaitForThreadTasks();

  const ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  ASSERT_NE(nullptr, root_node);
  EXPECT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());
  ASSERT_EQ(1u, root_node->GetChildCount());

  const ui::AXNode* status_wrapper_node = root_node->GetChildAtIndex(0);
  ASSERT_NE(nullptr, status_wrapper_node);
  EXPECT_EQ(ax::mojom::Role::kBanner, status_wrapper_node->GetRole());
  ASSERT_EQ(1u, status_wrapper_node->GetChildCount());

  const ui::AXNode* status_node = status_wrapper_node->GetChildAtIndex(0);
  ASSERT_NE(nullptr, status_node);
  EXPECT_EQ(ax::mojom::Role::kStatus, status_node->GetRole());
  EXPECT_EQ(1u, status_node->GetChildCount());
  EXPECT_FALSE(
      status_node->HasBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic));
  EXPECT_FALSE(status_node->HasStringAttribute(
      ax::mojom::StringAttribute::kLiveRelevant));
  EXPECT_FALSE(
      status_node->HasStringAttribute(ax::mojom::StringAttribute::kLiveStatus));
  EXPECT_FALSE(status_node->HasBoolAttribute(
      ax::mojom::BoolAttribute::kContainerLiveAtomic));
  EXPECT_FALSE(status_node->HasStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant));
  EXPECT_FALSE(status_node->HasStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus));
}

TEST_F(PdfOcrTest, FeatureNotificationOnInaccessiblePdf) {
  CreatePdfAccessibilityTree();

  page_objects_.images.push_back(CreateMockInaccessibleImage());

  // Get and use the underlying AXTree to create an AXEventGenerator. This
  // event generator is usually instrumented in the test.
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  WaitForThreadTasks();

  page_info_.page_index = 0;
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  const ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, page_count_,
                          /*is_pdf_ocr_test=*/true,
                          /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);
}

TEST_F(PdfOcrTest, NoFeatureNotificationOnAccessiblePdf) {
  text_runs_.emplace_back(kFirstTextRun);
  text_runs_.emplace_back(kSecondTextRun);
  chars_.insert(chars_.end(), std::begin(kDummyCharsData),
                std::end(kDummyCharsData));

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();
  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(
      CreateAccessibilityDocInfo());
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  const ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  // `is_pdf_ocr_test` needs to be set to false below, as it shouldn't announce
  // the PDF OCR feature notification in this case.
  CheckRootAndStatusNodes(root_node, page_count_,
                          /*is_pdf_ocr_test=*/false,
                          /*is_ocr_completed=*/false,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(root_node->GetChildCount(), 1u);
  const ui::AXNode* page_node = root_node->GetChildAtIndex(1);
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  ASSERT_EQ(2u, page_node->GetChildCount());

  ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  EXPECT_TRUE(paragraph_node->GetBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject));
  ASSERT_EQ(1u, paragraph_node->GetChildCount());

  ui::AXNode* static_text_node = paragraph_node->GetChildAtIndex(0);
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->GetChildCount());

  paragraph_node = page_node->GetChildAtIndex(1);
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  EXPECT_TRUE(paragraph_node->GetBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject));
  ASSERT_EQ(1u, paragraph_node->GetChildCount());

  static_text_node = paragraph_node->GetChildAtIndex(0);
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->GetChildCount());
}

#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

}  // namespace pdf
