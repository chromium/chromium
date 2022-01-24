// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/projector/projector_annotator_controller.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/test/mock_projector_controller.h"
#include "ash/webui/projector_app/annotator_tool.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
#include "chrome/browser/ui/ash/capture_mode/recording_overlay_view_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"

namespace ash {

class RecordingOverlayView;

class ProjectorAnnotatorControllerTest : public InProcessBrowserTest {
 public:
  ProjectorAnnotatorControllerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kProjector,
                              features::kProjectorAnnotator},
        /*disabled_features=*/{});
  }
  ProjectorAnnotatorControllerTest(const ProjectorAnnotatorControllerTest&) =
      delete;
  ProjectorAnnotatorControllerTest& operator=(
      const ProjectorAnnotatorControllerTest&) = delete;
  ~ProjectorAnnotatorControllerTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    scoped_resetter_ =
        std::make_unique<ProjectorController::ScopedInstanceResetterForTest>();
    controller_ = std::make_unique<MockProjectorController>();

    overlay_view_ =
        ChromeCaptureModeDelegate::Get()->CreateRecordingOverlayView();
    content::WebContents* web_contents =
        GetOverlayView()->GetWebViewForTest()->GetWebContents();
    EXPECT_EQ(web_contents->GetWebUI()->GetHandlersForTesting()->size(), 1u);
    // Wait for the annotator contents view to load and the WebUI listeners to
    // register.
    content::TestNavigationObserver navigation_observer(web_contents);
    navigation_observer.Wait();
  }

  void TearDownOnMainThread() override {
    overlay_view_.reset();
    controller_.reset();
    scoped_resetter_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  RecordingOverlayViewImpl* GetOverlayView() {
    return static_cast<RecordingOverlayViewImpl*>(overlay_view_.get());
  }

  MockProjectorController* GetController() { return controller_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ProjectorController::ScopedInstanceResetterForTest>
      scoped_resetter_;
  std::unique_ptr<MockProjectorController> controller_;
  std::unique_ptr<RecordingOverlayView> overlay_view_;
};

IN_PROC_BROWSER_TEST_F(ProjectorAnnotatorControllerTest, SetTool) {
  AnnotatorTool expected_tool;
  expected_tool.color = SK_ColorBLACK;
  expected_tool.size = 5;
  expected_tool.type = AnnotatorToolType::kPen;

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*GetController(), OnToolSet(expected_tool))
      .WillOnce([&quit_closure] { quit_closure.Run(); });
  ProjectorAnnotatorController::Get()->SetTool(expected_tool);
  run_loop.Run();
}

// This edge case can happen if the user navigates to
// chrome://projector/annotator/annotator_embedder.html while doing a screen
// capture with annotator tools enabled.
IN_PROC_BROWSER_TEST_F(ProjectorAnnotatorControllerTest, TwoAnnotators) {
  AnnotatorTool expected_tool;
  expected_tool.color = SK_ColorGREEN;
  expected_tool.size = 6;
  expected_tool.type = AnnotatorToolType::kMarker;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(kChromeUITrustedAnnotatorUrl)));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // We enforce only one AnnotatorMessageHandler at a time. The second annotator
  // does not have a message handler.
  EXPECT_TRUE(tab->GetWebUI()->GetHandlersForTesting()->empty());

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*GetController(), OnToolSet(expected_tool))
      .WillOnce([&quit_closure] { quit_closure.Run(); });
  ProjectorAnnotatorController::Get()->SetTool(expected_tool);
  run_loop.Run();
}

}  // namespace ash
