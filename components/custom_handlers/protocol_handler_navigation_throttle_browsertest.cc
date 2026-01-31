// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/custom_handlers/protocol_handler_navigation_throttle.h"

#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/simple_protocol_handler_registry_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::WebContents;

namespace custom_handlers {

constexpr char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

using HandlerPermissionGrantedCallback =
    ProtocolHandlerNavigationThrottle::HandlerPermissionGrantedCallback;
using HandlerPermissionDeniedCallback =
    ProtocolHandlerNavigationThrottle::HandlerPermissionDeniedCallback;

class ProtocolHandlerNavigationThrottleBrowserTest
    : public content::ContentBrowserTest {
 public:
  ProtocolHandlerNavigationThrottleBrowserTest() = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/");
    ASSERT_TRUE(embedded_test_server()->Start());

    content::ShellContentBrowserClient::Get()
        ->set_create_throttles_for_navigation_callback(base::BindRepeating(
            &ProtocolHandlerNavigationThrottle::MaybeCreateAndAdd, registry()));
  }

  void TearDownOnMainThread() override {
    ProtocolHandlerNavigationThrottle::GetDialogLaunchCallbackForTesting() =
        ProtocolHandlerNavigationThrottle::LaunchCallbackForTesting();
    content::ContentBrowserTest::TearDownOnMainThread();
  }

  ProtocolHandlerRegistry* registry() {
    return SimpleProtocolHandlerRegistryFactory::GetForBrowserContext(
        browser_context(), true);
  }
  content::WebContents* web_contents() { return shell()->web_contents(); }
  content::BrowserContext* browser_context() {
    return web_contents()->GetBrowserContext();
  }

  ProtocolHandler CreateUnconfirmedProtocolHandler(const std::string& protocol,
                                                   const GURL& url) {
    // Only handlers with extension id can be unconfirmed for now.
    return ProtocolHandler::CreateExtensionProtocolHandler(protocol, url,
                                                           kTestExtensionId);
  }

  void SetConfirmTestingCallback(bool permission_granted, bool remember) {
    ProtocolHandlerNavigationThrottle::GetDialogLaunchCallbackForTesting() =
        base::BindRepeating(
            [](bool accept, bool save,
               HandlerPermissionGrantedCallback granted_callback,
               HandlerPermissionDeniedCallback denied_callback) {
              if (accept) {
                std::move(granted_callback).Run(save);
              } else {
                std::move(denied_callback).Run();
              }
            },
            permission_granted, remember);
  }

  void RegisterProtocolHandler(const std::string& scheme,
                               const GURL& handler_url) {
    ProtocolHandler ph1 = CreateUnconfirmedProtocolHandler(scheme, handler_url);
    registry()->OnAcceptRegisterProtocolHandler(ph1);
    ASSERT_TRUE(registry()->IsHandledProtocol(scheme));
    ASSERT_FALSE(registry()->IsProtocolHandlerConfirmed(scheme));
  }
};

// An unconfirmed protocol handler registered should defer any navigation
// request to an URL with a handled scheme until the user grants permission to
// use the such protocol handler.
IN_PROC_BROWSER_TEST_F(ProtocolHandlerNavigationThrottleBrowserTest,
                       ConfirmProtocolHandler) {
  GURL url("news:test");
  GURL handler_url =
      embedded_test_server()->GetURL("/custom_handlers/custom_handler.html");
  RegisterProtocolHandler(url.GetScheme(), handler_url);

  // Grant permission to use the handler, so the navigation is completed.
  // The navigation will be deferred until the handler confirmation is
  // completed, hence the NavigateToURL returns 'false' in any case.
  SetConfirmTestingCallback(/*permission_granted=*/true, /*remember=*/false);
  ASSERT_FALSE(NavigateToURL(shell(), url));
  ASSERT_EQ(handler_url, web_contents()->GetLastCommittedURL());
}

// An unconfirmed protocol handler registered to handle a navigation request
// to an URL with an unknown scheme is not allowed if the user denies
// permission.
IN_PROC_BROWSER_TEST_F(ProtocolHandlerNavigationThrottleBrowserTest,
                       DenyProtocolHandler) {
  GURL url("news:test");
  GURL handler_url =
      embedded_test_server()->GetURL("/custom_handlers/custom_handler.html");
  RegisterProtocolHandler(url.GetScheme(), handler_url);

  // Initial navigation to check against, given that the new navigation won't be
  // completed.
  GURL initial_url =
      embedded_test_server()->GetURL("/custom_handlers/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Deny permission to use the handler, so the navigation is cancelled.
  SetConfirmTestingCallback(/*permission_granted=*/false, /*remember=*/false);
  ASSERT_FALSE(NavigateToURL(shell(), url));
  ASSERT_EQ(initial_url, web_contents()->GetLastCommittedURL());
}

// The decision of confirming a protocol persists in the BrowserContext.
IN_PROC_BROWSER_TEST_F(ProtocolHandlerNavigationThrottleBrowserTest,
                       ConfirmDecisisionPersist) {
  GURL url("news:test");
  GURL handler_url =
      embedded_test_server()->GetURL("/custom_handlers/custom_handler.html");
  RegisterProtocolHandler(url.GetScheme(), handler_url);

  // Grant permission to use the handler, so the navigation is completed.
  SetConfirmTestingCallback(/*permission_granted=*/true, /*remember=*/false);
  ASSERT_FALSE(NavigateToURL(shell(), url));
  ASSERT_EQ(handler_url, web_contents()->GetLastCommittedURL());

  // Initial navigation to check against, given that the new navigation won't be
  // completed.
  GURL initial_url =
      embedded_test_server()->GetURL("/custom_handlers/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Denying permission won't take any effect, because it's been granted
  // previously.
  SetConfirmTestingCallback(/*permission_granted=*/false, /*remember=*/false);
  ASSERT_FALSE(NavigateToURL(shell(), url));
  ASSERT_EQ(handler_url, web_contents()->GetLastCommittedURL());
}

// The decision of denying permission to use the handler is not persistent,
// so the next navigation request will ask again.
IN_PROC_BROWSER_TEST_F(ProtocolHandlerNavigationThrottleBrowserTest,
                       DenyDecisisionDoesNotPersist) {
  GURL url("news:test");
  GURL handler_url =
      embedded_test_server()->GetURL("/custom_handlers/custom_handler.html");
  RegisterProtocolHandler(url.GetScheme(), handler_url);

  // Initial navigation to check against, given that the new navigation won't be
  // completed.
  GURL initial_url =
      embedded_test_server()->GetURL("/custom_handlers/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Deny permission to use the handler, so the navigation is cancelled.
  SetConfirmTestingCallback(/*permission_granted=*/false, /*remember=*/false);
  ASSERT_FALSE(NavigateToURL(shell(), url));
  ASSERT_EQ(initial_url, web_contents()->GetLastCommittedURL());

  // Grant permission to use the handler, so the navigation is completed.
  SetConfirmTestingCallback(/*permission_granted=*/true, /*remember=*/false);
  ASSERT_FALSE(NavigateToURL(shell(), url));
  ASSERT_EQ(handler_url, web_contents()->GetLastCommittedURL());
}

}  // namespace custom_handlers
