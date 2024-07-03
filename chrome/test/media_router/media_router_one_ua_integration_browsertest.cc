// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include "base/cfi_buildflags.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/media_router/media_router_integration_browsertest.h"
#include "content/public/test/browser_test.h"
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
    base::FilePath resource_dir =
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .Append(FILE_PATH_LITERAL("media_router/browser_test_resources/"));
    embedded_test_server()->ServeFilesFromDirectory(resource_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetTestPageUrl(const base::FilePath& full_path) override {
    return embedded_test_server()->GetURL("/basic_test.html?__oneUA__=true");
  }

  WebContents* StartSessionWithTestPageAndChooseSink() override {
    WebContents* web_contents = MediaRouterIntegrationBrowserTest::
        StartSessionWithTestPageAndChooseSink();
    CaptureOffScreenTab();
    return web_contents;
  }

  void CaptureOffScreenTab() {
    GURL receiver_page =
        embedded_test_server()->GetURL("/presentation_receiver.html");
    EXPECT_EQ(static_cast<int>(test_provider_->get_presentation_ids().size()),
              1);
    std::string presentation_id = test_provider_->get_presentation_ids().at(0);
    test_provider_->CaptureOffScreenTab(GetActiveWebContents(), receiver_page,
                                        presentation_id);
    // Wait for offscreen tab to be created and loaded.
    Wait(base::Seconds(3));
  }
};

// TODO(crbug.com/40567200): Flaky in Chromium waterfall.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Basic MANUAL_Basic
#else
#define MAYBE_Basic Basic
#endif
IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationOneUABrowserTest, MAYBE_Basic) {
  RunBasicTest();
}
#undef MAYBE_Basic

// TODO(crbug.com/40567200): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationOneUABrowserTest,
                       MANUAL_SendAndOnMessage) {
  RunSendMessageTest("foo");
}

// TODO(crbug.com/40567200): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationOneUABrowserTest,
                       MANUAL_ReceiverCloseConnection) {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(web_contents, kInitiateCloseFromReceiverPageScript);
}

// TODO(crbug.com/40567200): Flaky in Chromium waterfall.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_Fail_SendMessage MANUAL_Fail_SendMessage
#else
#define MAYBE_Fail_SendMessage Fail_SendMessage
#endif
IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationOneUABrowserTest,
                       MAYBE_Fail_SendMessage) {
  RunFailToSendMessageTest();
}
#undef MAYBE_Fail_SendMessage

#if BUILDFLAG(IS_CHROMEOS) ||                                    \
    (BUILDFLAG(IS_LINUX) &&                                      \
     (BUILDFLAG(CFI_CAST_CHECK) || BUILDFLAG(CFI_ICALL_CHECK) || \
      BUILDFLAG(CFI_ENFORCEMENT_TRAP) ||                         \
      BUILDFLAG(CFI_ENFORCEMENT_DIAGNOSTIC)))
// https://crbug.com/966827. Flaky on Linux CFI.
// TODO(crbug.com/40567200): Flaky in Chromium OS waterfall.
#define MAYBE_ReconnectSession MANUAL_ReconnectSession
#else
#define MAYBE_ReconnectSession ReconnectSession
#endif
IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationOneUABrowserTest,
                       MAYBE_ReconnectSession) {
  RunReconnectSessionTest();
}
#undef MAYBE_ReconnectSession

// TODO(crbug.com/40567200): Flaky in Chromium waterfall.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ReconnectSessionSameTab MANUAL_ReconnectSessionSameTab
#else
#define MAYBE_ReconnectSessionSameTab ReconnectSessionSameTab
#endif
IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationOneUABrowserTest,
                       MAYBE_ReconnectSessionSameTab) {
  RunReconnectSessionSameTabTest();
}
#undef MAYBE_ReconnectSessionSameTab

class MediaRouterIntegrationOneUANoReceiverBrowserTest
    : public MediaRouterIntegrationOneUABrowserTest {
 public:
  GURL GetTestPageUrl(const base::FilePath& full_path) override {
    return embedded_test_server()->GetURL(
        "/basic_test.html?__oneUANoReceiver__=true");
  }
};  // namespace media_router

// TODO(crbug.com/40567200): Flaky in Chromium waterfall.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_Basic MANUAL_Basic
#else
#define MAYBE_Basic Basic
#endif
IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       MAYBE_Basic) {
  RunBasicTest();
}
#undef MAYBE_Basic

// TODO(crbug.com/40567200): Flaky in Chromium waterfall.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_Fail_SendMessage MANUAL_Fail_SendMessage
#else
#define MAYBE_Fail_SendMessage Fail_SendMessage
#endif
IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       MAYBE_Fail_SendMessage) {
  RunFailToSendMessageTest();
}
#undef MAYBE_Fail_SendMessage

// TODO(crbug.com/40567200): Flaky in Chromium waterfall.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ReconnectSession MANUAL_ReconnectSession
#else
#define MAYBE_ReconnectSession ReconnectSession
#endif
IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       MAYBE_ReconnectSession) {
  RunReconnectSessionTest();
}

// TODO(crbug.com/40567200): Flaky in Chromium waterfall.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ReconnectSessionSameTab MANUAL_ReconnectSessionSameTab
#else
#define MAYBE_ReconnectSessionSameTab ReconnectSessionSameTab
#endif
IN_PROC_BROWSER_TEST_P(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       MAYBE_ReconnectSessionSameTab) {
  RunReconnectSessionSameTabTest();
}
#undef MAYBE_ReconnectSessionSameTab

INSTANTIATE_MEDIA_ROUTER_INTEGRATION_BROWER_TEST_SUITE(
    MediaRouterIntegrationOneUABrowserTest);
INSTANTIATE_MEDIA_ROUTER_INTEGRATION_BROWER_TEST_SUITE(
    MediaRouterIntegrationOneUANoReceiverBrowserTest);

}  // namespace media_router
