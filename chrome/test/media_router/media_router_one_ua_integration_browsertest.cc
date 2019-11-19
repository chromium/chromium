// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include "base/cfi_buildflags.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/test/media_router/media_router_integration_browsertest.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

namespace media_router {

namespace {
const char kInitiateCloseFromReceiverPageScript[] =
    "initiateCloseFromReceiverPage();";
}

class MediaRouterIntegrationOneUABrowserTest
    : public MediaRouterIntegrationBrowserTest {
 public:
  void SetUpOnMainThread() override {
    MediaRouterIntegrationBrowserTest::SetUpOnMainThread();

    // Set up embedded test server to serve offscreen presentation with relative
    // URL "presentation_receiver.html".
    base::FilePath base_dir;
    CHECK(base::PathService::Get(base::DIR_MODULE, &base_dir));
    base::FilePath resource_dir = base_dir.Append(
        FILE_PATH_LITERAL("media_router/browser_test_resources/"));
    embedded_test_server()->ServeFilesFromDirectory(resource_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetTestPageUrl(const base::FilePath& full_path) override {
    return embedded_test_server()->GetURL("/basic_test.html?__oneUA__=true");
  }
};

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest, Basic) {
  RunBasicTest();
}

// TODO(https://crbug.com/822231): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest,
                       MANUAL_SendAndOnMessage) {
  RunSendMessageTest("foo");
}

// TODO(https://crbug.com/822231): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest,
                       MANUAL_ReceiverCloseConnection) {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(web_contents, kInitiateCloseFromReceiverPageScript);
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest,
                       Fail_SendMessage) {
  RunFailToSendMessageTest();
}

#if defined(OS_LINUX) &&                                        \
    (BUILDFLAG(CFI_CAST_CHECK) || BUILDFLAG(CFI_ICALL_CHECK) || \
     BUILDFLAG(CFI_ENFORCEMENT_TRAP) || BUILDFLAG(CFI_ENFORCEMENT_DIAGNOSTIC))
// https://crbug.com/966827. Flaky on Linux CFI.
#define MAYBE_ReconnectSession DISABLED_ReconnectSession
#else
#define MAYBE_ReconnectSession ReconnectSession
#endif
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest,
                       MAYBE_ReconnectSession) {
  RunReconnectSessionTest();
}
#undef MAYBE_ReconnectSession

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest,
                       ReconnectSessionSameTab) {
  RunReconnectSessionSameTabTest();
}

class MediaRouterIntegrationOneUANoReceiverBrowserTest
    : public MediaRouterIntegrationOneUABrowserTest {
 public:
  GURL GetTestPageUrl(const base::FilePath& full_path) override {
    return embedded_test_server()->GetURL(
        "/basic_test.html?__oneUANoReceiver__=true");
  }
};

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       Basic) {
  RunBasicTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       Fail_SendMessage) {
  RunFailToSendMessageTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       ReconnectSession) {
  RunReconnectSessionTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       ReconnectSessionSameTab) {
  RunReconnectSessionSameTabTest();
}

}  // namespace media_router
