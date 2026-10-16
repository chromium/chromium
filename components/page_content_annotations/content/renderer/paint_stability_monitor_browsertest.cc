// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/renderer/paint_stability_monitor.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/page_content_annotations/content/renderer/page_stability_monitor.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_stability_monitor_delegate.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace page_content_annotations {

class PaintStabilityMonitorTest : public content::RenderViewTest {
 public:
  PaintStabilityMonitorTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPageSettledMonitor,
          {{features::kPaintStabilityInitialPaintTimeout.name, "1000ms"},
           {features::kPaintStabilitySubsequentPaintTimeout.name, "500ms"}}},
         {features::kPaintStabilityDeferInitialPaintTimeout, {}},
         {features::kPageStabilityExitStartDelayOnFirstPaint, {}}},
        /*disabled_features=*/{});
  }

 protected:
  static base::TimeDelta GetInitialPaintTimeout() {
    return features::kPaintStabilityInitialPaintTimeout.Get();
  }

  static base::TimeDelta GetSubsequentPaintTimeout() {
    return features::kPaintStabilitySubsequentPaintTimeout.Get();
  }

  void Render() {
    GetWebFrameWidget()->UpdateAllLifecyclePhases(
        blink::DocumentUpdateReason::kTest);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PaintStabilityMonitorTest, NoContentfulPaint) {
  // The button doesn't trigger a contentful paint. The `PaintStabilityMonitor`
  // should reach stability after the initial paint timeout expires.
  LoadHTML(R"HTML(
      <button id="target" onclick="handleClick()">Press Me</button>
      <div id="content"></div>
      <script>
        var clickCount = 0;
        function handleClick() { ++clickCount; }
      </script>
    )HTML");

  bool did_reach_paint_stability = false;
  {
    std::unique_ptr<PaintStabilityMonitor> monitor =
        PaintStabilityMonitor::Create(*GetMainRenderFrame(),
                                      /*delegate=*/nullptr);

    bool result = SimulateElementClick("target");
    ASSERT_TRUE(result);

    int value;
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"clickCount", &value));
    EXPECT_EQ(value, 1);

    monitor->Start();
    monitor->WaitForStable(base::BindLambdaForTesting(
        [&]() { did_reach_paint_stability = true; }));
    EXPECT_FALSE(did_reach_paint_stability);

    Render();

    task_environment_.FastForwardBy(GetInitialPaintTimeout() -
                                    base::Milliseconds(1));
    EXPECT_FALSE(did_reach_paint_stability);
    task_environment_.FastForwardBy(base::Milliseconds(1));
    EXPECT_TRUE(did_reach_paint_stability);
  }
}

TEST_F(PaintStabilityMonitorTest, SinglePaint) {
  // The button updates the UI synchronously. The `PaintStabilityMonitor` should
  // reach stability after the subsequent paint timeout expires after rendering
  // the content.
  LoadHTML(R"HTML(
      <button id="target" onclick="handleClick()">Press Me</button>
      <div id="content"></div>
      <script>
        var clickCount = 0;
        function handleClick() {
          ++clickCount;
          content.innerHTML = '<div>This is some content</div>';
        }
      </script>
    )HTML");

  bool did_reach_paint_stability = false;
  {
    std::unique_ptr<PaintStabilityMonitor> monitor =
        PaintStabilityMonitor::Create(*GetMainRenderFrame(),
                                      /*delegate=*/nullptr);

    bool result = SimulateElementClick("target");
    ASSERT_TRUE(result);

    int value;
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"clickCount", &value));
    EXPECT_EQ(value, 1);

    monitor->Start();
    monitor->WaitForStable(base::BindLambdaForTesting(
        [&]() { did_reach_paint_stability = true; }));
    EXPECT_FALSE(did_reach_paint_stability);

    Render();

    task_environment_.FastForwardBy(GetSubsequentPaintTimeout() -
                                    base::Milliseconds(1));
    EXPECT_FALSE(did_reach_paint_stability);
    task_environment_.FastForwardBy(base::Milliseconds(1));
    EXPECT_TRUE(did_reach_paint_stability);
  }
}

