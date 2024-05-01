// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "ipc/ipc_security_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/webui/untrusted_web_ui_browsertest_util.h"
#include "url/url_constants.h"

// Tests embedder specific behavior of WebUIs.
class ChromeWebUINavigationBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  content::TestWebUIControllerFactory factory_;
  content::ScopedWebUIControllerFactoryRegistration factory_registration_{
      &factory_};
};

// Verify that a browser check stops websites from embeding chrome:// iframes.
// This is a copy of the DisallowEmbeddingChromeSchemeFromWebFrameBrowserCheck
// test in content/browser/webui/web_ui_navigation_browsertest.cc. We need a
// copy here because the browser side check is done by embedders.
IN_PROC_BROWSER_TEST_F(ChromeWebUINavigationBrowserTest,
                       DisallowEmbeddingChromeSchemeFromWebFrameBrowserCheck) {
  GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  auto* main_frame = web_contents->GetPrimaryMainFrame();

  // Add iframe but don't navigate it to a chrome:// URL yet.
  EXPECT_TRUE(content::ExecJs(main_frame,
                              "var frame = document.createElement('iframe');\n"
                              "document.body.appendChild(frame);\n",
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              1 /* world_id */));

  content::RenderFrameHost* child = content::ChildFrameAt(main_frame, 0);
  EXPECT_EQ("about:blank", child->GetLastCommittedURL());

  content::TestNavigationObserver observer(web_contents);
  content::PwnMessageHelper::OpenURL(
      child, content::GetWebUIURL("web-ui/title1.html?noxfo=true"));
  observer.Wait();

  // Retrieve the RenderFrameHost again since it might have been swapped.
  child = content::ChildFrameAt(main_frame, 0);
  EXPECT_EQ(content::kBlockedURL, child->GetLastCommittedURL());
}

// Verify that a browser check stops websites from embeding chrome-untrusted://
// iframes. This is a copy of the
// DisallowEmbeddingChromeUntrustedSchemeFromWebFrameBrowserCheck test in
// content/browser/webui/web_ui_navigation_browsertest.cc. We need a copy here
// because the browser side check is done by embedders.
IN_PROC_BROWSER_TEST_F(
    ChromeWebUINavigationBrowserTest,
    DisallowEmbeddingChromeUntrustedSchemeFromWebFrameBrowserCheck) {
  GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  auto* main_frame = web_contents->GetPrimaryMainFrame();

  // Add iframe but don't navigate it to a chrome-untrusted:// URL yet.
  EXPECT_TRUE(content::ExecJs(main_frame,
                              "var frame = document.createElement('iframe');\n"
                              "document.body.appendChild(frame);\n",
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              1 /* world_id */));

  content::RenderFrameHost* child = content::ChildFrameAt(main_frame, 0);
  EXPECT_EQ("about:blank", child->GetLastCommittedURL());

  content::TestNavigationObserver observer(web_contents);
  content::TestUntrustedDataSourceHeaders headers;
  headers.no_xfo = true;
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-iframe-host",
                                                     headers));

  content::PwnMessageHelper::OpenURL(
      child, content::GetChromeUntrustedUIURL("test-iframe-host/title1.html"));
  observer.Wait();

  // Retrieve the RenderFrameHost again since it might have been swapped.
  child = content::ChildFrameAt(main_frame, 0);
  EXPECT_EQ(content::kBlockedURL, child->GetLastCommittedURL());
}
