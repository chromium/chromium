// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_web_contents_helper.h"

#include "base/test/metrics/user_action_tester.h"
#include "components/pdf/browser/pdf_web_contents_helper_client.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "pdf/mojom/pdf.mojom.h"
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
};

}  // namespace

// A mock PDFWebContentsHelperClient.
class TestPDFWebContentsHelperClient : public PDFWebContentsHelperClient {
 public:
  TestPDFWebContentsHelperClient() = default;
  ~TestPDFWebContentsHelperClient() override = default;
  TestPDFWebContentsHelperClient(const TestPDFWebContentsHelperClient&) =
      delete;
  TestPDFWebContentsHelperClient& operator=(
      const TestPDFWebContentsHelperClient&) = delete;

 private:
  // PDFWebContentsHelperClient:
  content::RenderFrameHost* FindPdfFrame(
      content::WebContents* contents) override {
    return contents->GetPrimaryMainFrame();
  }

  void UpdateContentRestrictions(content::RenderFrameHost* render_frame_host,
                                 int content_restrictions) override {}
  void OnPDFHasUnsupportedFeature(content::WebContents* contents) override {}
  void OnSaveURL(content::WebContents* contents) override {}
  void SetPluginCanSave(content::RenderFrameHost* render_frame_host,
                        bool can_save) override {}
};

// A mock content::TouchSelectionControllerClientManager.
class TestTouchSelectionControllerClientManager
    : public content::TouchSelectionControllerClientManager {
 public:
  TestTouchSelectionControllerClientManager() = default;
  ~TestTouchSelectionControllerClientManager() override = default;
  TestTouchSelectionControllerClientManager(
      const TestTouchSelectionControllerClientManager&) = delete;
  TestTouchSelectionControllerClientManager& operator=(
      const TestTouchSelectionControllerClientManager&) = delete;

  const gfx::SelectionBound& GetSelectionBoundStart() { return start_; }

  const gfx::SelectionBound& GetSelectionBoundEnd() { return end_; }

 private:
  // content::TouchSelectionControllerClientManager:
  void DidStopFlinging() override {}

  void OnSwipeToMoveCursorBegin() override {}

  void OnSwipeToMoveCursorEnd() override {}

  void OnClientHitTestRegionUpdated(
      ui::TouchSelectionControllerClient* client) override {}

  void UpdateClientSelectionBounds(
      const gfx::SelectionBound& start,
      const gfx::SelectionBound& end,
      ui::TouchSelectionControllerClient* client,
      ui::TouchSelectionMenuClient* menu_client) override {
    start_ = start;
    end_ = end;
  }

  void InvalidateClient(ui::TouchSelectionControllerClient* client) override {}

  ui::TouchSelectionController* GetTouchSelectionController() override {
    return nullptr;
  }

  void AddObserver(content::TouchSelectionControllerClientManager::Observer*
                       observer) override {}

  void RemoveObserver(content::TouchSelectionControllerClientManager::Observer*
                          observer) override {}

  gfx::SelectionBound start_;
  gfx::SelectionBound end_;
};

class PDFWebContentsHelperTest : public content::ContentBrowserTest {
 public:
  PDFWebContentsHelperTest() = default;
  ~PDFWebContentsHelperTest() override = default;

 protected:
  void SelectionChanged(const gfx::PointF& left,
                        int32_t left_height,
                        const gfx::PointF& right,
                        int32_t right_height) {
    pdf_web_contents_helper()->SelectionChanged(left, left_height, right,
                                                right_height);
  }

  PDFWebContentsHelper* pdf_web_contents_helper() {
    return PDFWebContentsHelper::FromWebContents(shell()->web_contents());
  }

  TestTouchSelectionControllerClientManager*
  touch_selection_controller_client_manager() {
    return touch_selection_controller_client_manager_.get();
  }

  content::RenderWidgetHostView* GetRenderWidgetHostView() {
    return shell()->web_contents()->GetRenderWidgetHostView();
  }

