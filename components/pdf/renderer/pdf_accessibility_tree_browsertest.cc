// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/pdf/renderer/pdf_accessibility_tree.h"

#include <map>
#include <memory>

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
#include "pdf/pdf_accessibility_image_fetcher.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
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
#include <tuple>

#include "base/containers/queue.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"  // nogncheck crbug.com/1125897
#include "services/screen_ai/public/test/fake_screen_ai_annotator.h"  // nogncheck crbug.com/40147906
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/geometry/transform.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

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

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
ui::AXTreeUpdate CreateMockOCRResult(const gfx::RectF& image_bounds,
                                     const gfx::RectF& text_bounds1,
                                     const gfx::RectF& text_bounds2) {
  ui::AXNodeData page_node;
  page_node.role = ax::mojom::Role::kRegion;
  page_node.id = 1001;
  page_node.relative_bounds.bounds = image_bounds;

  ui::AXNodeData text_node1;
  text_node1.role = ax::mojom::Role::kStaticText;
  text_node1.id = 1002;
  text_node1.relative_bounds.bounds = text_bounds1;
  page_node.child_ids.push_back(text_node1.id);

  ui::AXNodeData text_node2;
  text_node2.role = ax::mojom::Role::kStaticText;
  text_node2.id = 1003;
  text_node2.relative_bounds.bounds = text_bounds2;
  text_node2.AddIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets,
                                 {0, 10, 20, 30});
  page_node.child_ids.push_back(text_node2.id);

  ui::AXTreeUpdate child_tree_update;
  child_tree_update.root_id = page_node.id;
  child_tree_update.nodes = {page_node, text_node1, text_node2};
  child_tree_update.has_tree_data = true;
  child_tree_update.tree_data.title = "OCR results";

  return child_tree_update;
}

uint32_t CalculateBatchCount(uint32_t page_count, uint32_t batch_size) {
  return (page_count + batch_size - 1) / batch_size;
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

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

bool operator<(const ImagePosition& p1, const ImagePosition& p2) {
  return (p1.page_index < p2.page_index ||
          (p1.page_index == p2.page_index &&
           p1.page_object_index < p2.page_object_index));
}

// This class overrides PdfAccessibilityImageFetcher to return an image from
// previously stored images, instead of looking for it in the PDF.
class TestPdfAccessibilityImageFetcher
    : public chrome_pdf::PdfAccessibilityImageFetcher {
 public:
  TestPdfAccessibilityImageFetcher() {
    default_bitmap_.allocN32Pixels(/*width=*/1, /*height=*/1,
                                   /*isOpaque=*/false);
  }

  ~TestPdfAccessibilityImageFetcher() override = default;

  SkBitmap GetImageForOcr(int32_t page_index,
                          int32_t page_object_index) override {
    auto image = images_.find(ImagePosition(page_index, page_object_index));
    return image != images_.end() ? image->second : default_bitmap_;
  }

  void AddImage(int32_t page_index,
                int32_t page_object_index,
                SkBitmap bitmap) {
    images_[ImagePosition(page_index, page_object_index)] = std::move(bitmap);
  }

 private:
  // Keeps images for (page_index, page_object_index) positions.
  std::map<ImagePosition, SkBitmap> images_;

  // Returned for all requests that have no assigned bitmap in `images_`.
  SkBitmap default_bitmap_;

  chrome_pdf::AccessibilityActionData received_action_data_;
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
      chrome_pdf::PdfAccessibilityActionHandler* action_handler,
      chrome_pdf::PdfAccessibilityImageFetcher* image_fetcher)
      : PdfAccessibilityTree(render_frame,
                             action_handler,
                             image_fetcher,
                             /*plugin_container=*/nullptr,
                             /*print_preview=*/false) {
    ForcePluginAXObjectForTesting(blink::WebAXObject::FromWebNode(
        render_frame->GetWebFrame()->GetDocument().Body()));
  }

  ~TestPdfAccessibilityTree() override = default;
  TestPdfAccessibilityTree(const TestPdfAccessibilityTree&) = delete;
  TestPdfAccessibilityTree& operator=(const TestPdfAccessibilityTree&) = delete;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  std::vector<std::vector<ui::AXTreeUpdate>>& GetTreeUpdates() {
    return tree_updates_;
  }

  void OnOcrDataReceived(std::vector<PdfOcrRequest> ocr_requests,
                         std::vector<ui::AXTreeUpdate> tree_updates) override {
    tree_updates_.push_back(tree_updates);
    PdfAccessibilityTree::OnOcrDataReceived(ocr_requests, tree_updates);
  }

  void CreateFakeOCRHelper(bool create_empty_result) {
    CreateOcrHelper();
    fake_annotator_ = std::make_unique<screen_ai::test::FakeScreenAIAnnotator>(
        create_empty_result);
    ocr_helper_for_testing()->SetScreenAIAnnotatorForTesting(
        fake_annotator_->BindNewPipeAndPassRemote());
  }

 private:
  std::vector<std::vector<ui::AXTreeUpdate>> tree_updates_;
  std::unique_ptr<screen_ai::test::FakeScreenAIAnnotator> fake_annotator_;
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
};

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
    viewport_info_.selection_start_page_index = 0u;
    viewport_info_.selection_start_char_index = 0u;
    viewport_info_.selection_end_page_index = 0u;
    viewport_info_.selection_end_char_index = 0u;
    doc_info_.text_accessible = true;
    doc_info_.text_copyable = true;
    doc_info_.page_count = 1u;
    page_info_.page_index = 0u;
    page_info_.text_run_count = 0u;
    page_info_.char_count = 0u;
    page_info_.bounds = gfx::Rect(0, 0, 1, 1);
  }

  void TearDown() override {
    // Ensure we clean up the PDF accessibility tree before the page closes
    // since we directly set a plugin container.
    pdf_accessibility_tree_->ForcePluginAXObjectForTesting(
        blink::WebAXObject());
    content::RenderViewTest::TearDown();
  }

  void CreatePdfAccessibilityTree() {
    content::RenderFrame* render_frame = GetMainRenderFrame();
    render_frame->SetAccessibilityModeForTest(ui::kAXModeComplete);
    ASSERT_TRUE(render_frame->GetRenderAccessibility());

    pdf_accessibility_tree_ = std::make_unique<TestPdfAccessibilityTree>(
        render_frame, &action_handler_, &image_fetcher_);
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
  chrome_pdf::AccessibilityImageInfo CreateMockInaccessibleImage() {
    chrome_pdf::AccessibilityImageInfo image;
    image.alt_text = "";
    image.bounds = gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f);
    image.page_object_index = 0;
    return image;
  }

  chrome_pdf::AccessibilityViewportInfo viewport_info_;
  chrome_pdf::AccessibilityDocInfo doc_info_;
  chrome_pdf::AccessibilityPageInfo page_info_;
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs_;
  std::vector<chrome_pdf::AccessibilityCharInfo> chars_;
  chrome_pdf::AccessibilityPageObjects page_objects_;
  std::unique_ptr<TestPdfAccessibilityTree> pdf_accessibility_tree_;
  TestPdfAccessibilityActionHandler action_handler_;
  TestPdfAccessibilityImageFetcher image_fetcher_;
};

