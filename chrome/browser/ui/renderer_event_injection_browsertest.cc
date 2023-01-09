// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/display/display_switches.h"

namespace {

// This test verifies renderer event injection works. That is, it verifies
// a renderer can inject events and that they're received by content. It's in
// the Chrome side (not content) so that it can verify events work correctly
// when all of Chrome is brought up. This is especially important for ChromeOS,
// as content tests do not bring up the ChromeOS window-manager (ash).
//
// The parameter is how the display is configured, and is only applicable to
// ChromeOS.
class RendererEventInjectionTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<const char*> {
 public:
  RendererEventInjectionTest() {}

  RendererEventInjectionTest(const RendererEventInjectionTest&) = delete;
  RendererEventInjectionTest& operator=(const RendererEventInjectionTest&) =
      delete;

  ~RendererEventInjectionTest() override {}

  // InProcessBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSyntheticPointerActions);
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableRendererBackgrounding);
    command_line->AppendSwitch(cc::switches::kEnableGpuBenchmarking);
    // kHostWindowBounds is unique to ChromeOS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitchASCII(switches::kHostWindowBounds, GetParam());
#endif
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Detects when a touch press is received.
class TouchEventObserver
    : public content::RenderWidgetHost::InputEventObserver {
 public:
  TouchEventObserver(const gfx::Point& location,
                     base::RepeatingClosure quit_closure)
      : expected_location_(location), quit_closure_(std::move(quit_closure)) {}

  TouchEventObserver(const TouchEventObserver&) = delete;
  TouchEventObserver& operator=(const TouchEventObserver&) = delete;

  ~TouchEventObserver() override = default;

 private:
  // content::RenderWidgetHost::InputEventObserver:
  void OnInputEvent(const blink::WebInputEvent& event) override {
    if (blink::WebInputEvent::IsTouchEventType(event.GetType())) {
      const blink::WebTouchEvent& web_touch =
          static_cast<const blink::WebTouchEvent&>(event);
      if (event.GetType() == blink::WebInputEvent::Type::kTouchStart) {
        for (unsigned i = 0; i < web_touch.touches_length; i++) {
          const blink::WebTouchPoint& touch_point = web_touch.touches[i];
          const gfx::Point location(
              static_cast<int>(touch_point.PositionInWidget().x()),
              static_cast<int>(touch_point.PositionInWidget().y()));
          if (touch_point.state == blink::WebTouchPoint::State::kStatePressed &&
              location == expected_location_) {
            quit_closure_.Run();
          }
        }
      }
    }
  }

  const gfx::Point expected_location_;
  base::RepeatingClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_P(RendererEventInjectionTest, TestRootTransform) {
  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  base::RunLoop run_loop;
  content::RenderWidgetHost* rwh =
      main_contents->GetRenderWidgetHostView()->GetRenderWidgetHost();
  TouchEventObserver touch_observer(gfx::Point(100, 150),
                                    run_loop.QuitClosure());
  rwh->AddInputEventObserver(&touch_observer);
  EXPECT_TRUE(ExecJs(main_contents,
                     "chrome.gpuBenchmarking.tap(100, 150, ()=>{}, "
                     "50, chrome.gpuBenchmarking.TOUCH_INPUT);"));
  run_loop.Run();
  rwh->RemoveInputEventObserver(&touch_observer);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// This configures the display in various interesting ways for ChromeOS. In
// particular, it tests rotation "/r" and a scale factor of 2 "*2".
INSTANTIATE_TEST_SUITE_P(
    All,
    RendererEventInjectionTest,
    ::testing::Values("1200x800", "1200x800/r", "1200x800*2", "1200x800*2/r"));
#else
INSTANTIATE_TEST_SUITE_P(All,
                         RendererEventInjectionTest,
                         ::testing::Values(""));
#endif

}  // namespace
