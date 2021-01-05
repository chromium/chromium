// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/extensions/cast_extension_system_factory.h"
#include "chromecast/browser/webview/webview_controller.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Truly;

namespace chromecast {
namespace {
constexpr base::TimeDelta kDefaultTimeout =
    base::TimeDelta::FromMilliseconds(5000);
}

class MockClient : public WebviewController::Client {
 public:
  MOCK_METHOD(void,
              EnqueueSend,
              (std::unique_ptr<webview::WebviewResponse> response));
  MOCK_METHOD(void, OnError, (const std::string& error_message));
};

class WebviewTest : public content::BrowserTestBase {
 protected:
  WebviewTest() = default;
  ~WebviewTest() override = default;

  void PreRunTestOnMainThread() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    base::RunLoop().RunUntilIdle();

    context_ = std::make_unique<content::TestBrowserContext>();
    run_loop_.reset(new base::RunLoop());
  }
  void SetUp() final {
    SetUpCommandLine(base::CommandLine::ForCurrentProcess());
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &WebviewTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    BrowserTestBase::SetUp();
  }
  void SetUpOnMainThread() final {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }
  void TearDownOnMainThread() final {
    BrowserContextDependencyManager::GetInstance()
        ->DestroyBrowserContextServices(context_.get());
    context_.reset();
  }
  void SetUpCommandLine(base::CommandLine* command_line) final {
    command_line->AppendSwitchASCII(switches::kTestType, "browser");
  }
  void RunTestOnMainThread() override {}
  void PostRunTestOnMainThread() override {}

  void RunMessageLoop() {
    base::test::ScopedRunLoopTimeout timeout(
        FROM_HERE, kDefaultTimeout,
        base::BindRepeating(&WebviewTest::OnTimeout, base::Unretained(this)));
    run_loop_->Run();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    GURL absolute_url = embedded_test_server()->GetURL(request.relative_url);
    if (absolute_url.path() != "/test")
      return std::unique_ptr<net::test_server::HttpResponse>();

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("hello");
    http_response->set_content_type("text/plain");
    return http_response;
  }

  std::string OnTimeout() {
    Quit();
    return "Timeout in webview browsertest";
  }

  void Quit() { run_loop_->QuitWhenIdle(); }

  std::unique_ptr<content::TestBrowserContext> context_;
  std::unique_ptr<base::RunLoop> run_loop_;
  MockClient client_;
};

IN_PROC_BROWSER_TEST_F(WebviewTest, Navigate) {
  auto check = [](const std::unique_ptr<webview::WebviewResponse>& response) {
    return response->has_page_event() &&
           response->page_event().current_page_state() ==
               webview::AsyncPageEvent_State_LOADED;
  };
  EXPECT_CALL(client_, EnqueueSend(_)).Times(testing::AnyNumber());
  EXPECT_CALL(client_, EnqueueSend(Truly(check)))
      .Times(testing::AtLeast(1))
      .WillOnce([this](std::unique_ptr<webview::WebviewResponse> response) {
        Quit();
      });
  WebviewController webview(context_.get(), &client_, true);

  GURL test_url = embedded_test_server()->GetURL("foo.com", "/test");

  webview::WebviewRequest nav;
  nav.mutable_navigate()->set_url(test_url.spec());
  webview.ProcessRequest(nav);

  RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(WebviewTest, SetInsets) {
  // Webview creation sends messages to the client (eg: accessibility ID).
  EXPECT_CALL(client_, EnqueueSend(_)).Times(testing::AnyNumber());

  WebviewController webview(context_.get(), &client_, true);
  GURL test_url = embedded_test_server()->GetURL("foo.com", "/test");

  auto check = [](const std::unique_ptr<webview::WebviewResponse>& response) {
    return response->has_page_event() &&
           response->page_event().current_page_state() ==
               webview::AsyncPageEvent_State_LOADED;
  };
  EXPECT_CALL(client_, EnqueueSend(Truly(check)))
      .Times(testing::AtLeast(1))
      .WillOnce(
          [this, &webview](std::unique_ptr<webview::WebviewResponse> response) {
            webview::WebviewRequest request;
            request.mutable_set_insets()->set_top(0);
            request.mutable_set_insets()->set_left(0);
            request.mutable_set_insets()->set_bottom(200);
            request.mutable_set_insets()->set_right(0);
            webview.ProcessRequest(request);

            gfx::Size size_after = webview.GetWebContents()
                                       ->GetRenderWidgetHostView()
                                       ->GetVisibleViewportSize();
            EXPECT_EQ(gfx::Size(800, 400), size_after);

            Quit();
          });

  // Requests are executed serially. Resize first to make sure the Webview is
  // properly sized by the time the page loads.
  webview::WebviewRequest resize;
  resize.mutable_resize()->set_width(800);
  resize.mutable_resize()->set_height(600);
  webview.ProcessRequest(resize);

  webview::WebviewRequest nav;
  nav.mutable_navigate()->set_url(test_url.spec());
  webview.ProcessRequest(nav);

  RunMessageLoop();
}

}  // namespace chromecast