TEST_F(PaintStabilityMonitorTest, PaintStabilityReached_DelayedPaint) {
  // The button updates the UI asynchronously. The `PaintStabilityMonitor`
  // should reach stability after the subsequent paint timeout expires after
  // rendering the content.
  LoadHTML(R"HTML(
      <button id="target" onclick="handleClick()">Press Me</button>
      <div id="content"></div>
      <script>
        var clickCount = 0;
        var timerCount = 0;
        function handleClick() {
          ++clickCount;
          setTimeout(() => {
            ++timerCount;
            content.innerHTML = '<div>This is some content</div>';
          }, 700);
        }
      </script>
    )HTML");

  bool did_reach_paint_stability = false;
  {
    std::unique_ptr<PaintStabilityMonitor> monitor =
        PaintStabilityMonitor::Create(*GetMainRenderFrame(),
                                      /*delegate=*/nullptr);

    bool result = SimulateElementClick("target");
    ASSERT_TRUE(result);

    int value;
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"clickCount", &value));
    EXPECT_EQ(value, 1);

    monitor->Start();
    monitor->WaitForStable(base::BindLambdaForTesting(
        [&]() { did_reach_paint_stability = true; }));
    EXPECT_FALSE(did_reach_paint_stability);

    // Force a no-op frame and make sure the monitor is still in the initial
    // waiting state.
    Render();
    // This should cause the timeout to run. Make the value between the initial
    // and subsequent timeouts to ensure the monitor waits long enough waits
    // long enough.
    static constexpr base::TimeDelta kDelayedPaintTimeout =
        base::Milliseconds(700);
    ASSERT_LT(kDelayedPaintTimeout, GetInitialPaintTimeout());
    ASSERT_GT(kDelayedPaintTimeout, GetSubsequentPaintTimeout());

    task_environment_.FastForwardBy(kDelayedPaintTimeout);
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"timerCount", &value));
    EXPECT_EQ(value, 1);

    // Paint the new contents. This should cause the subsequent paint timer to
    // start.
    Render();

    task_environment_.FastForwardBy(GetSubsequentPaintTimeout() -
                                    base::Milliseconds(1));
    EXPECT_FALSE(did_reach_paint_stability);
    task_environment_.FastForwardBy(base::Milliseconds(1));
    EXPECT_TRUE(did_reach_paint_stability);
  }
}

TEST_F(PaintStabilityMonitorTest, PaintStabilityReached_MultiplePaints) {
  // The button updates the UI asynchronously, staggered over several tasks. The
  // `PaintStabilityMonitor` should reach stability after the subsequent paint
  // timeout expires after rendering all of the content.
  LoadHTML(R"HTML(
      <button id="target" onclick="handleClick()">Press Me</button>
      <div id="content"></div>
      <script>
        var clickCount = 0;
        var timerCount = 0;
        function handleClick() {
          ++clickCount;
          const task = () => {
            ++timerCount;
            content.innerHTML =
                `<div>This is some content: ${timerCount}</div>`;
            if (timerCount < 5) {
              setTimeout(task, 100);
            }
          };
          setTimeout(task, 100);
        }
      </script>
    )HTML");

  bool did_reach_paint_stability = false;
  {
    std::unique_ptr<PaintStabilityMonitor> monitor =
        PaintStabilityMonitor::Create(*GetMainRenderFrame(),
                                      /*delegate=*/nullptr);

    bool result = SimulateElementClick("target");
    ASSERT_TRUE(result);

    int value;
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"clickCount", &value));
    EXPECT_EQ(value, 1);

    monitor->Start();
    monitor->WaitForStable(base::BindLambdaForTesting(
        [&]() { did_reach_paint_stability = true; }));
    EXPECT_FALSE(did_reach_paint_stability);

    Render();

    // Choose a value that's lower than the subsequent paint timeout so each
    // paint resets the timer.
    static constexpr base::TimeDelta kDelayedPaintTimeout =
        base::Milliseconds(100);
    ASSERT_LT(kDelayedPaintTimeout, GetInitialPaintTimeout());
    ASSERT_LT(kDelayedPaintTimeout, GetSubsequentPaintTimeout());

    // Simulate a series of staggered paints.
    for (int i = 1; i <= 5; i++) {
      task_environment_.FastForwardBy(kDelayedPaintTimeout);
      EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"timerCount", &value));
      EXPECT_EQ(value, i);
      Render();
    }

    EXPECT_FALSE(did_reach_paint_stability);
    task_environment_.FastForwardBy(GetSubsequentPaintTimeout() -
                                    base::Milliseconds(1));
    EXPECT_FALSE(did_reach_paint_stability);
    task_environment_.FastForwardBy(base::Milliseconds(1));
    EXPECT_TRUE(did_reach_paint_stability);
  }
}

