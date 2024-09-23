// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/simple_protocol_handler_registry_factory.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::WebContents;

namespace {

using custom_handlers::ProtocolHandlerRegistry;

class ProtocolHandlerChangeWaiter : public ProtocolHandlerRegistry::Observer {
 public:
  explicit ProtocolHandlerChangeWaiter(ProtocolHandlerRegistry* registry) {
    registry_observation_.Observe(registry);
  }
  ProtocolHandlerChangeWaiter(const ProtocolHandlerChangeWaiter&) = delete;
  ProtocolHandlerChangeWaiter& operator=(const ProtocolHandlerChangeWaiter&) =
      delete;
  ~ProtocolHandlerChangeWaiter() override = default;

  void Wait() { run_loop_.Run(); }
  // ProtocolHandlerRegistry::Observer:
  void OnProtocolHandlerRegistryChanged() override { run_loop_.Quit(); }

 private:
  base::ScopedObservation<custom_handlers::ProtocolHandlerRegistry,
                          custom_handlers::ProtocolHandlerRegistry::Observer>
      registry_observation_{this};
  base::RunLoop run_loop_;
};

}  // namespace

namespace custom_handlers {

class RegisterProtocolHandlerBrowserTest : public content::ContentBrowserTest {
 public:
  RegisterProtocolHandlerBrowserTest() = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/");
  }

  void AddProtocolHandler(const std::string& protocol, const GURL& url) {
    ProtocolHandler handler =
        ProtocolHandler::CreateProtocolHandler(protocol, url);
    ProtocolHandlerRegistry* registry =
        SimpleProtocolHandlerRegistryFactory::GetForBrowserContext(
            browser_context(), true);
    // Fake that this registration is happening on profile startup. Otherwise
    // it'll try to register with the OS, which causes DCHECKs on Windows when
    // running as admin on Windows 7.
    registry->SetIsLoading(true);
    registry->OnAcceptRegisterProtocolHandler(handler);
    registry->SetIsLoading(true);
    ASSERT_TRUE(registry->IsHandledProtocol(protocol));
  }

  void RemoveProtocolHandler(const std::string& protocol, const GURL& url) {
    ProtocolHandler handler =
        ProtocolHandler::CreateProtocolHandler(protocol, url);
    ProtocolHandlerRegistry* registry =
        SimpleProtocolHandlerRegistryFactory::GetForBrowserContext(
            browser_context(), true);
    registry->RemoveHandler(handler);
    ASSERT_FALSE(registry->IsHandledProtocol(protocol));
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }
  content::BrowserContext* browser_context() {
    return web_contents()->GetBrowserContext();
  }

 protected:
  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(RegisterProtocolHandlerBrowserTest, CustomHandler) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL handler_url =
      embedded_test_server()->GetURL("/custom_handlers/custom_handler.html");
  AddProtocolHandler("news", handler_url);

  ASSERT_TRUE(NavigateToURL(shell(), GURL("news:test"), handler_url));

  ASSERT_EQ(handler_url, web_contents()->GetLastCommittedURL());

  // Also check redirects.
  GURL redirect_url =
      embedded_test_server()->GetURL("/server-redirect?news:test");
  ASSERT_TRUE(NavigateToURL(shell(), redirect_url, handler_url));

  ASSERT_EQ(handler_url, web_contents()->GetLastCommittedURL());
}

// https://crbug.com/178097: Implement registerProtocolHandler on Android
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(RegisterProtocolHandlerBrowserTest,
                       IgnoreRequestWithoutUserGesture) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/custom_handlers/title1.html")));

  // Ensure the registry is currently empty.
  GURL url("web+search:testing");
  ProtocolHandlerRegistry* registry =
      SimpleProtocolHandlerRegistryFactory::GetForBrowserContext(
          browser_context(), true);
  ASSERT_EQ(0u, registry->GetHandlersFor(url.scheme()).size());

  // Attempt to add an entry.
  ProtocolHandlerChangeWaiter waiter(registry);
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "navigator.registerProtocolHandler('web+"
                              "search', 'test.html?%s', 'test');",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  waiter.Wait();

  // Verify the registration is ignored if no user gesture involved.
  ASSERT_EQ(1u, registry->GetHandlersFor(url.scheme()).size());
  ASSERT_FALSE(registry->IsHandledProtocol(url.scheme()));
}

// FencedFrames can not register to handle any protocols.
IN_PROC_BROWSER_TEST_F(RegisterProtocolHandlerBrowserTest, FencedFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/custom_handlers/title1.html")));

  // Create a FencedFrame.
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(),
          embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  ASSERT_TRUE(fenced_frame_host);

  // Ensure the registry is currently empty.
  GURL url("web+search:testing");
  ProtocolHandlerRegistry* registry =
      SimpleProtocolHandlerRegistryFactory::GetForBrowserContext(
          browser_context(), true);
  ASSERT_EQ(0u, registry->GetHandlersFor(url.scheme()).size());

  // Attempt to add an entry.
  ProtocolHandlerChangeWaiter waiter(registry);
  ASSERT_TRUE(content::ExecJs(fenced_frame_host,
                              "navigator.registerProtocolHandler('web+"
                              "search', 'test.html?%s', 'test');"));
  waiter.Wait();

  // Ensure the registry is still empty.
  ASSERT_EQ(0u, registry->GetHandlersFor(url.scheme()).size());
}
#endif

// https://crbug.com/178097: Implement registerProtocolHandler on Android
#if !BUILDFLAG(IS_ANDROID)
class RegisterProtocolHandlerAndServiceWorkerInterceptor
    : public RegisterProtocolHandlerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    RegisterProtocolHandlerBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    // Navigate to the test page.
    ASSERT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(
                     "/protocol_handler/service_workers/"
                     "test_protocol_handler_and_service_workers.html")));
  }
};

// TODO(crbug.com/40763886): Fix flakiness.
IN_PROC_BROWSER_TEST_F(RegisterProtocolHandlerAndServiceWorkerInterceptor,
                       DISABLED_RegisterFetchListenerForHTMLHandler) {
  // Register a service worker intercepting requests to the HTML handler.
  EXPECT_EQ(true,
            content::EvalJs(shell(), "registerFetchListenerForHTMLHandler();"));

  {
    // Register a HTML handler with a user gesture.
    ProtocolHandlerRegistry* registry =
        SimpleProtocolHandlerRegistryFactory::GetForBrowserContext(
            browser_context(), true);
    ProtocolHandlerChangeWaiter waiter(registry);
    ASSERT_TRUE(content::ExecJs(shell(), "registerHTMLHandler();"));
    waiter.Wait();
  }

  // Verify that a page with the registered scheme is managed by the service
  // worker, not the HTML handler.
  EXPECT_EQ(true,
            content::EvalJs(shell(),
                            "pageWithCustomSchemeHandledByServiceWorker();"));
}
#endif

}  // namespace custom_handlers
