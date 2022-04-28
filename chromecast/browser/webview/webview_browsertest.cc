// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/extensions/cast_extension_system_factory.h"
#include "chromecast/browser/webview/webview_browser_context.h"
#include "chromecast/browser/webview/webview_controller.h"
#include "chromecast/graphics/cast_window_manager_aura.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window_tree_host.h"

using testing::_;
using testing::InSequence;
using testing::Truly;

namespace chromecast {
namespace {

constexpr base::TimeDelta kDefaultTimeout = base::Milliseconds(5000);

const std::string kKeyInputDataURL = R"HTML(
<!DOCTYPE html>
<html>
<input type="text" id="input" autofocus></input>
<script type="text/javascript">
var keyDownCode = 0;
var keyPressCode = 0;
var keyUpCode = 0;
var events = "";
var inputRect;
var touchX = 0;
var touchY = 0;

document.addEventListener("keydown", function(event) {
  keyDownCode = event.keyCode;
  events += "keydown ";
  if (keyDownCode == 9) {
    event.preventDefault();
  }
});
document.addEventListener("keypress", function(event) {
  keyPressCode = event.keyCode;
  events += "keypress ";
});
document.addEventListener("keyup", function(event) {
  keyUpCode = event.keyCode;
  events += "keyup";
});
inputRect = document.getElementById("input").getBoundingClientRect();
touchX = (inputRect.left + inputRect.right) / 2;
touchY = (inputRect.top + inputRect.bottom) / 2;
document.title = "ready";
</script>
</html>
)HTML";

}  // namespace

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
    net::test_server::RegisterDefaultHandlers(embedded_test_server());
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
    command_line->AppendSwitch("allow-pre-commit-input");
  }
  void RunTestOnMainThread() override {}
  void PostRunTestOnMainThread() override {}

  void RunMessageLoop() {
    base::test::ScopedRunLoopTimeout timeout(
        FROM_HERE, kDefaultTimeout,
        base::BindRepeating(&WebviewTest::OnTimeout, base::Unretained(this)));
    run_loop_->Run();
  }

  // Handle an inbound "request" (simulated gRPC call from client)
  // asynchronously.
  void SubmitWebviewRequest(WebviewController* controller,
                            const webview::WebviewRequest& request) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&WebviewController::ProcessRequest,
                                  base::Unretained(controller), request));
  }

  // Asynchronously submit an attempt to navigate from one page to another.
  void SubmitNavigation(content::WebContents* web_contents,
                        const std::string& path) {
    GURL url = embedded_test_server()->GetURL("foo.com", path);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(
                       [](content::WebContents* web_contents, const GURL& url) {
                         std::ignore =
                             content::NavigateToURL(web_contents, url);
                       },
                       web_contents, url));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    GURL absolute_url = embedded_test_server()->GetURL(request.relative_url);
    if (absolute_url.path() != "/test" && absolute_url.path() != "/test2" &&
        absolute_url.path() != "/key_input")
      return nullptr;

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    if (absolute_url.path() == "/key_input") {
      http_response->set_content(kKeyInputDataURL);
      http_response->set_content_type("text/html");
    } else {
      http_response->set_content("hello");
      http_response->set_content_type("text/plain");
    }
    return http_response;
  }

  std::string OnTimeout() {
    Quit();
    return "Timeout in webview browsertest";
  }

  std::function<bool(const std::unique_ptr<webview::WebviewResponse>& response)>
  CheckForPageLoadedEvent(const std::string& path) {
    return [path](const std::unique_ptr<webview::WebviewResponse>& response) {
      if (!response->has_page_event() ||
          response->page_event().current_page_state() !=
              webview::AsyncPageEvent_State_LOADED)
        return false;
      GURL url(response->page_event().url());
      return url.path() == path;
    };
  }

  void Quit() { run_loop_->QuitWhenIdle(); }

  webview::WebviewRequest GenerateKeyInputRequest(
      int event_type,
      const std::string& key_string) {
    webview::WebviewRequest request;
    request.mutable_input()->set_event_type(event_type);
    request.mutable_input()->set_timestamp(
        base::TimeTicks::Now().since_origin().InMicroseconds());
    request.mutable_input()->mutable_key()->set_key_string(key_string);
    return request;
  }

  webview::WebviewRequest GenerateTouchInputRequest(int event_type,
                                                    float touch_x,
                                                    float touch_y) {
    webview::WebviewRequest request;
    webview::InputEvent* input_event = request.mutable_input();
    input_event->set_event_type(event_type);
    input_event->set_timestamp(
        base::TimeTicks::Now().since_origin().InMicroseconds());
    webview::TouchInput* touch_input = input_event->mutable_touch();
    touch_input->set_x(touch_x);
    touch_input->set_y(touch_y);
    touch_input->set_root_x(touch_x);
    touch_input->set_root_y(touch_y);
    touch_input->set_pointer_type(
        static_cast<int>(ui::EventPointerType::kTouch));
    touch_input->set_pointer_id(1);
    return request;
  }

  int ExecuteScriptAndExtractInt(content::WebContents* contents,
                                 const std::string& script) {
    int value = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        contents, "domAutomationController.send(" + script + ")", &value));
    return value;
  }

  std::string ExecuteScriptAndExtractString(content::WebContents* contents,
                                            const std::string& script) {
    std::string value = "";
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        contents, "domAutomationController.send(" + script + ")", &value));
    return value;
  }

  double ExecuteScriptAndExtractDouble(content::WebContents* contents,
                                       const std::string& script) {
    double value = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractDouble(
        contents, "domAutomationController.send(" + script + ")", &value));
    return value;
  }

  bool URLLoaded(content::WebContents* contents) {
    std::u16string ready_title(u"ready");
    content::TitleWatcher watcher(contents, ready_title);
    const std::u16string title = watcher.WaitAndGetTitle();
    return title == ready_title;
  }

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
  SubmitWebviewRequest(&webview, nav);

  RunMessageLoop();
}