TEST_F(PaintStabilityMonitorTest, DelayedStabilityCallback) {
  // The button updates the UI synchronously. The `PaintStabilityMonitor` should
  // reach stability after the subsequent paint timeout expires after rendering
  // the content, and it should report to the callback as soon as it's
  // registered via (delayed) WaitForStable.
  LoadHTML(R"HTML(
      <button id="target" onclick="handleClick()">Press Me</button>
      <div id="content"></div>
      <script>
        var clickCount = 0;
        function handleClick() {
          ++clickCount;
          content.innerHTML = `<div>This is some content</div>`;
        }
      </script>
    )HTML");

  bool did_reach_paint_stability = false;
  {
    std::unique_ptr<PaintStabilityMonitor> monitor =
        PaintStabilityMonitor::Create(*GetMainRenderFrame(),
                                      /*delegate=*/nullptr);

    bool result = SimulateElementClick("target");
    ASSERT_TRUE(result);

    int value;
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"clickCount", &value));
    EXPECT_EQ(value, 1);

    // Start the monitor, but don't WaitForStable yet.
    monitor->Start();
    Render();

    // This should be enough to trigger stability.
    task_environment_.FastForwardBy(GetSubsequentPaintTimeout());
    monitor->WaitForStable(base::BindLambdaForTesting(
        [&]() { did_reach_paint_stability = true; }));
    // Fast-forwarding by a zero time delta runs any tasks that are ready to run
    // at the current virtual time. In this case, it ensures that the callback
    // passed to WaitForStable() is run if it was scheduled synchronously.
    task_environment_.FastForwardBy(base::TimeDelta());
    EXPECT_TRUE(did_reach_paint_stability);
  }
}

TEST_F(PaintStabilityMonitorTest, DelayedStabilityCallback_ResetTimer) {
  // The button updates the UI synchronously and asynchronously. The
  // `PaintStabilityMonitor` should reach stability after the subsequent paint
  // timeout expires after rendering the initial content, but since there is no
  // WaitForStable callback the heuristic is reset when the async DOM update is
  // painted.
  LoadHTML(R"HTML(
    <button id="target" onclick="handleClick()">Press Me</button>
    <div id="content"></div>
    <script>
      var clickCount = 0;
      var timerCount = 0;
      function handleClick() {
        ++clickCount;
        content.innerHTML = `<div>This is some content</div>`;
        setTimeout(() => {
          ++timerCount;
          content.innerHTML = `<div>This is different content</div>`;
        }, 2000);
      }
    </script>
  )HTML");

  bool did_reach_paint_stability = false;
  {
    std::unique_ptr<PaintStabilityMonitor> monitor =
        PaintStabilityMonitor::Create(*GetMainRenderFrame(),
                                      /*delegate=*/nullptr);

    bool result = SimulateElementClick("target");
    ASSERT_TRUE(result);

    int value;
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"clickCount", &value));
    EXPECT_EQ(value, 1);

    // Start the monitor, but don't WaitForStable yet.
    monitor->Start();
    Render();

    // Even though stability was reached, this will reset the timer/stability
    // flag since WaitForStable wasn't called.
    task_environment_.FastForwardBy(base::Seconds(2));
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"timerCount", &value));
    EXPECT_EQ(value, 1);

    Render();

    monitor->WaitForStable(base::BindLambdaForTesting(
        [&]() { did_reach_paint_stability = true; }));
    EXPECT_FALSE(did_reach_paint_stability);

    task_environment_.FastForwardBy(GetSubsequentPaintTimeout() -
                                    base::Milliseconds(1));
    EXPECT_FALSE(did_reach_paint_stability);
    task_environment_.FastForwardBy(base::Milliseconds(1));
    EXPECT_TRUE(did_reach_paint_stability);
  }
}

