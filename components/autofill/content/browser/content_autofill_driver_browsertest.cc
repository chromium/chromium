// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver.h"

#include <memory>
#include <string>

#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "components/autofill/content/browser/content_autofill_driver_test_api.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::Property;

class ContentAutofillDriverBrowserTest : public content::ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
};

// Sets up a page `a.com/main.html` with a cross-origin iframe
// `b.com/outer.html` at (x=100, y=100, width=200, height=200) and a nested
// cross-origin iframe `c.com/inner.html` at (x=-500, y=-500, width=2000,
// height=2000) relative to `outer_frame`, i.e., (x=-400, y=-400, width=2000,
// height=2000) in root coordinates.
class ContentAutofillDriverNestedFramesBrowserTest
    : public ContentAutofillDriverBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ContentAutofillDriverBrowserTest::SetUpOnMainThread();
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

  ContentAutofillDriver& inner_driver() {
    return *ContentAutofillDriver::GetForRenderFrameHost(inner_frame());
  }
};

// Tests that field bounds in a nested cross-origin iframe that overlap with an
// ancestor frame's bounds are clamped to all ancestor frames' bounds in root
// coordinates (crbug.com/574502257).
IN_PROC_BROWSER_TEST_F(ContentAutofillDriverNestedFramesBrowserTest,
                       LiftClampsOverlappingBounds) {
  // In root coordinates before clamping, gfx::RectF(450, 450, 100, 100) is
  // (x=50, y=50, width=100, height=100).
  FormData form;
  FormFieldData field;
  field.set_bounds(gfx::RectF(450, 450, 100, 100));
  form.set_fields({field});

  test_api(inner_driver()).LiftForTest(form);

  EXPECT_THAT(form.fields(),
              ElementsAre(Property(&FormFieldData::bounds,
                                   gfx::RectF(100, 100, 50, 50))));
}

// Tests that field bounds in a nested cross-origin iframe that are disjoint
// from an ancestor frame's bounds collapse to an empty rectangle at the closest
// point of the ancestor frame's bounds rather than (0, 0, 0, 0).
IN_PROC_BROWSER_TEST_F(ContentAutofillDriverNestedFramesBrowserTest,
                       LiftClampsDisjointBounds) {
  // In root coordinates before clamping, gfx::RectF(450, 800, 100, 20) is
  // (x=50, y=400, width=100, height=20), which is disjoint from `outer_frame`.
  FormData form;
  FormFieldData field;
  field.set_bounds(gfx::RectF(450, 800, 100, 20));
  form.set_fields({field});

  test_api(inner_driver()).LiftForTest(form);

  EXPECT_THAT(form.fields(), ElementsAre(Property(&FormFieldData::bounds,
                                                  gfx::RectF(100, 300, 0, 0))));
}

}  // namespace
}  // namespace autofill