// Verify the navigation request process
// Disabled due to flakiness. http://crbug.com/1192724
IN_PROC_BROWSER_TEST_F(WebviewTest, DISABLED_VerifyNavigationDelegation) {
  WebviewController webview(context_.get(), &client_, true);

  EXPECT_CALL(client_, EnqueueSend(_)).Times(testing::AnyNumber());

  EXPECT_CALL(client_, EnqueueSend(Truly(
                           // Look for our initial page load event
                           CheckForPageLoadedEvent("/test"))))
      .Times(testing::AtLeast(1))
      .WillOnce(
          [this, &webview](std::unique_ptr<webview::WebviewResponse> response) {
            // Turn on navigation delegation in the webview.
            webview::WebviewRequest test_settings_request;
            test_settings_request.mutable_update_settings()
                ->set_has_navigation_delegate(true);
            SubmitWebviewRequest(&webview, test_settings_request);

            // And now ask to navigate to the second page.
            SubmitNavigation(webview.GetWebContents(), "/test2");
          });

  EXPECT_CALL(
      client_,
      EnqueueSend(
          // Look for navigation events.
          Truly([](const std::unique_ptr<webview::WebviewResponse>& response) {
            return response->has_navigation_event();
          })))
      .Times(testing::AtLeast(1))
      .WillOnce(
          // Respond with our navigation decision (NAVIGATE)
          [this, &webview](std::unique_ptr<webview::WebviewResponse> response) {
            webview::WebviewRequest nav_decision_request;
            nav_decision_request.set_navigation_decision(
                webview::NavigationDecision::NAVIGATE);

            // Submit the navigation decision back to the controller.
            SubmitWebviewRequest(&webview, nav_decision_request);
          });

  // Checks for secondary page (/test2) loaded event.
  EXPECT_CALL(client_, EnqueueSend(Truly(CheckForPageLoadedEvent("/test2"))))
      .Times(testing::AtLeast(1))
      .WillOnce([this](std::unique_ptr<webview::WebviewResponse> response) {
        Quit();
      });

  GURL test_url = embedded_test_server()->GetURL("foo.com", "/test");

  webview::WebviewRequest nav;
  nav.mutable_navigate()->set_url(test_url.spec());
  SubmitWebviewRequest(&webview, nav);
  RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(WebviewTest, Incognito) {
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
  std::unique_ptr<content::BrowserContext> owned_context =
      std::make_unique<WebviewBrowserContext>(context_.get());
  WebviewController webview(std::move(owned_context), &client_, true);

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
            // Note this one needs to be processed synchronously (not through
            // SubmitWebviewRequest) because we check the results immediately
            // after.
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
  SubmitWebviewRequest(&webview, resize);

  webview::WebviewRequest nav;
  nav.mutable_navigate()->set_url(test_url.spec());
  SubmitWebviewRequest(&webview, nav);
  RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(WebviewTest, UserDataOverrideOnFirstRequest) {
  // Webview creation sends messages to the client (eg: accessibility ID).
  EXPECT_CALL(client_, EnqueueSend(_)).Times(testing::AnyNumber());

  WebviewController webview(context_.get(), &client_, true);
  const std::string kHeaderPath =
      std::string("/echoheader?") + net::HttpRequestHeaders::kUserAgent;
  GURL test_url = embedded_test_server()->GetURL(kHeaderPath);

  const std::string kUserAgentOverride = "bar";

  auto check = [](const std::unique_ptr<webview::WebviewResponse>& response) {
    return response->has_page_event() &&
           response->page_event().current_page_state() ==
               webview::AsyncPageEvent_State_LOADED;
  };
  EXPECT_CALL(client_, EnqueueSend(Truly(check)))
      .Times(2)
      .WillOnce([&](std::unique_ptr<webview::WebviewResponse> response) {
        std::string header_value;
        EXPECT_TRUE(content::ExecuteScriptAndExtractString(
            webview.GetWebContents(),
            "window.domAutomationController.send(document.body.textContent);",
            &header_value));
        EXPECT_EQ(kUserAgentOverride, header_value);

        // Send an update without the override so we can check that the caller
        // can disable the override.
        webview::WebviewRequest update_settings;
        update_settings.mutable_update_settings()->set_javascript_enabled(true);
        SubmitWebviewRequest(&webview, update_settings);

        webview::WebviewRequest reload;
        reload.mutable_reload();
        SubmitWebviewRequest(&webview, reload);
      })
      .WillOnce([&](std::unique_ptr<webview::WebviewResponse> response) {
        std::string header_value;
        EXPECT_TRUE(content::ExecuteScriptAndExtractString(
            webview.GetWebContents(),
            "window.domAutomationController.send(document.body.textContent);",
            &header_value));
        EXPECT_NE(kUserAgentOverride, header_value);
        Quit();
      });

  // Need to enable JS in order to extract the UserAgent string from the loaded
  // web page.
  webview::WebviewRequest update_settings;
  update_settings.mutable_update_settings()->set_javascript_enabled(true);
  update_settings.mutable_update_settings()->mutable_user_agent()->set_value(
      kUserAgentOverride);
  SubmitWebviewRequest(&webview, update_settings);

  webview::WebviewRequest navigate;
  navigate.mutable_navigate()->set_url(test_url.spec());
  SubmitWebviewRequest(&webview, navigate);

  RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(WebviewTest, UserDataOverride) {
  // Webview creation sends messages to the client (eg: accessibility ID).
  EXPECT_CALL(client_, EnqueueSend(_)).Times(testing::AnyNumber());

  WebviewController webview(context_.get(), &client_, true);
  const std::string kHeaderPath =
      std::string("/echoheader?") + net::HttpRequestHeaders::kUserAgent;
  GURL test_url = embedded_test_server()->GetURL(kHeaderPath);

  const std::string kUserAgentOverride = "bar";

  auto check = [](const std::unique_ptr<webview::WebviewResponse>& response) {
    return response->has_page_event() &&
           response->page_event().current_page_state() ==
               webview::AsyncPageEvent_State_LOADED;
  };
  EXPECT_CALL(client_, EnqueueSend(Truly(check)))
      .Times(2)
      .WillOnce([&](std::unique_ptr<webview::WebviewResponse> response) {
        std::string header_value;
        EXPECT_TRUE(content::ExecuteScriptAndExtractString(
            webview.GetWebContents(),
            "window.domAutomationController.send(document.body.textContent);",
            &header_value));
        EXPECT_NE(kUserAgentOverride, header_value);

        webview::WebviewRequest update_settings;
        update_settings.mutable_update_settings()->set_javascript_enabled(true);
        update_settings.mutable_update_settings()
            ->mutable_user_agent()
            ->set_value(kUserAgentOverride);
        SubmitWebviewRequest(&webview, update_settings);

        webview::WebviewRequest reload;
        reload.mutable_reload();
        SubmitWebviewRequest(&webview, reload);
      })
      .WillOnce([&](std::unique_ptr<webview::WebviewResponse> response) {
        std::string header_value;
        EXPECT_TRUE(content::ExecuteScriptAndExtractString(
            webview.GetWebContents(),
            "window.domAutomationController.send(document.body.textContent);",
            &header_value));
        EXPECT_EQ(kUserAgentOverride, header_value);
        Quit();
      });

  // Need to enable JS in order to extract the UserAgent string from the loaded
  // web page.
  webview::WebviewRequest update_settings;
  update_settings.mutable_update_settings()->set_javascript_enabled(true);
  SubmitWebviewRequest(&webview, update_settings);

  webview::WebviewRequest navigate;
  navigate.mutable_navigate()->set_url(test_url.spec());
  SubmitWebviewRequest(&webview, navigate);

  RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(WebviewTest, Focus) {
  // Webview creation sends messages to the client (eg: accessibility ID).
  EXPECT_CALL(client_, EnqueueSend(_)).Times(testing::AnyNumber());

  WebviewController webview(context_.get(), &client_, true);
  GURL test_url = embedded_test_server()->GetURL("foo.com", "/test");
  webview.GetWebContents()->GetNativeView()->Show();
  CastWindowManagerAura window_manager(false);
  window_manager.Setup();
  window_manager.AddWindow(webview.GetWebContents()->GetNativeView());

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
            request.mutable_focus();
            webview.ProcessRequest(request);
            EXPECT_TRUE(webview.GetWebContents()->GetNativeView()->HasFocus());

            Quit();
          });

  webview::WebviewRequest navigate;
  navigate.mutable_navigate()->set_url(test_url.spec());
  SubmitWebviewRequest(&webview, navigate);

  RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(WebviewTest, GetUserAgent) {
  auto check = [](const std::unique_ptr<webview::WebviewResponse>& response) {
    return response->has_get_user_agent();
  };
  EXPECT_CALL(client_, EnqueueSend(_)).Times(testing::AnyNumber());
  EXPECT_CALL(client_, EnqueueSend(Truly(check)))
      .Times(testing::AtLeast(1))
      .WillOnce([this](std::unique_ptr<webview::WebviewResponse> response) {
        EXPECT_NE(response->get_user_agent().user_agent(), "");
        Quit();
      });
  WebviewController webview(context_.get(), &client_, true);

  webview::WebviewRequest request;
  request.mutable_get_user_agent();
  SubmitWebviewRequest(&webview, request);

  RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(WebviewTest, KeyInput) {
  // Webview creation sends messages to the client (eg: accessibility ID).
  EXPECT_CALL(client_, EnqueueSend(_)).Times(testing::AnyNumber());

  CastWindowManagerAura window_manager(false);
  window_manager.Setup();

  WebviewController webview(context_.get(), &client_, true);
  window_manager.AddWindow(webview.GetWebContents()->GetNativeView());
  webview.GetWebContents()->GetNativeView()->Show();
  webview.GetWebContents()->GetNativeView()->Focus();

  GURL test_url = embedded_test_server()->GetURL("foo.com", "/key_input");

  auto check = [](const std::unique_ptr<webview::WebviewResponse>& response) {
    return response->has_page_event() &&
           response->page_event().current_page_state() ==
               webview::AsyncPageEvent_State_LOADED;
  };
  EXPECT_CALL(client_, EnqueueSend(Truly(check)))
      .Times(testing::AtLeast(1))
      .WillOnce([this,
                 &webview](std::unique_ptr<webview::WebviewResponse> response) {
        DCHECK(URLLoaded(webview.GetWebContents()));

        content::RenderFrameSubmissionObserver frame_observer(
            webview.GetWebContents());

        webview::WebviewRequest input =
            GenerateKeyInputRequest(ui::ET_KEY_PRESSED, "a");
        webview.ProcessRequest(input);
        input = GenerateKeyInputRequest(ui::ET_KEY_RELEASED, "a");
        webview.ProcessRequest(input);

        while (ExecuteScriptAndExtractInt(webview.GetWebContents(),
                                          "keyDownCode") == 0 ||
               ExecuteScriptAndExtractInt(webview.GetWebContents(),
                                          "keyPressCode") == 0 ||
               ExecuteScriptAndExtractInt(webview.GetWebContents(),
                                          "keyUpCode") == 0)
          frame_observer.WaitForAnyFrameSubmission();

        EXPECT_EQ(
            ExecuteScriptAndExtractString(webview.GetWebContents(), "events"),
            "keydown keypress keyup");
        EXPECT_EQ(
            ExecuteScriptAndExtractInt(webview.GetWebContents(), "keyDownCode"),
            65);
        EXPECT_EQ(ExecuteScriptAndExtractInt(webview.GetWebContents(),
                                             "keyPressCode"),
                  97);
        EXPECT_EQ(
            ExecuteScriptAndExtractInt(webview.GetWebContents(), "keyUpCode"),
            65);

        input = GenerateKeyInputRequest(ui::ET_KEY_PRESSED, u8"\u00b6");
        webview.ProcessRequest(input);
        input = GenerateKeyInputRequest(ui::ET_KEY_RELEASED, u8"\u00b6");
        webview.ProcessRequest(input);

        while (ExecuteScriptAndExtractInt(webview.GetWebContents(),
                                          "keyPressCode") == 97)
          frame_observer.WaitForAnyFrameSubmission();

        // Non-US layout keys should only generate a keypress event.
        EXPECT_EQ(
            ExecuteScriptAndExtractInt(webview.GetWebContents(), "keyDownCode"),
            65);
        EXPECT_EQ(ExecuteScriptAndExtractInt(webview.GetWebContents(),
                                             "keyPressCode"),
                  182);
        EXPECT_EQ(
            ExecuteScriptAndExtractInt(webview.GetWebContents(), "keyUpCode"),
            65);

        input = GenerateKeyInputRequest(ui::ET_KEY_PRESSED, "Backspace");
        webview.ProcessRequest(input);
        input = GenerateKeyInputRequest(ui::ET_KEY_RELEASED, "Backspace");
        webview.ProcessRequest(input);

        while (ExecuteScriptAndExtractInt(webview.GetWebContents(),
                                          "keyDownCode") == 0 ||
               ExecuteScriptAndExtractInt(webview.GetWebContents(),
                                          "keyUpCode") == 0)
          frame_observer.WaitForAnyFrameSubmission();

        EXPECT_EQ(
            ExecuteScriptAndExtractInt(webview.GetWebContents(), "keyDownCode"),
            8);
        // Backspace does not generate a keypress event.
        EXPECT_EQ(ExecuteScriptAndExtractInt(webview.GetWebContents(),
                                             "keyPressCode"),
                  182);
        EXPECT_EQ(
            ExecuteScriptAndExtractInt(webview.GetWebContents(), "keyUpCode"),
            8);

        input = GenerateKeyInputRequest(ui::ET_KEY_PRESSED, "Tab");
        webview.ProcessRequest(input);
        input = GenerateKeyInputRequest(ui::ET_KEY_RELEASED, "Tab");
        webview.ProcessRequest(input);

        while (ExecuteScriptAndExtractInt(webview.GetWebContents(),
                                          "keyDownCode") == 8 ||
               ExecuteScriptAndExtractInt(webview.GetWebContents(),
                                          "keyUpCode") == 8)
          frame_observer.WaitForAnyFrameSubmission();

        EXPECT_EQ(
            ExecuteScriptAndExtractInt(webview.GetWebContents(), "keyDownCode"),
            9);
        // Tab does not generate a keypress event.
        EXPECT_EQ(ExecuteScriptAndExtractInt(webview.GetWebContents(),
                                             "keyPressCode"),
                  182);
        EXPECT_EQ(
            ExecuteScriptAndExtractInt(webview.GetWebContents(), "keyUpCode"),
            9);
        Quit();
      })
      .WillRepeatedly(
          [](std::unique_ptr<webview::WebviewResponse> response) {});

  // Need to enable JS in order to extract the key string from the loaded
  // web page.
  webview::WebviewRequest update_settings;
  update_settings.mutable_update_settings()->set_javascript_enabled(true);
  SubmitWebviewRequest(&webview, update_settings);

  // Requests are executed serially. Resize first to make sure the Webview is
  // properly sized by the time the page loads.
  webview::WebviewRequest resize;
  resize.mutable_resize()->set_width(800);
  resize.mutable_resize()->set_height(600);
  SubmitWebviewRequest(&webview, resize);

  webview::WebviewRequest navigate;
  navigate.mutable_navigate()->set_url(test_url.spec());
  SubmitWebviewRequest(&webview, navigate);
  RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(WebviewTest, SendFocusEventWhenVKShouldBeShown) {
  // Webview creation sends messages to the client (eg: accessibility ID).
  EXPECT_CALL(client_, EnqueueSend(_)).Times(testing::AnyNumber());

  CastWindowManagerAura window_manager(false);
  window_manager.Setup();
  WebviewController webview(context_.get(), &client_, true);
  window_manager.AddWindow(webview.GetWebContents()->GetNativeView());
  webview.GetWebContents()->GetNativeView()->Show();
  webview.GetWebContents()->GetNativeView()->Focus();
  webview.OnVisible(webview.GetWebContents()->GetNativeView());

  GURL test_url = embedded_test_server()->GetURL("foo.com", "/key_input");

  InSequence seq;

  auto check = [](const std::unique_ptr<webview::WebviewResponse>& response) {
    return response->has_page_event() &&
           response->page_event().current_page_state() ==
               webview::AsyncPageEvent_State_LOADED;
  };
  EXPECT_CALL(client_, EnqueueSend(Truly(check)))
      .Times(testing::AtLeast(1))
      .WillOnce([&](std::unique_ptr<webview::WebviewResponse> response) {
        DCHECK(URLLoaded(webview.GetWebContents()));
        float touch_x =
            ExecuteScriptAndExtractDouble(webview.GetWebContents(), "touchX");
        float touch_y =
            ExecuteScriptAndExtractDouble(webview.GetWebContents(), "touchY");
        SubmitWebviewRequest(
            &webview,
            GenerateTouchInputRequest(ui::ET_TOUCH_PRESSED, touch_x, touch_y));
        SubmitWebviewRequest(
            &webview,
            GenerateTouchInputRequest(ui::ET_TOUCH_RELEASED, touch_x, touch_y));
      })
      .WillRepeatedly(
          [](std::unique_ptr<webview::WebviewResponse> response) {});

  auto input_focus_text_check =
      [](const std::unique_ptr<webview::WebviewResponse>& response) {
        return response->has_input_focus_event() &&
               response->input_focus_event().type() ==
                   webview::TEXT_INPUT_TYPE_TEXT;
      };
  EXPECT_CALL(client_, EnqueueSend(Truly(input_focus_text_check)))
      .WillOnce([](std::unique_ptr<webview::WebviewResponse> response) {})
      .WillOnce([&](std::unique_ptr<webview::WebviewResponse> response) {
        // Tap outside the input field to verify focus loss.
        SubmitWebviewRequest(&webview, GenerateTouchInputRequest(
                                           ui::ET_TOUCH_PRESSED, 300, 300));
        SubmitWebviewRequest(&webview, GenerateTouchInputRequest(
                                           ui::ET_TOUCH_RELEASED, 300, 300));
      })
      .WillRepeatedly(
          [](std::unique_ptr<webview::WebviewResponse> response) {});

  auto input_focus_none_check =
      [](const std::unique_ptr<webview::WebviewResponse>& response) {
        return response->has_input_focus_event() &&
               response->input_focus_event().type() ==
                   webview::TEXT_INPUT_TYPE_NONE;
      };
  EXPECT_CALL(client_, EnqueueSend(Truly(input_focus_none_check)))
      .WillOnce(
          [&](std::unique_ptr<webview::WebviewResponse> response) { Quit(); });

  // Need to enable JS in order to extract the text input field touch
  // coordinates from the loaded web page.
  webview::WebviewRequest update_settings;
  update_settings.mutable_update_settings()->set_javascript_enabled(true);
  SubmitWebviewRequest(&webview, update_settings);

  // Requests are executed serially. Resize first to make sure the Webview is
  // properly sized by the time the page loads.
  webview::WebviewRequest resize;
  resize.mutable_resize()->set_width(500);
  resize.mutable_resize()->set_height(500);
  SubmitWebviewRequest(&webview, resize);

  webview::WebviewRequest navigate;
  navigate.mutable_navigate()->set_url(test_url.spec());
  SubmitWebviewRequest(&webview, navigate);
  RunMessageLoop();
}

}  // namespace chromecast