TEST_F(PaintStabilityMonitorTest, DelayedStabilityCallback_NoContentfulPaint) {
  // The button doesn't trigger a contentful paint. When `WaitForStable` is
  // delayed (e.g., due to `monitoring_start_delay_`), the initial paint timeout
  // should not start until `WaitForStable` is called.
  LoadHTML(R"HTML(
      <button id="target" onclick="handleClick()">Press Me</button>
      <div id="content"></div>
      <script>
        var clickCount = 0;
        function handleClick() { ++clickCount; }
      </script>
    )HTML");

  bool did_reach_paint_stability = false;
  {
    std::unique_ptr<PaintStabilityMonitor> monitor =
        PaintStabilityMonitor::Create(*GetMainRenderFrame(),
                                      /*delegate=*/nullptr);

    bool result = SimulateElementClick("target");
    ASSERT_TRUE(result);

    int value;
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"clickCount", &value));
    EXPECT_EQ(value, 1);

    // Start the monitor, but delay calling `WaitForStable` beyond
    // `GetInitialPaintTimeout()`.
    monitor->Start();
    Render();

    task_environment_.FastForwardBy(GetInitialPaintTimeout() * 2);

    monitor->WaitForStable(base::BindLambdaForTesting(
        [&]() { did_reach_paint_stability = true; }));
    task_environment_.FastForwardBy(base::TimeDelta());
    EXPECT_FALSE(did_reach_paint_stability);

    task_environment_.FastForwardBy(GetInitialPaintTimeout() -
                                    base::Milliseconds(1));
    EXPECT_FALSE(did_reach_paint_stability);
    task_environment_.FastForwardBy(base::Milliseconds(1));
    EXPECT_TRUE(did_reach_paint_stability);
  }
}

TEST_F(PaintStabilityMonitorTest,
       DelayedStabilityCallback_DelayedInitialPaint) {
  // The button updates the UI asynchronously after the
  // `monitoring_start_delay_` window. The monitor should not time out during
  // the delay before `WaitForStable()`, and should wait for the initial paint
  // after `WaitForStable()` followed by the subsequent paint timeout.
  LoadHTML(R"HTML(
      <button id="target" onclick="handleClick()">Press Me</button>
      <div id="content"></div>
      <script>
        var clickCount = 0;
        var timerCount = 0;
        function handleClick() {
          ++clickCount;
          setTimeout(() => {
            ++timerCount;
            content.innerHTML = '<div>This is some content</div>';
          }, 2500);
        }
      </script>
    )HTML");

  bool did_reach_paint_stability = false;
  {
    std::unique_ptr<PaintStabilityMonitor> monitor =
        PaintStabilityMonitor::Create(*GetMainRenderFrame(),
                                      /*delegate=*/nullptr);

    bool result = SimulateElementClick("target");
    ASSERT_TRUE(result);

    int value;
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"clickCount", &value));
    EXPECT_EQ(value, 1);

    monitor->Start();
    Render();

    // Simulate a 2s start delay before calling `WaitForStable()`.
    task_environment_.FastForwardBy(base::Seconds(2));
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"timerCount", &value));
    EXPECT_EQ(value, 0);

    monitor->WaitForStable(base::BindLambdaForTesting(
        [&]() { did_reach_paint_stability = true; }));
    task_environment_.FastForwardBy(base::TimeDelta());
    EXPECT_FALSE(did_reach_paint_stability);

    // Advance 500ms (< `GetInitialPaintTimeout()`) so the async DOM update runs
    // and paints.
    task_environment_.FastForwardBy(base::Milliseconds(500));
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"timerCount", &value));
    EXPECT_EQ(value, 1);
    Render();
    EXPECT_FALSE(did_reach_paint_stability);

    // Stability should now be reached after `GetSubsequentPaintTimeout()`.
    task_environment_.FastForwardBy(GetSubsequentPaintTimeout() -
                                    base::Milliseconds(1));
    EXPECT_FALSE(did_reach_paint_stability);
    task_environment_.FastForwardBy(base::Milliseconds(1));
    EXPECT_TRUE(did_reach_paint_stability);
  }
}

