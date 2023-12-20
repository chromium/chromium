// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/eye_dropper/eye_dropper_view.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"

class EyeDropperViewAuraBrowserTest : public InProcessBrowserTest {
 public:
  EyeDropperViewAuraBrowserTest() = default;

  class EyeDropperListener : public content::EyeDropperListener {
   public:
    void ColorSelected(SkColor color) override {}
    void ColorSelectionCanceled() override { is_canceled_ = true; }
    bool IsCanceled() const { return is_canceled_; }

   private:
    bool is_canceled_ = false;
  };
};

IN_PROC_BROWSER_TEST_F(EyeDropperViewAuraBrowserTest, ActiveChangeCancel) {
  EyeDropperListener listener;
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("about:blank"), 1);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<content::EyeDropper> eye_dropper =
      ShowEyeDropper(web_contents->GetPrimaryMainFrame(), &listener);
  EXPECT_FALSE(listener.IsCanceled());
  web_contents->GetRenderWidgetHostView()->Hide();
  EXPECT_TRUE(listener.IsCanceled());
}

IN_PROC_BROWSER_TEST_F(EyeDropperViewAuraBrowserTest, InactiveWindow) {
  EyeDropperListener listener;
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("about:blank"), 1);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetRenderWidgetHostView()->Hide();
  std::unique_ptr<content::EyeDropper> eye_dropper =
      ShowEyeDropper(web_contents->GetPrimaryMainFrame(), &listener);
  EXPECT_EQ(eye_dropper, nullptr);
}