TEST_F(PdfAccessibilityTreeTest, TestEmptyPDFPage) {
  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
    pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++++ Text Field
   * ++++++ Text Field
   */

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
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
  ASSERT_EQ(1u, paragraph_node->GetChildCount());

  ui::AXNode* static_text_node = paragraph_node->GetChildAtIndex(0);
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->GetChildCount());

  paragraph_node = page_node->GetChildAtIndex(1);
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>& child_nodes =
      paragraph_node->GetAllChildren();
  ASSERT_EQ(3u, child_nodes.size());

  static_text_node = child_nodes[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->GetChildCount());

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
  EXPECT_EQ(0u, text_field_node->GetChildCount());

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
  EXPECT_EQ(0u, text_field_node->GetChildCount());
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
   * ++++ Paragraph
   * ++++++ Static Text
   * ++++++ Check Box
   * ++++++ Radio Button
   * ++++++ Radio Button
   * ++++++ Button
   */

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
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
  ASSERT_EQ(1u, paragraph_node->GetChildCount());

  ui::AXNode* static_text_node = paragraph_node->GetChildAtIndex(0);
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->GetChildCount());

  paragraph_node = page_node->GetChildAtIndex(1);
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>& child_nodes =
      paragraph_node->GetAllChildren();
  ASSERT_EQ(5u, child_nodes.size());

  static_text_node = child_nodes[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->GetChildCount());

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
  EXPECT_EQ(0u, check_box_node->GetChildCount());

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
  EXPECT_EQ(0u, radio_button_node->GetChildCount());

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
  EXPECT_EQ(0u, radio_button_node->GetChildCount());

  ui::AXNode* push_button_node = child_nodes[4];
  ASSERT_TRUE(push_button_node);
  EXPECT_EQ(ax::mojom::Role::kButton, push_button_node->GetRole());
  EXPECT_EQ("Push Button", push_button_node->GetStringAttribute(
                               ax::mojom::StringAttribute::kName));
  EXPECT_EQ(gfx::RectF(1.0f, 4.0f, 5.0f, 6.0f),
            push_button_node->data().relative_bounds.bounds);
  EXPECT_EQ(0u, push_button_node->GetChildCount());
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
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
  ASSERT_EQ(1u, paragraph_node->GetChildCount());

  ui::AXNode* static_text_node = paragraph_node->GetChildAtIndex(0);
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->GetChildCount());

  paragraph_node = page_node->GetChildAtIndex(1);
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>& child_nodes =
      paragraph_node->GetAllChildren();
  ASSERT_EQ(3u, child_nodes.size());

  static_text_node = child_nodes[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->GetChildCount());

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
    ASSERT_EQ(std::size(kExpectedOptions[0]), listbox_node->GetChildCount());
    const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>&
        listbox_child_nodes = listbox_node->GetAllChildren();
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
    ASSERT_EQ(std::size(kExpectedOptions[1]), listbox_node->GetChildCount());
    const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>&
        listbox_child_nodes = listbox_node->GetAllChildren();
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
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
  ASSERT_EQ(1u, paragraph_node->GetChildCount());

  ui::AXNode* static_text_node = paragraph_node->GetChildAtIndex(0);
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->GetChildCount());

  paragraph_node = page_node->GetChildAtIndex(1);
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kParagraph, paragraph_node->GetRole());
  const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>& child_nodes =
      paragraph_node->GetAllChildren();
  ASSERT_EQ(3u, child_nodes.size());

  static_text_node = child_nodes[0];
  ASSERT_TRUE(static_text_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, static_text_node->GetRole());
  ASSERT_EQ(1u, static_text_node->GetChildCount());

  {
    ui::AXNode* combobox_node = child_nodes[1];
    ASSERT_TRUE(combobox_node);
    EXPECT_EQ(ax::mojom::Role::kComboBoxGrouping, combobox_node->GetRole());
    EXPECT_NE(ax::mojom::Restriction::kReadOnly,
              combobox_node->data().GetRestriction());
    EXPECT_TRUE(combobox_node->HasState(ax::mojom::State::kFocusable));
    EXPECT_EQ(kExpectedBounds[0], combobox_node->data().relative_bounds.bounds);
    ASSERT_EQ(2u, combobox_node->GetChildCount());
    const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>&
        combobox_child_nodes = combobox_node->GetAllChildren();

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
              combobox_popup_node->GetChildCount());
    const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>&
        popup_child_nodes = combobox_popup_node->GetAllChildren();
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
    ASSERT_EQ(2u, combobox_node->GetChildCount());
    const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>&
        combobox_child_nodes = combobox_node->GetAllChildren();

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
              combobox_popup_node->GetChildCount());
    const std::vector<raw_ptr<ui::AXNode, VectorExperimental>>&
        popup_child_nodes = combobox_popup_node->GetAllChildren();
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

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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

  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  CheckRootAndStatusNodes(pdf_root, doc_info_.page_count,
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
struct PdfOcrHelperTestBatchData {
  uint32_t page_count;
  uint32_t expected_batch_size;
};

class PdfOcrHelperTest : public PdfAccessibilityTreeTest,
                         public testing::WithParamInterface<std::tuple<
                             /* is_ocr_helper_started_before_pdf_loads */ bool,
                             PdfOcrHelperTestBatchData>> {
 public:
  PdfOcrHelperTest() : feature_list_(::features::kPdfOcr) {}
  PdfOcrHelperTest(const PdfOcrHelperTest&) = delete;
  PdfOcrHelperTest& operator=(const PdfOcrHelperTest&) = delete;
  ~PdfOcrHelperTest() override = default;

 protected:
  void CreateInaccessiblePdfAndOcrHelper(
      uint32_t page_count,
      bool is_ocr_helper_started_before_pdf_loads,
      bool create_empty_results) {
    ASSERT_TRUE(pdf_accessibility_tree_);
    doc_info_.page_count = page_count;

    chrome_pdf::AccessibilityImageInfo image = CreateMockInaccessibleImage();
    ASSERT_EQ(0u, image.text_run_index)
        << "Images should not be anchored to any `TextRunInfo` for the "
           "`PdfOcrHelper` to work with them.";
    // Each page has two images in it.
    page_objects_.images.push_back(image);
    page_objects_.images.push_back(image);

    if (is_ocr_helper_started_before_pdf_loads) {
      pdf_accessibility_tree_->CreateFakeOCRHelper(create_empty_results);
      ASSERT_NE(nullptr, pdf_accessibility_tree_->ocr_helper_for_testing());
    }

    pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
    pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
    ASSERT_EQ(0u, text_runs_.size())
        << "OcrHelper won't run unless the PDF has no accessible text in it.";
    ASSERT_EQ(0u, chars_.size())
        << "OcrHelper won't run unless the PDF has no accessible text in it.";
    for (uint32_t i = 0; i < doc_info_.page_count; ++i) {
      page_info_.page_index = i;
      // All pages are identical.
      pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                        chars_, page_objects_);
    }
    WaitForThreadTasks();
    // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
    WaitForThreadDelayedTasks();

    ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
    CheckRootAndStatusNodes(root_node, doc_info_.page_count,
                            /*is_pdf_ocr_test=*/true,
                            is_ocr_helper_started_before_pdf_loads,
                            create_empty_results);

    ASSERT_GT(root_node->GetChildCount(), 1u);
    ui::AXNode* page_node = root_node->GetChildAtIndex(1);
    ASSERT_NE(nullptr, page_node);
    ASSERT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
    ASSERT_EQ(1u, page_node->GetChildCount());

    ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
    ASSERT_NE(nullptr, paragraph_node);
    ASSERT_EQ((is_ocr_helper_started_before_pdf_loads && !create_empty_results)
                  ? ax::mojom::Role::kGenericContainer
                  : ax::mojom::Role::kParagraph,
              paragraph_node->GetRole());
    ASSERT_EQ(2u, paragraph_node->GetChildCount());

    ui::AXNode* first_node = paragraph_node->GetChildAtIndex(0);
    ASSERT_NE(nullptr, first_node);
    ASSERT_EQ(is_ocr_helper_started_before_pdf_loads && !create_empty_results
                  ? ax::mojom::Role::kStaticText
                  : ax::mojom::Role::kImage,
              first_node->GetRole());
    ASSERT_EQ(0u, first_node->GetChildCount());

    ui::AXNode* second_node = paragraph_node->GetChildAtIndex(1);
    ASSERT_NE(nullptr, second_node);
    ASSERT_EQ(is_ocr_helper_started_before_pdf_loads && !create_empty_results
                  ? ax::mojom::Role::kStaticText
                  : ax::mojom::Role::kImage,
              second_node->GetRole());
    ASSERT_EQ(0u, second_node->GetChildCount());

    if (!is_ocr_helper_started_before_pdf_loads) {
      pdf_accessibility_tree_->CreateFakeOCRHelper(create_empty_results);
      ASSERT_NE(nullptr, pdf_accessibility_tree_->ocr_helper_for_testing());
    }
  }

  bool GetIsOcrHelperStartedBeforePdfLoads() const {
    return std::get<0>(GetParam());
  }

  uint32_t GetPageCount() const { return std::get<1>(GetParam()).page_count; }

  uint32_t GetExpectedBatchSize() const {
    return std::get<1>(GetParam()).expected_batch_size;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(PdfOcrHelperTest, PageBatching) {
  CreatePdfAccessibilityTree();

  const bool is_ocr_helper_started_before_pdf_loads =
      GetIsOcrHelperStartedBeforePdfLoads();
  const uint32_t page_count = GetPageCount();
  ASSERT_NO_FATAL_FAILURE(CreateInaccessiblePdfAndOcrHelper(
      page_count, is_ocr_helper_started_before_pdf_loads,
      /*create_empty_results=*/false));

  const uint32_t pages_per_batch =
      pdf_accessibility_tree_->ocr_helper_for_testing()
          ->pages_per_batch_for_testing();
  EXPECT_EQ(GetExpectedBatchSize(), pages_per_batch);

  const uint32_t batch_count = CalculateBatchCount(page_count, pages_per_batch);

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  // The first node of the root node's children is a status node.
  ASSERT_EQ(page_count + 1u, root_node->GetChildCount());
  for (uint32_t i = 0; i < page_count; ++i) {
    if (!is_ocr_helper_started_before_pdf_loads) {
      ui::AXNode* page_node = root_node->GetChildAtIndex(i + 1);
      ASSERT_NE(nullptr, page_node);
      ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
      ASSERT_NE(nullptr, paragraph_node);
      ui::AXNode* image1_node = paragraph_node->GetChildAtIndex(0);
      ASSERT_NE(nullptr, image1_node);
      ui::AXNode* image2_node = paragraph_node->GetChildAtIndex(1);
      ASSERT_NE(nullptr, image2_node);
      base::queue<PdfOcrRequest> requests;
      requests.emplace(image1_node->id(), CreateMockInaccessibleImage(),
                       root_node->id(), paragraph_node->id(), page_node->id(),
                       /*page_index=*/i);
      requests.emplace(image2_node->id(), CreateMockInaccessibleImage(),
                       root_node->id(), paragraph_node->id(), page_node->id(),
                       /*page_index=*/i);
      pdf_accessibility_tree_->ocr_helper_for_testing()->OcrPage(requests);

      // Each page has two images.
      WaitForThreadTasks();
      WaitForThreadTasks();
    } else {
      // Each page has two images.
      WaitForThreadTasks();
      WaitForThreadTasks();
    }
  }

  const auto& tree_updates = pdf_accessibility_tree_->GetTreeUpdates();
  ASSERT_EQ(batch_count, tree_updates.size());
  for (uint32_t i = 0; i < tree_updates.size(); ++i) {
    const std::vector<ui::AXTreeUpdate>& page_tree_updates = tree_updates[i];
    if (page_count % pages_per_batch != 0u && i == 0) {
      // The first batch should have the remaining pages that cannot be
      // processed by the rest of the batches because they are full. By design,
      // this is always set to 5u in the instantiation of these parameterized
      // tests.
      ASSERT_EQ(10u, page_tree_updates.size())
          << "There should be five pages in the first batch with two images "
             "per page, because we first process the remaining pages after "
             "dividing with the batch size.";

      for (uint32_t j = 0; j < 10u; ++j) {
        EXPECT_EQ(page_tree_updates[j].nodes.size(), 1u);
        EXPECT_EQ(page_tree_updates[j].root_id,
                  page_tree_updates[j].nodes[0].id);
        EXPECT_EQ(page_tree_updates[j].nodes[0].role,
                  ax::mojom::Role::kStaticText);
      }
    } else {
      // All other batches should be full, i.e., their page count should equal
      // the number of pages allowed in each batch.
      ASSERT_EQ(pages_per_batch * 2u, page_tree_updates.size())
          << "There should be 20 pages in the remaining batches, with two "
             "images per page.";

      for (uint32_t j = 0; j < pages_per_batch * 2u; ++j) {
        EXPECT_EQ(page_tree_updates[j].nodes.size(), 1u);
        EXPECT_EQ(page_tree_updates[j].root_id,
                  page_tree_updates[j].nodes[0].id);
        EXPECT_EQ(page_tree_updates[j].nodes[0].role,
                  ax::mojom::Role::kStaticText);
      }
    }
  }
}

TEST_P(PdfOcrHelperTest, UMAMetrics) {
  CreatePdfAccessibilityTree();

  base::HistogramTester histograms;
  const bool is_ocr_helper_started_before_pdf_loads =
      GetIsOcrHelperStartedBeforePdfLoads();
  const uint32_t page_count = GetPageCount();
  ASSERT_NO_FATAL_FAILURE(CreateInaccessiblePdfAndOcrHelper(
      page_count, is_ocr_helper_started_before_pdf_loads,
      /*create_empty_results=*/false));
  const uint32_t pages_per_batch =
      pdf_accessibility_tree_->ocr_helper_for_testing()
          ->pages_per_batch_for_testing();

  for (uint32_t i = 0; i < page_count; ++i) {
    if (!is_ocr_helper_started_before_pdf_loads) {
      ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
      ui::AXNode* page_node = root_node->GetChildAtIndex(i + 1);
      ASSERT_NE(nullptr, page_node);
      ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
      ASSERT_NE(nullptr, paragraph_node);
      ui::AXNode* image1_node = paragraph_node->GetChildAtIndex(0);
      ASSERT_NE(nullptr, image1_node);
      ui::AXNode* image2_node = paragraph_node->GetChildAtIndex(1);
      ASSERT_NE(nullptr, image2_node);
      base::queue<PdfOcrRequest> requests;
      requests.emplace(image1_node->id(), CreateMockInaccessibleImage(),
                       root_node->id(), paragraph_node->id(), page_node->id(),
                       /*page_index=*/i);
      requests.emplace(image2_node->id(), CreateMockInaccessibleImage(),
                       root_node->id(), paragraph_node->id(), page_node->id(),
                       /*page_index=*/i);
      pdf_accessibility_tree_->ocr_helper_for_testing()->OcrPage(requests);
      // The UMA metric recorded in `PdfAccessibilityTree::OnOcrDataReceived()`
      // is triggered by `OcrPage()`. `WaitForThreadTasks()` below is similar
      // to the purpose of `content::FetchHistogramsFromChildProcesses()`.
      WaitForThreadTasks();
    }

    // Each page has two images.
    WaitForThreadTasks();
    WaitForThreadTasks();
  }

  const auto& tree_updates = pdf_accessibility_tree_->GetTreeUpdates();
  const uint32_t batch_count = CalculateBatchCount(page_count, pages_per_batch);
  ASSERT_EQ(batch_count, tree_updates.size());

  histograms.ExpectBucketCount(
      "Accessibility.PdfOcr.ActiveWhenInaccessiblePdfOpened",
      is_ocr_helper_started_before_pdf_loads,
      /*expected_count=*/1);
  histograms.ExpectTotalCount(
      "Accessibility.PdfOcr.ActiveWhenInaccessiblePdfOpened",
      /*expected_count=*/1);

  // There are two mock images per page.
  histograms.ExpectBucketCount("Accessibility.PdfOcr.PDFImages",
                               PdfOcrRequestStatus::kRequested,
                               /*expected_count=*/page_count * 2);
  histograms.ExpectBucketCount("Accessibility.PdfOcr.PDFImages",
                               PdfOcrRequestStatus::kPerformed,
                               /*expected_count=*/page_count * 2);
  histograms.ExpectTotalCount("Accessibility.PdfOcr.PDFImages",
                              /*expected_count=*/page_count * 4);

  // TODO(crbug.com/40267312): The current test fixture does not trigger
  // `PdfAccessibilityTree::MaybeHandleAccessibilityChange` when OCR is enabled
  // after tree load, and hence does result in calling
  // `PdfAccessibilityTree::SetAccessibilityPageInfo` for the second time.
  // Either update text fixture to be more realistic, or add metrics test to
  // browser test without fake OCR helper.
  histograms.ExpectBucketCount("Accessibility.PDF.HasAccessibleText",
                               /*sample=*/false,
                               /*expected_count=*/1);
  histograms.ExpectTotalCount("Accessibility.PDF.HasAccessibleText",
                              /*expected_count=*/1);

  histograms.ExpectBucketCount("Accessibility.PdfOcr.InaccessiblePdfPageCount",
                               doc_info_.page_count,
                               /*expected_count=*/1);
  histograms.ExpectTotalCount("Accessibility.PdfOcr.InaccessiblePdfPageCount",
                              /*expected_count=*/1);
}

TEST_P(PdfOcrHelperTest, EmptyOCRResults) {
  CreatePdfAccessibilityTree();

  const bool is_ocr_helper_started_before_pdf_loads =
      GetIsOcrHelperStartedBeforePdfLoads();
  const uint32_t page_count = GetPageCount();
  ASSERT_NO_FATAL_FAILURE(CreateInaccessiblePdfAndOcrHelper(
      page_count, is_ocr_helper_started_before_pdf_loads,
      /*create_empty_results=*/true));

  for (uint32_t i = 0; i < page_count; ++i) {
    if (!is_ocr_helper_started_before_pdf_loads) {
      ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
      ui::AXNode* page_node = root_node->GetChildAtIndex(i + 1);
      ASSERT_NE(nullptr, page_node);
      ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
      ASSERT_NE(nullptr, paragraph_node);
      ui::AXNode* image1_node = paragraph_node->GetChildAtIndex(0);
      ASSERT_NE(nullptr, image1_node);
      ui::AXNode* image2_node = paragraph_node->GetChildAtIndex(1);
      ASSERT_NE(nullptr, image2_node);
      base::queue<PdfOcrRequest> requests;
      requests.emplace(image1_node->id(), CreateMockInaccessibleImage(),
                       root_node->id(), paragraph_node->id(), page_node->id(),
                       /*page_index=*/i);
      requests.emplace(image2_node->id(), CreateMockInaccessibleImage(),
                       root_node->id(), paragraph_node->id(), page_node->id(),
                       /*page_index=*/i);
      pdf_accessibility_tree_->ocr_helper_for_testing()->OcrPage(requests);
    }

    // Each page has two images.
    WaitForThreadTasks();
    WaitForThreadTasks();
  }

  // Make sure that the OCR helper counts a response with empty results to
  // determine whether it finished processing all OCR requests.
  EXPECT_TRUE(
      pdf_accessibility_tree_->ocr_helper_for_testing()->AreAllPagesOcred());

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  ASSERT_NE(nullptr, root_node);
  ASSERT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());
  uint32_t pages_plus_status_node_count = doc_info_.page_count + 1u;
  ASSERT_EQ(pages_plus_status_node_count, root_node->GetChildCount());

  ui::AXNode* status_wrapper_node = root_node->GetChildAtIndex(0);
  ASSERT_NE(nullptr, status_wrapper_node);
  ASSERT_EQ(ax::mojom::Role::kBanner, status_wrapper_node->GetRole());
  ASSERT_EQ(1u, status_wrapper_node->GetChildCount());

  ui::AXNode* status_node = status_wrapper_node->GetChildAtIndex(0);
  ASSERT_NE(nullptr, status_node);
  ASSERT_EQ(ax::mojom::Role::kStatus, status_node->GetRole());
  // Note that the string below must be synced with `IDS_PDF_OCR_NO_RESULT`.
  constexpr char kPdfOcrNoResult[] =
      "This PDF is inaccessible. No text extracted";
  ASSERT_EQ(kPdfOcrNoResult,
            status_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  for (uint32_t i = 0; i < page_count; ++i) {
    // All children nodes stay the same, except for image nodes. Image nodes
    // should have the label set for an image without alt text.
    ui::AXNode* page_node = root_node->GetChildAtIndex(i + 1);
    ASSERT_NE(nullptr, page_node);
    ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
    ASSERT_NE(nullptr, paragraph_node);
    ui::AXNode* image1_node = paragraph_node->GetChildAtIndex(0);
    ASSERT_NE(nullptr, image1_node);
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION),
              image1_node->GetNameUTF8());
    ui::AXNode* image2_node = paragraph_node->GetChildAtIndex(1);
    ASSERT_NE(nullptr, image2_node);
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION),
              image2_node->GetNameUTF8());
    EXPECT_FALSE(image2_node->HasStringAttribute(
        ax::mojom::StringAttribute::kDescription));
  }
}

