// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_document_helper.h"

#include "base/test/metrics/user_action_tester.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "components/pdf/browser/pdf_document_helper_client.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "pdf/mojom/pdf.mojom.h"
#include "pdf/pdf_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/gfx/selection_bound.h"

namespace pdf {

namespace {

using ::testing::NiceMock;

class FakePdfListener : public pdf::mojom::PdfListener {
 public:
  FakePdfListener() = default;
  FakePdfListener(const FakePdfListener&) = delete;
  FakePdfListener& operator=(const FakePdfListener&) = delete;
  ~FakePdfListener() override = default;

  MOCK_METHOD(void, SetCaretPosition, (const gfx::PointF&), (override));
  MOCK_METHOD(void, MoveRangeSelectionExtent, (const gfx::PointF&), (override));
  MOCK_METHOD(void,
              SetSelectionBounds,
              (const gfx::PointF&, const gfx::PointF&),
              (override));
  MOCK_METHOD(void,
              GetPdfBytes,
              (uint32_t, GetPdfBytesCallback callback),
              (override));
};

class TestPDFDocumentHelperClient : public PDFDocumentHelperClient {
 public:
  TestPDFDocumentHelperClient() = default;
  ~TestPDFDocumentHelperClient() override = default;
  TestPDFDocumentHelperClient(const TestPDFDocumentHelperClient&) = delete;
  TestPDFDocumentHelperClient& operator=(const TestPDFDocumentHelperClient&) =
      delete;

  const gfx::SelectionBound& GetSelectionBoundStart() const { return start_; }
  const gfx::SelectionBound& GetSelectionBoundEnd() const { return end_; }

 private:
  // PDFDocumentHelperClient:
  void UpdateContentRestrictions(content::RenderFrameHost* render_frame_host,
                                 int content_restrictions) override {}
  void OnPDFHasUnsupportedFeature(content::WebContents* contents) override {}
  void OnSaveURL(content::WebContents* contents) override {}
  void SetPluginCanSave(content::RenderFrameHost* render_frame_host,
                        bool can_save) override {}
  void OnDidScroll(const gfx::SelectionBound& start,
                   const gfx::SelectionBound& end) override {
    start_ = start;
    end_ = end;
  }

 private:
  // The last bounds reported by PDFDocumentHelper.
  gfx::SelectionBound start_;
  gfx::SelectionBound end_;
};

}  // namespace

class PDFDocumentHelperTest : public base::test::WithFeatureOverride,
                              public content::ContentBrowserTest {
 public:
  PDFDocumentHelperTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}
  ~PDFDocumentHelperTest() override = default;

 protected:
  void SelectionChanged(const gfx::PointF& left,
                        int32_t left_height,
                        const gfx::PointF& right,
                        int32_t right_height) {
    pdf_document_helper()->SelectionChanged(left, left_height, right,
                                            right_height);
  }

  PDFDocumentHelper* pdf_document_helper() {
    return PDFDocumentHelper::GetForCurrentDocument(
        shell()->web_contents()->GetPrimaryMainFrame());
  }

  content::RenderWidgetHostView* GetRenderWidgetHostView() {
    return shell()->web_contents()->GetRenderWidgetHostView();
  }

  TestPDFDocumentHelperClient* client() { return client_; }

  // content::ContentBrowserTest:
  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(content::NavigateToURL(shell(), GURL("about:blank")));
    auto client = std::make_unique<TestPDFDocumentHelperClient>();
    client_ = client.get();
    PDFDocumentHelper::CreateForCurrentDocument(
        shell()->web_contents()->GetPrimaryMainFrame(), std::move(client));
  }

  void TearDownOnMainThread() override {
    client_ = nullptr;
    content::ContentBrowserTest::TearDownOnMainThread();
  }

 private:
  raw_ptr<TestPDFDocumentHelperClient> client_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(PDFDocumentHelperTest, SetListenerTwice) {
  NiceMock<FakePdfListener> listener;

  {
    mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
    pdf_document_helper()->SetListener(receiver.BindNewPipeAndPassRemote());
  }

  {
    mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
    pdf_document_helper()->SetListener(receiver.BindNewPipeAndPassRemote());
  }
}

