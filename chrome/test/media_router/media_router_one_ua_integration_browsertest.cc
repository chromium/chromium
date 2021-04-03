// Copyright 2017 The Chromium Authors. All rights reserved.
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
        base::PathService::CheckedGet(base::DIR_MODULE)
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
    std::string presentation_id = test_provider_->get_presentation_ids().at(0);
    test_provider_->CaptureOffScreenTab(GetActiveWebContents(), receiver_page,
                                        presentation_id);
    // Wait for offscreen tab to be created and loaded.
    Wait(base::TimeDelta::FromSeconds(3));
  }
};

// TODO(https://crbug.com/822231): Flaky in Chromium waterfall.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest, MAYBE_Basic) {
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

// TODO(https://crbug.com/822231): Flaky in Chromium waterfall.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_Fail_SendMessage DISABLED_Fail_SendMessage
#else
#define MAYBE_Fail_SendMessage Fail_SendMessage
#endif
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest,
                       MAYBE_Fail_SendMessage) {
  RunFailToSendMessageTest();
}
#undef MAYBE_Fail_SendMessage

#if defined(OS_CHROMEOS) ||                                      \
    (defined(OS_LINUX) &&                                        \
     (BUILDFLAG(CFI_CAST_CHECK) || BUILDFLAG(CFI_ICALL_CHECK) || \
      BUILDFLAG(CFI_ENFORCEMENT_TRAP) ||                         \
      BUILDFLAG(CFI_ENFORCEMENT_DIAGNOSTIC)))
// https://crbug.com/966827. Flaky on Linux CFI.
// TODO(https://crbug.com/822231): Flaky in Chromium OS waterfall.
#define MAYBE_ReconnectSession DISABLED_ReconnectSession
#else
#define MAYBE_ReconnectSession ReconnectSession
#endif
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest,
                       MAYBE_ReconnectSession) {
  RunReconnectSessionTest();
}
#undef MAYBE_ReconnectSession

// TODO(https://crbug.com/822231): Flaky in Chromium waterfall.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ReconnectSessionSameTab DISABLED_ReconnectSessionSameTab
#else
#define MAYBE_ReconnectSessionSameTab ReconnectSessionSameTab
#endif
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest,
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
};

// TODO(https://crbug.com/822231): Flaky in Chromium waterfall.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       MAYBE_Basic) {
  RunBasicTest();
}
#undef MAYBE_Basic

// TODO(https://crbug.com/822231): Flaky in Chromium waterfall.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_Fail_SendMessage DISABLED_Fail_SendMessage
#else
#define MAYBE_Fail_SendMessage Fail_SendMessage
#endif
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       MAYBE_Fail_SendMessage) {
  RunFailToSendMessageTest();
}
#undef MAYBE_Fail_SendMessage

// TODO(https://crbug.com/822231): Flaky in Chromium waterfall.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ReconnectSession DISABLED_ReconnectSession
#else
#define MAYBE_ReconnectSession ReconnectSession
#endif
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       MAYBE_ReconnectSession) {
  RunReconnectSessionTest();
}

// TODO(https://crbug.com/822231): Flaky in Chromium waterfall.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ReconnectSessionSameTab DISABLED_ReconnectSessionSameTab
#else
#define MAYBE_ReconnectSessionSameTab ReconnectSessionSameTab
#endif
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       MAYBE_ReconnectSessionSameTab) {
  RunReconnectSessionSameTabTest();
}
#undef MAYBE_ReconnectSessionSameTab

}  // namespace media_router
