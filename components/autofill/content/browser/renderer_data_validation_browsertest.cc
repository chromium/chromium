// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/renderer_data_validation.h"

#include <limits>
#include <memory>

#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {
namespace {

class RendererDataValidationBrowserTest : public content::ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    content::IsolateAllSitesForTesting(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1.0");
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Sets up a page `a.com/main.html` with a cross-origin iframe
// `b.com/outer.html` at (x=100, y=100, width=200, height=200) and a nested
// cross-origin iframe `c.com/inner.html` at (x=-500, y=-500, width=2000,
// height=2000) relative to `outer_frame`, i.e., (x=-400, y=-400, width=2000,
// height=2000) in root coordinates.
class RendererDataValidationNestedFramesBrowserTest
    : public RendererDataValidationBrowserTest {
 public:
  void SetUpOnMainThread() override {
    RendererDataValidationBrowserTest::SetUpOnMainThread();
    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/html; charset=utf-8");
          if (request.relative_url == "/main.html") {
            response->set_content(base::ReplaceStringPlaceholders(
                R"(
                  <meta name="viewport"
                        content="width=device-width, initial-scale=1">
                  <style>* { margin: 0; padding: 0; border: 0; }</style>
                  <iframe src="$1"
                          style="position: absolute; left: 100px; top: 100px;
                                 width: 200px; height: 200px;"></iframe>
                )",
                {embedded_test_server()->GetURL("b.com", "/outer.html").spec()},
                nullptr));
            return response;
          }
          if (request.relative_url == "/outer.html") {
            response->set_content(base::ReplaceStringPlaceholders(
                R"(
                  <style>* { margin: 0; padding: 0; border: 0; }</style>
                  <iframe src="$1"
                          style="position: absolute; left: -500px; top: -500px;
                                 width: 2000px; height: 2000px;"></iframe>
                )",
                {embedded_test_server()->GetURL("c.com", "/inner.html").spec()},
                nullptr));
            return response;
          }
          if (request.relative_url == "/inner.html") {
            response->set_content("<body></body>");
            return response;
          }
          return nullptr;
        }));
    ASSERT_TRUE(embedded_test_server()->Start());

    ASSERT_TRUE(content::NavigateToURL(
        shell(), embedded_test_server()->GetURL("a.com", "/main.html")));

    ASSERT_TRUE(outer_frame());
    ASSERT_TRUE(inner_frame());
    ASSERT_TRUE(outer_frame()->IsCrossProcessSubframe());
    ASSERT_TRUE(inner_frame()->IsCrossProcessSubframe());

    content::WaitForHitTestData(outer_frame());
    content::WaitForHitTestData(inner_frame());
  }

  content::RenderFrameHost* outer_frame() {
    return content::ChildFrameAt(shell(), 0);
  }

  content::RenderFrameHost* inner_frame() {
    return content::ChildFrameAt(outer_frame(), 0);
  }
};

// Tests that bounds in a nested cross-origin iframe that overlap with an
// ancestor frame's bounds are clamped to all ancestor frames' bounds in root
// coordinates (crbug.com/574502257).
IN_PROC_BROWSER_TEST_F(RendererDataValidationNestedFramesBrowserTest,
                       TransformToRootCoordinatesClampsOverlappingBounds) {
  // In root coordinates before clamping, gfx::RectF(450, 450, 100, 100) is
  // (x=50, y=50, width=100, height=100).
  EXPECT_EQ(TransformToRootCoordinates(*inner_frame(),
                                       gfx::RectF(450, 450, 100, 100)),
            gfx::RectF(100, 100, 50, 50));
}

// Tests that bounds in a nested cross-origin iframe that are disjoint from an
// ancestor frame's bounds collapse to an empty rectangle at the closest point
// of the ancestor frame's bounds rather than gfx::RectF(0, 0, 0, 0).
IN_PROC_BROWSER_TEST_F(RendererDataValidationNestedFramesBrowserTest,
                       TransformToRootCoordinatesClampsDisjointBounds) {
  // In root coordinates before clamping, (450, 800, 100, 20) is
  // (x=50, y=400, width=100, height=20), which is disjoint from `outer_frame`.
  EXPECT_EQ(
      TransformToRootCoordinates(*inner_frame(), gfx::RectF(450, 800, 100, 20)),
      gfx::RectF(100, 300, 0, 0));
}