TEST_F(PaintStabilityMonitorTest, FirstContentfulPaintCallback) {
  LoadHTML(R"HTML(
      <button id="target" onclick="handleClick()">Press Me</button>
      <div id="content"></div>
      <script>
        var clickCount = 0;
        function handleClick() {
          ++clickCount;
          content.innerHTML = '<div>This is some content</div>';
        }
      </script>
    )HTML");

  bool did_invoke_contentful_paint_callback = false;
  bool did_reach_paint_stability = false;
  {
    std::unique_ptr<PaintStabilityMonitor> monitor =
        PaintStabilityMonitor::Create(*GetMainRenderFrame(),
                                      /*delegate=*/nullptr);

    bool result = SimulateElementClick("target");
    ASSERT_TRUE(result);

    int value;
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"clickCount", &value));
    EXPECT_EQ(value, 1);

    monitor->Start(base::BindLambdaForTesting([&]() {
      did_invoke_contentful_paint_callback = true;
      monitor->WaitForStable(base::BindLambdaForTesting(
          [&]() { did_reach_paint_stability = true; }));
    }));
    EXPECT_FALSE(did_invoke_contentful_paint_callback);

    Render();
    EXPECT_TRUE(did_invoke_contentful_paint_callback);
    EXPECT_FALSE(did_reach_paint_stability);

    // Since `WaitForStable()` was called inside the contentful paint callback,
    // stability should be reached after `GetSubsequentPaintTimeout()`.
    task_environment_.FastForwardBy(GetSubsequentPaintTimeout() -
                                    base::Milliseconds(1));
    EXPECT_FALSE(did_reach_paint_stability);
    task_environment_.FastForwardBy(base::Milliseconds(1));
    EXPECT_TRUE(did_reach_paint_stability);
  }
}

TEST_F(PaintStabilityMonitorTest,
       PageStabilityMonitor_EarlyExitStartDelayOnFirstPaint) {
  // When a contentful paint occurs during `observation_delay`
  // (`State::kMonitorStartDelay`), `PageStabilityMonitor` should cancel the
  // remaining start delay and transition immediately to monitoring.
  LoadHTML(R"HTML(
      <button id="target" onclick="handleClick()">Press Me</button>
      <div id="content"></div>
      <script>
        var clickCount = 0;
        var timerCount = 0;
        function handleClick() {
          ++clickCount;
          setTimeout(() => {
            ++timerCount;
            content.innerHTML = '<div>This is some content</div>';
          }, 500);
        }
      </script>
    )HTML");

  bool did_reach_stability = false;
  {
    PageStabilityMonitor monitor(
        *GetMainRenderFrame(), /*supports_paint_stability=*/true,
        std::make_unique<PageStabilityMonitorDelegate>());

    bool result = SimulateElementClick("target");
    ASSERT_TRUE(result);

    int value;
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"clickCount", &value));
    EXPECT_EQ(value, 1);

    // Use a 3s observation delay.
    monitor.NotifyWhenStable(
        /*observation_delay=*/base::Seconds(3),
        base::BindLambdaForTesting([&]() { did_reach_stability = true; }));

    // Advance 500ms (still well within the 3s observation delay) so the DOM
    // updates and triggers the first contentful paint.
    task_environment_.FastForwardBy(base::Milliseconds(500));
    EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"timerCount", &value));
    EXPECT_EQ(value, 1);
    Render();
    EXPECT_FALSE(did_reach_stability);

    // Because the first contentful paint exited the start delay early at
    // t = 500ms, stability should complete after the 1s min wait (at t = 1.5s
    // total) rather than waiting for the remaining 2.5s of the 3s observation
    // delay.
    task_environment_.FastForwardBy(features::kPageStabilityMinWait.Get() -
                                    base::Milliseconds(1));
    EXPECT_FALSE(did_reach_stability);
    task_environment_.FastForwardBy(base::Milliseconds(1));
    EXPECT_TRUE(did_reach_stability);
  }
}

}  // namespace page_content_annotations
