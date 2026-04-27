// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace views {

class WebViewInteractiveUiTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // Don't open the default browser window - we're testing a WebView in a
    // separate widget.
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }
};

// TODO(crbug.com/503006729): this test times out on Windows.
#if !BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(WebViewInteractiveUiTest, MouseMoveEventDelivered) {
  // Create a widget with a WebView.
  TestingProfile testing_profile;
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET);
  params.bounds = gfx::Rect(0, 0, 400, 300);
  widget->Init(std::move(params));

  auto* web_view = widget->SetClientContentsView(
      std::make_unique<WebView>(&testing_profile));

  // Load a simple page with a mousemove listener.
  GURL url(
      "data:text/html,"
      "<html><body><script>"
      "  window.mouseMoveReceived = false;"
      "  window.mouseX = -1;"
      "  window.mouseY = -1;"
      "  document.addEventListener('mousemove', (e) => {"
      "    window.mouseMoveReceived = true;"
      "    window.mouseX = e.clientX;"
      "    window.mouseY = e.clientY;"
      "  });"
      "</script></body></html>");
  web_view->LoadInitialURL(url);

  widget->Show();

  content::WebContents* web_contents = web_view->GetWebContents();
  ASSERT_TRUE(web_contents);

  // Wait for load to complete.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Wait for the primary main frame to become ready for input.
  content::ReadyForInputObserver activation_observer(web_contents);
  activation_observer.Wait();

  // Move mouse over the WebView.
  gfx::Point point = web_view->GetBoundsInScreen().CenterPoint();

  base::RunLoop run_loop;
  ui_controls::SendMouseMoveNotifyWhenDone(
      point.x(), point.y(), run_loop.QuitClosure(), widget->GetNativeWindow());
  run_loop.Run();

  // Verify listener was triggered.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    auto result = content::EvalJs(web_contents, "window.mouseMoveReceived");
    return result.is_ok() && result.is_bool() && result.ExtractBool();
  }));

  int mouse_x = content::EvalJs(web_contents, "window.mouseX").ExtractInt();
  int mouse_y = content::EvalJs(web_contents, "window.mouseY").ExtractInt();

  // The point we sent was the center of the WebView.
  // We calculate the expected coordinates using the local bounds of the
  // WebView.
  int expected_x = web_view->GetLocalBounds().width() / 2;
  int expected_y = web_view->GetLocalBounds().height() / 2;

  EXPECT_NEAR(expected_x, mouse_x, 2);
  EXPECT_NEAR(expected_y, mouse_y, 2);
}
#endif  // !BUILDFLAG(IS_WIN)

}  // namespace views