// Tests that non-finite coordinates do not bypass ancestor frame bounds
// clamping but collapse to an empty rectangle inside all ancestor frames'
// bounds (crbug.com/587098427).
IN_PROC_BROWSER_TEST_F(RendererDataValidationNestedFramesBrowserTest,
                       TransformToRootCoordinatesClampsNonFiniteBounds) {
  constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
  constexpr float kInf = std::numeric_limits<float>::infinity();
  constexpr float kMaxFloat = std::numeric_limits<float>::max();

  // Interior finite bounds map without unintended clamping. The coordinate
  // transform saturates extreme and infinite coordinates to finite values,
  // which are then clamped to `outer_frame`.
  EXPECT_EQ(
      TransformToRootCoordinates(*inner_frame(), gfx::RectF(520, 530, 40, 20)),
      gfx::RectF(120, 130, 40, 20));
  EXPECT_EQ(TransformToRootCoordinates(
                *inner_frame(), gfx::RectF(kMaxFloat, 550, kMaxFloat, 20)),
            gfx::RectF(300, 150, 0, 0));
  EXPECT_EQ(TransformToRootCoordinates(*inner_frame(),
                                       gfx::RectF(-3e38f, 550, kInf, 20)),
            gfx::RectF(100, 150, 200, 20));

  // Main frame's `TransformPointToRootCoordSpaceF` is the identity function, so
  // non-finite coordinates reach the clamping logic directly and collapse to
  // the main frame's origin.
  content::RenderFrameHost* main_frame = outer_frame()->GetMainFrame();
  ASSERT_TRUE(main_frame);
  EXPECT_EQ(
      TransformToRootCoordinates(*main_frame, gfx::RectF(kNaN, -50, 100, 20)),
      gfx::RectF(0, 0, 0, 0));
  EXPECT_EQ(
      TransformToRootCoordinates(*main_frame, gfx::RectF(kNaN, 50, 100, 20)),
      gfx::RectF(0, 0, 0, 0));

  // Create a 3D-transformed cross-origin child frame in `outer_frame` so
  // `TransformPointToRootCoordSpaceF` uses a full `Matrix44` (`matrix_[0][1] ==
  // 0`), verifying that finite coordinates whose `x + width` overflows `float`
  // to `+Inf` (and may corrupt the orthogonal `y` axis via `+Inf * 0 == NaN`)
  // still end up inside `outer_frame`.
  ASSERT_TRUE(content::ExecJs(outer_frame(), content::JsReplace(
                                                 R"(
            new Promise(resolve => {
              const f = document.createElement('iframe');
              f.style.cssText =
                  'position: absolute; left: -500px; top: -500px; ' +
                  'width: 2000px; height: 2000px; ' +
                  'transform: translateZ(1px);';
              f.src = $1;
              f.onload = resolve;
              document.body.appendChild(f);
            });
          )",
                                                 embedded_test_server()->GetURL(
                                                     "c.com", "/inner.html"))));
  content::RenderFrameHost* transformed_inner_frame =
      content::ChildFrameAt(outer_frame(), 1);
  ASSERT_TRUE(transformed_inner_frame);
  ASSERT_TRUE(transformed_inner_frame->IsCrossProcessSubframe());
  content::WaitForHitTestData(transformed_inner_frame);

  EXPECT_EQ(TransformToRootCoordinates(*transformed_inner_frame,
                                       gfx::RectF(3e38f, 550, 3e38f, 20)),
            gfx::RectF(300, 100, 0, 0));

  // Create a `display: none` cross-origin child frame in `outer_frame` so
  // `HitTestQuery` has no target entry for it and
  // `TransformPointToRootCoordSpaceF` falls back without clamping, delivering
  // non-finite coordinates to `outer_frame`'s clamp step.
  ASSERT_TRUE(content::ExecJs(outer_frame(), content::JsReplace(
                                                 R"(
            new Promise(resolve => {
              const f = document.createElement('iframe');
              f.style.display = 'none';
              f.src = $1;
              f.onload = resolve;
              document.body.appendChild(f);
            });
          )",
                                                 embedded_test_server()->GetURL(
                                                     "c.com", "/inner.html"))));
  content::RenderFrameHost* hidden_inner_frame =
      content::ChildFrameAt(outer_frame(), 2);
  ASSERT_TRUE(hidden_inner_frame);
  ASSERT_TRUE(hidden_inner_frame->IsCrossProcessSubframe());

  // `hidden_inner_frame` has 0x0 view bounds at (0, 0), so non-finite inputs
  // collapse to (0, 0, 0, 0) and are then clamped to `outer_frame`'s closest
  // point at (100, 100, 0, 0) rather than preserving NaN.
  EXPECT_EQ(TransformToRootCoordinates(*hidden_inner_frame,
                                       gfx::RectF(kNaN, 150, 100, 20)),
            gfx::RectF(100, 100, 0, 0));
  EXPECT_EQ(TransformToRootCoordinates(*hidden_inner_frame,
                                       gfx::RectF(kNaN, kInf, 0, 0)),
            gfx::RectF(100, 100, 0, 0));
  EXPECT_EQ(TransformToRootCoordinates(*hidden_inner_frame,
                                       gfx::RectF(150, kNaN, 20, 100)),
            gfx::RectF(100, 100, 0, 0));
  EXPECT_EQ(TransformToRootCoordinates(*hidden_inner_frame,
                                       gfx::RectF(-kInf, 150, kInf, 20)),
            gfx::RectF(100, 100, 0, 0));
}

}  // namespace
}  // namespace autofill
