// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_web_contents_helper.h"

#include "components/pdf/browser/pdf_web_contents_helper_client.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "ui/gfx/selection_bound.h"

namespace pdf {

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
  void UpdateContentRestrictions(content::WebContents* contents,
                                 int content_restrictions) override {}
  void OnPDFHasUnsupportedFeature(content::WebContents* contents) override {}
  void OnSaveURL(content::WebContents* contents) override {}
  void SetPluginCanSave(content::WebContents* contents,
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

// Tests that select-changed on a pdf text brings up selection handles and the
// quick menu in the reasonable position.
IN_PROC_BROWSER_TEST_F(PDFWebContentsHelperTest, SelectionChanged) {
  TestTouchSelectionControllerClientManager* manager =
      touch_selection_controller_client_manager();
  gfx::SelectionBound start = manager->GetSelectionBoundStart();
  gfx::SelectionBound end = manager->GetSelectionBoundEnd();

  EXPECT_EQ(gfx::RectF(), gfx::RectFBetweenSelectionBounds(start, end));
  EXPECT_EQ(gfx::RectF(), gfx::RectFBetweenVisibleSelectionBounds(start, end));

  gfx::PointF left(1.0f, 1.0f);
  gfx::PointF right(5.0f, 5.0f);
  int32_t left_height = 2;
  int32_t right_height = 2;
  SelectionChanged(left, left_height, right, right_height);

  start = manager->GetSelectionBoundStart();
  end = manager->GetSelectionBoundEnd();

  gfx::PointF origin_f;
  content::RenderWidgetHostView* view = GetRenderWidgetHostView();
  if (view)
    origin_f = view->TransformPointToRootCoordSpaceF(gfx::PointF());

  gfx::PointF edge_start(left.x() + origin_f.x(), left.y() + origin_f.y());
  gfx::PointF edge_end(left.x() + origin_f.x(),
                       left.y() + origin_f.y() + left_height);
  gfx::SelectionBound expected_start;
  expected_start.SetEdge(edge_start, edge_end);
  expected_start.SetVisibleEdge(edge_start, edge_end);

  edge_start = gfx::PointF(right.x() + origin_f.x(), right.y() + origin_f.y());
  edge_end = gfx::PointF(right.x() + origin_f.x(),
                         right.y() + origin_f.y() + right_height);
  gfx::SelectionBound expected_end;
  expected_end.SetEdge(edge_start, edge_end);
  expected_end.SetVisibleEdge(edge_start, edge_end);

  bool has_selection = expected_start != expected_end;
  expected_start.set_visible(has_selection);
  expected_end.set_visible(has_selection);
  expected_start.set_type(has_selection ? gfx::SelectionBound::LEFT
                                        : gfx::SelectionBound::EMPTY);
  expected_end.set_type(has_selection ? gfx::SelectionBound::RIGHT
                                      : gfx::SelectionBound::EMPTY);

  EXPECT_EQ(expected_start, start);
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

}  // namespace pdf