  // content::ContentBrowserTest:
  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    PDFWebContentsHelper::CreateForWebContentsWithClient(
        shell()->web_contents(),
        std::make_unique<TestPDFWebContentsHelperClient>());
    touch_selection_controller_client_manager_ =
        std::make_unique<TestTouchSelectionControllerClientManager>();
    pdf_web_contents_helper()->touch_selection_controller_client_manager_ =
        touch_selection_controller_client_manager_.get();
  }

 private:
  std::unique_ptr<TestTouchSelectionControllerClientManager>
      touch_selection_controller_client_manager_;
};

IN_PROC_BROWSER_TEST_F(PDFWebContentsHelperTest, SetListenerTwice) {
  NiceMock<FakePdfListener> listener;

  {
    mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
    pdf_web_contents_helper()->SetListener(receiver.BindNewPipeAndPassRemote());
  }

  {
    mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
    pdf_web_contents_helper()->SetListener(receiver.BindNewPipeAndPassRemote());
  }
}

// Tests that select-changed on a pdf text brings up selection handles and the
// quick menu in the reasonable position.
IN_PROC_BROWSER_TEST_F(PDFWebContentsHelperTest, SelectionChanged) {
  TestTouchSelectionControllerClientManager* manager =
      touch_selection_controller_client_manager();
  gfx::SelectionBound start = manager->GetSelectionBoundStart();
  gfx::SelectionBound end = manager->GetSelectionBoundEnd();

  EXPECT_EQ(gfx::RectF(), gfx::RectFBetweenSelectionBounds(start, end));
  EXPECT_EQ(gfx::RectF(), gfx::RectFBetweenVisibleSelectionBounds(start, end));

  constexpr gfx::PointF kLeft(1.0f, 1.0f);
  constexpr gfx::PointF kRight(5.0f, 5.0f);
  constexpr int32_t kLeftHeight = 2;
  constexpr int32_t kRightHeight = 2;
  SelectionChanged(kLeft, kLeftHeight, kRight, kRightHeight);

  start = manager->GetSelectionBoundStart();
  end = manager->GetSelectionBoundEnd();

  gfx::PointF origin_f;
  content::RenderWidgetHostView* view = GetRenderWidgetHostView();
  if (view)
    origin_f = view->TransformPointToRootCoordSpaceF(gfx::PointF());

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
}

// When selecting something, only the copy command id should be enabled.
IN_PROC_BROWSER_TEST_F(PDFWebContentsHelperTest,
                       IsCommandIdEnabledCopyEnabled) {
  EXPECT_FALSE(
      pdf_web_contents_helper()->IsCommandIdEnabled(ui::TouchEditable::kCut));
  EXPECT_FALSE(
      pdf_web_contents_helper()->IsCommandIdEnabled(ui::TouchEditable::kCopy));

  constexpr gfx::PointF kLeft(1.0f, 1.0f);
  constexpr gfx::PointF kRight(5.0f, 5.0f);
  constexpr int32_t kLeftHeight = 2;
  constexpr int32_t kRightHeight = 2;
  SelectionChanged(kLeft, kLeftHeight, kRight, kRightHeight);

  EXPECT_FALSE(
      pdf_web_contents_helper()->IsCommandIdEnabled(ui::TouchEditable::kCut));
  EXPECT_TRUE(
      pdf_web_contents_helper()->IsCommandIdEnabled(ui::TouchEditable::kCopy));
}

// Test that the copy command executes.
IN_PROC_BROWSER_TEST_F(PDFWebContentsHelperTest, ExecuteCommandCopy) {
  content::NavigateToURL(shell(), GURL());

  base::UserActionTester action_tester;
  EXPECT_EQ(0, action_tester.GetActionCount("Copy"));

  pdf_web_contents_helper()->ExecuteCommand(ui::TouchEditable::kCopy, 0);

  EXPECT_EQ(1, action_tester.GetActionCount("Copy"));
}

IN_PROC_BROWSER_TEST_F(PDFWebContentsHelperTest, DefaultImplementation) {
  EXPECT_FALSE(pdf_web_contents_helper()->SupportsAnimation());
  EXPECT_FALSE(pdf_web_contents_helper()->CreateDrawable());
  EXPECT_FALSE(pdf_web_contents_helper()->ShouldShowQuickMenu());
  EXPECT_TRUE(pdf_web_contents_helper()->GetSelectedText().empty());
}

}  // namespace pdf