TEST_P(PdfOcrHelperTest, OCRCompleteNotification) {
  CreatePdfAccessibilityTree();

  const bool is_ocr_helper_started_before_pdf_loads =
      GetIsOcrHelperStartedBeforePdfLoads();
  const uint32_t page_count = GetPageCount();
  ASSERT_NO_FATAL_FAILURE(CreateInaccessiblePdfAndOcrHelper(
      page_count, is_ocr_helper_started_before_pdf_loads,
      /*create_empty_results=*/false));

  const ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  ASSERT_NE(nullptr, root_node);
  ASSERT_EQ(ax::mojom::Role::kPdfRoot, root_node->GetRole());
  const uint32_t pages_plus_status_node_count = doc_info_.page_count + 1u;
  ASSERT_EQ(pages_plus_status_node_count, root_node->GetChildCount());

  const ui::AXNode* status_wrapper_node = root_node->GetChildAtIndex(0);
  ASSERT_NE(nullptr, status_wrapper_node);
  ASSERT_EQ(ax::mojom::Role::kBanner, status_wrapper_node->GetRole());
  ASSERT_EQ(1u, status_wrapper_node->GetChildCount());

  const ui::AXNode* status_node = status_wrapper_node->GetChildAtIndex(0);
  ASSERT_NE(nullptr, status_node);
  ASSERT_EQ(ax::mojom::Role::kStatus, status_node->GetRole());

  for (uint32_t i = 0; i < page_count; ++i) {
    if (!is_ocr_helper_started_before_pdf_loads) {
      const ui::AXNode* page_node = root_node->GetChildAtIndex(i + 1);
      ASSERT_NE(nullptr, page_node);
      const ui::AXNode* paragraph_node = page_node->GetChildAtIndex(0);
      ASSERT_NE(nullptr, paragraph_node);
      const ui::AXNode* image1_node = paragraph_node->GetChildAtIndex(0);
      ASSERT_NE(nullptr, image1_node);
      const ui::AXNode* image2_node = paragraph_node->GetChildAtIndex(1);
      ASSERT_NE(nullptr, image2_node);
      base::queue<PdfOcrRequest> requests;
      requests.emplace(image1_node->id(), CreateMockInaccessibleImage(),
                       root_node->id(), paragraph_node->id(), page_node->id(),
                       /*page_index=*/i);
      requests.emplace(image2_node->id(), CreateMockInaccessibleImage(),
                       root_node->id(), paragraph_node->id(), page_node->id(),
                       /*page_index=*/i);
      pdf_accessibility_tree_->ocr_helper_for_testing()->OcrPage(requests);
    }

    // Each page has two images.
    WaitForThreadTasks();
    WaitForThreadTasks();
  }

  // Make sure that the OCR helper counts a response with empty results to
  // determine whether it finished processing all OCR requests.
  EXPECT_TRUE(
      pdf_accessibility_tree_->ocr_helper_for_testing()->AreAllPagesOcred());
  // Note that the string below must be synced with `IDS_PDF_OCR_COMPLETED`.
  constexpr char kPdfOcrCompleted[] =
      "This PDF is inaccessible. Text extracted, powered by Google AI";
  ASSERT_EQ(kPdfOcrCompleted,
            status_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
}

// 5 = smaller than the batch size, 105 = larger than the batch size
// with fewer remaining pages in the first batch, 280 = greater than the
// batch size by a lot and no remaining pages in the first batch.
INSTANTIATE_TEST_SUITE_P(
    PdfOcrHelperTests,
    PdfOcrHelperTest,
    testing::Combine(
        /* is_ocr_helper_started_before_pdf_loads */ testing::Bool(),
        /* (page_count, expected_batch_size) */ testing::Values(
            PdfOcrHelperTestBatchData(5u, 1u),
            PdfOcrHelperTestBatchData(105u, 10u),
            PdfOcrHelperTestBatchData(280u, 20u))));

// TODO(crbug.com/40267312): Add test for end result on a non-synthetic
// multi-page PDF.

class PdfOcrTest : public PdfAccessibilityTreeTest {
 public:
  PdfOcrTest() : feature_list_(::features::kPdfOcr) {}
  PdfOcrTest(const PdfOcrTest&) = delete;
  PdfOcrTest& operator=(const PdfOcrTest&) = delete;
  ~PdfOcrTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PdfOcrTest, CheckLiveRegionPoliteStatus) {
  CreatePdfAccessibilityTree();

  page_objects_.images.push_back(CreateMockInaccessibleImage());

  // Get and use the underlying AXTree to create an AXEventGenerator. This
  // event generator is usually instrumented in the test.
  ui::AXTree& tree = pdf_accessibility_tree_->tree_for_testing();
  ui::AXEventGenerator event_generator(&tree);
  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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

  uint32_t pages_plus_status_node_count = doc_info_.page_count + 1u;
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
  WaitForThreadTasks();

  page_info_.page_index = 0;
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  const ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
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
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  const ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  // `is_pdf_ocr_test` needs to be set to false below, as it shouldn't announce
  // the PDF OCR feature notification in this case.
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
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

// Test param: image orientation.
class PdfOcrRotationTest : public PdfOcrTest,
                           public testing::WithParamInterface<int> {
 public:
  PdfOcrRotationTest() = default;
  PdfOcrRotationTest(const PdfOcrRotationTest&) = delete;
  PdfOcrRotationTest& operator=(const PdfOcrRotationTest&) = delete;
  ~PdfOcrRotationTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(All, PdfOcrRotationTest, testing::Range(0, 4));

TEST_P(PdfOcrRotationTest, TestTransformFromOnOcrDataReceived) {
  // Assume `image` contains some text that will be extracted by OCR. `image`
  // will be passed to the function that creates a transform, which will be
  // then applied to the text paragraphs extracted by OCR.
  chrome_pdf::AccessibilityImageInfo image;
  // Simulate that the width and height of `image` got shrunk by 80% in
  // `image_data`.
  constexpr float kScaleFactor = 0.8f;
  constexpr float kImageWidth = 200.0f;
  constexpr float kImageHeight = 400.0f;
  constexpr int kBitmapWidth = static_cast<int>(kImageWidth * kScaleFactor);
  constexpr int kBitmapHeight = static_cast<int>(kImageHeight * kScaleFactor);
  image.page_object_index = 0;
  image.bounds = gfx::RectF(0.0f, 0.0f, kImageWidth, kImageHeight);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kBitmapWidth, kBitmapHeight, /*isOpaque=*/false);
  image_fetcher_.AddImage(/*page_index=*/0, image.page_object_index,
                          std::move(bitmap));
  page_objects_.images.push_back(image);

  page_info_.text_run_count = text_runs_.size();
  page_info_.char_count = chars_.size();

  int orientation = GetParam();
  viewport_info_.orientation = orientation;

  CreatePdfAccessibilityTree();

  pdf_accessibility_tree_->SetAccessibilityViewportInfo(viewport_info_);
  pdf_accessibility_tree_->SetAccessibilityDocInfo(doc_info_);
  pdf_accessibility_tree_->SetAccessibilityPageInfo(page_info_, text_runs_,
                                                    chars_, page_objects_);
  WaitForThreadTasks();
  // Wait for `PdfAccessibilityTree::UnserializeNodes()`, a delayed task.
  WaitForThreadDelayedTasks();

  /*
   * Expected PDF accessibility tree structure (with PDF OCR feature flag)
   * Document
   * ++ Banner
   * ++++ Status
   * ++ Region
   * ++++ Paragraph
   * ++++++ image
   */

  ui::AXNode* root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
                          /*is_pdf_ocr_test=*/true,
                          /*is_ocr_completed=*/false,
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

  ui::AXNode* image_node = paragraph_node->GetChildAtIndex(0);
  ASSERT_TRUE(image_node);
  EXPECT_EQ(ax::mojom::Role::kImage, image_node->GetRole());
  ASSERT_EQ(0u, image_node->GetChildCount());
  EXPECT_EQ(image.bounds, image_node->data().relative_bounds.bounds);

  // Simulate creating a child tree using OCR results.
  pdf_accessibility_tree_->CreateOcrHelper();

  // Text bounds before applying the transform.
  constexpr gfx::RectF kTextBoundsBeforeTransform1 = {{8.0f, 8.0f},
                                                      {80.0f, 24.0f}};
  constexpr gfx::RectF kTextBoundsBeforeTransform2 = {{16.0f, 88.0f},
                                                      {40.0f, 56.0f}};
  ui::AXTreeUpdate child_tree_update = CreateMockOCRResult(
      image.bounds, kTextBoundsBeforeTransform1, kTextBoundsBeforeTransform2);
  WaitForThreadTasks();

  EXPECT_EQ(child_tree_update.tree_data.tree_id, ui::AXTreeIDUnknown());

  PdfOcrRequest request(image_node->id(), image, root_node->id(),
                        paragraph_node->id(), page_node->id(),
                        /*page_index=*/0);
  // Image pixel size is automatically set when OCR request is running, but
  // this test skips that step.
  request.image_pixel_size = gfx::SizeF(kBitmapWidth, kBitmapHeight);

  // Reset `remaining_page_count_` to be zero. `remaining_page_count_` is later
  // used in `OnOcrDataReceived()` to check whether OCR is done or not. Note
  // that the OCR is considered to be done when `remaining_page_count_` == 0.
  pdf_accessibility_tree_->ocr_helper_for_testing()
      ->ResetRemainingPageCountForTesting();
  pdf_accessibility_tree_->OnOcrDataReceived(
      std::vector<PdfOcrRequest>{{request}},
      std::vector<ui::AXTreeUpdate>{child_tree_update});
  WaitForThreadTasks();

  /*
   * Expected PDF accessibility tree structure (after running OCR)
   * Document
   * ++ Status
   * ++ Region
   * ++++ Paragraph
   * ++++++ Region (child tree)
   * ++++++++ Static Text
   * ++++++++ Static Text
   */

  root_node = pdf_accessibility_tree_->GetRoot();
  CheckRootAndStatusNodes(root_node, doc_info_.page_count,
                          /*is_pdf_ocr_test=*/true,
                          /*is_ocr_completed=*/true,
                          /*create_empty_ocr_results=*/false);

  ASSERT_GT(root_node->GetChildCount(), 1u);
  page_node = root_node->GetChildAtIndex(1);
  ASSERT_TRUE(page_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, page_node->GetRole());
  ASSERT_EQ(1u, page_node->GetChildCount());

  paragraph_node = page_node->GetChildAtIndex(0);
  ASSERT_TRUE(paragraph_node);
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, paragraph_node->GetRole());
  ASSERT_EQ(1u, paragraph_node->GetChildCount());

  ui::AXNode* region_node = paragraph_node->GetChildAtIndex(0);
  ASSERT_TRUE(region_node);
  EXPECT_EQ(ax::mojom::Role::kRegion, region_node->GetRole());
  ASSERT_EQ(2u, region_node->GetChildCount());

  // Expected text bounds after applying the transform. These numbers are
  // expected to be kTextBoundsBeforeTransform / kScaleFactor, and then rotated.
  constexpr gfx::RectF kExpectedTextBoundRelativeToTreeBounds0[4] = {
      {{10.0f, 10.0f}, {100.0f, 30}},
      {{360.0f, 10.0f}, {30.0f, 100}},
      {{90.0f, 360.0f}, {100.0f, 30}},
      {{10.0f, 90.0f}, {30.0f, 100}}};
  constexpr gfx::RectF kExpectedTextBoundRelativeToTreeBounds2[4] = {
      {{20.0f, 110.0f}, {50.0f, 70}},
      {{220.0f, 20.0f}, {70.0f, 50}},
      {{130.0f, 220.0f}, {50.0f, 70}},
      {{110.0f, 130.0f}, {70.0f, 50}}};
  ASSERT_LT(GetParam(), 4);

  // Check the nodes from OCR results.
  ui::AXNode* ocred_node = region_node->GetChildAtIndex(0);
  ASSERT_TRUE(ocred_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, ocred_node->GetRole());
  gfx::RectF bounds = ocred_node->data().relative_bounds.bounds;
  // The bounds already got updated inside of OnOcrDataReceived().
  CompareRect(kExpectedTextBoundRelativeToTreeBounds0[GetParam()], bounds);

  ocred_node = region_node->GetChildAtIndex(1);
  ASSERT_TRUE(ocred_node);
  EXPECT_EQ(ax::mojom::Role::kStaticText, ocred_node->GetRole());
  bounds = ocred_node->data().relative_bounds.bounds;
  // The bounds already got updated inside of OnOcrDataReceived().
  CompareRect(kExpectedTextBoundRelativeToTreeBounds2[GetParam()], bounds);

  // Verify that character offsets are only scaled by 1 / kScaleFactor and are
  // not modified by rotation.
  std::vector<int32_t> character_offsets = ocred_node->GetIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets);
  EXPECT_THAT(character_offsets, testing::ElementsAreArray({0, 12, 25, 37}));
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

}  // namespace pdf