// Tests that select-changed on a pdf text brings up selection handles and the
// quick menu in the reasonable position.
IN_PROC_BROWSER_TEST_P(PDFDocumentHelperTest, SelectionChanged) {
  gfx::SelectionBound initial_start = client()->GetSelectionBoundStart();
  gfx::SelectionBound initial_end = client()->GetSelectionBoundEnd();

  EXPECT_EQ(gfx::RectF(),
            gfx::RectFBetweenSelectionBounds(initial_start, initial_end));
  EXPECT_EQ(gfx::RectF(), gfx::RectFBetweenVisibleSelectionBounds(initial_start,
                                                                  initial_end));

  constexpr gfx::PointF kLeft(1.0f, 1.0f);
  constexpr gfx::PointF kRight(5.0f, 5.0f);
  constexpr int32_t kLeftHeight = 2;
  constexpr int32_t kRightHeight = 2;
  SelectionChanged(kLeft, kLeftHeight, kRight, kRightHeight);

  gfx::SelectionBound start = client()->GetSelectionBoundStart();
  gfx::SelectionBound end = client()->GetSelectionBoundEnd();

#if BUILDFLAG(IS_MAC)
  // Since macOS does not support Touch Selection Editing, the
  // SelectionChanged() call does not affect the selection bounds.
  EXPECT_EQ(start, initial_start);
  EXPECT_EQ(end, initial_end);
#else
  gfx::PointF origin_f;
  content::RenderWidgetHostView* view = GetRenderWidgetHostView();
  if (view) {
    origin_f = view->TransformPointToRootCoordSpaceF(gfx::PointF());
  }

  gfx::SelectionBound expected_start;
  {
    gfx::PointF edge_start(kLeft.x() + origin_f.x(), kLeft.y() + origin_f.y());
    gfx::PointF edge_end(kLeft.x() + origin_f.x(),
                         kLeft.y() + origin_f.y() + kLeftHeight);
    expected_start.SetEdge(edge_start, edge_end);
    expected_start.SetVisibleEdge(edge_start, edge_end);
  }

  gfx::SelectionBound expected_end;
  {
    gfx::PointF edge_start(kRight.x() + origin_f.x(),
                           kRight.y() + origin_f.y());
    gfx::PointF edge_end(kRight.x() + origin_f.x(),
                         kRight.y() + origin_f.y() + kRightHeight);
    expected_end.SetEdge(edge_start, edge_end);
    expected_end.SetVisibleEdge(edge_start, edge_end);
  }

  ASSERT_NE(expected_start, expected_end);

  expected_start.set_visible(true);
  expected_start.set_type(gfx::SelectionBound::LEFT);
  EXPECT_EQ(expected_start, start);

  expected_end.set_visible(true);
  expected_end.set_type(gfx::SelectionBound::RIGHT);
  EXPECT_EQ(expected_end, end);

  gfx::RectF expected_rect(
      expected_start.edge_start().x(), expected_start.edge_start().y(),
      expected_end.edge_start().x() - expected_start.edge_start().x(),
      expected_end.edge_end().y() - expected_start.edge_start().y());

  // The rect between the visible selection bounds determines the position of
  // the quick menu.
  EXPECT_EQ(expected_rect, gfx::RectFBetweenSelectionBounds(start, end));
  EXPECT_EQ(expected_rect, gfx::RectFBetweenVisibleSelectionBounds(start, end));
#endif  // BUILDFLAG(IS_MAC)
}

// When selecting something, only the copy command id should be enabled.
IN_PROC_BROWSER_TEST_P(PDFDocumentHelperTest, IsCommandIdEnabledCopyEnabled) {
  EXPECT_FALSE(
      pdf_document_helper()->IsCommandIdEnabled(ui::TouchEditable::kCut));
  EXPECT_FALSE(
      pdf_document_helper()->IsCommandIdEnabled(ui::TouchEditable::kCopy));

  constexpr gfx::PointF kLeft(1.0f, 1.0f);
  constexpr gfx::PointF kRight(5.0f, 5.0f);
  constexpr int32_t kLeftHeight = 2;
  constexpr int32_t kRightHeight = 2;
  SelectionChanged(kLeft, kLeftHeight, kRight, kRightHeight);

  EXPECT_FALSE(
      pdf_document_helper()->IsCommandIdEnabled(ui::TouchEditable::kCut));

#if BUILDFLAG(IS_MAC)
  // Since macOS does not support Touch Selection Editing, the copy command is
  // not enabled.
  EXPECT_FALSE(
      pdf_document_helper()->IsCommandIdEnabled(ui::TouchEditable::kCopy));
#else
  EXPECT_TRUE(
      pdf_document_helper()->IsCommandIdEnabled(ui::TouchEditable::kCopy));
#endif  // BUILDFLAG(IS_MAC)
}

// Test that the copy command executes.
IN_PROC_BROWSER_TEST_P(PDFDocumentHelperTest, ExecuteCommandCopy) {
  base::UserActionTester action_tester;
  EXPECT_EQ(0, action_tester.GetActionCount("Copy"));

  pdf_document_helper()->ExecuteCommand(ui::TouchEditable::kCopy, 0);

  EXPECT_EQ(1, action_tester.GetActionCount("Copy"));
}

IN_PROC_BROWSER_TEST_P(PDFDocumentHelperTest, DefaultImplementation) {
  EXPECT_FALSE(pdf_document_helper()->SupportsAnimation());
  EXPECT_FALSE(pdf_document_helper()->CreateDrawable());
  EXPECT_FALSE(pdf_document_helper()->ShouldShowQuickMenu());
  EXPECT_TRUE(pdf_document_helper()->GetSelectedText().empty());
}

// TODO(crbug.com/40268279): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFDocumentHelperTest);

}  // namespace pdf
