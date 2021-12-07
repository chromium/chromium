// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

namespace {

const char kAppHost[] = "app.com";
const char kApp2Host[] = "app2.com";
const char kNonAppHost[] = "nonapp.com";

}  // namespace

class IsolatedAppBrowserTest : public WebAppControllerBrowserTest {
 public:
  IsolatedAppBrowserTest()
      : scoped_feature_list_(blink::features::kWebAppEnableIsolatedStorage) {}

  IsolatedAppBrowserTest(const IsolatedAppBrowserTest&) = delete;
  IsolatedAppBrowserTest& operator=(const IsolatedAppBrowserTest&) = delete;
  ~IsolatedAppBrowserTest() override = default;

 protected:
  AppId InstallIsolatedApp(const std::string& host) {
    GURL app_url = https_server()->GetURL(host,
                                          "/banners/manifest_test_page.html"
                                          "?manifest=manifest_isolated.json");
    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), app_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    return test::InstallPwaForCurrentUrl(browser());
  }

  content::StoragePartition* default_storage_partition() {
    return browser()->profile()->GetDefaultStoragePartition();
  }

  content::RenderFrameHost* GetMainFrame(Browser* browser) {
    return browser->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IsolatedAppBrowserTest, AppsPartitioned) {
  InstallIsolatedApp(kAppHost);
  InstallIsolatedApp(kApp2Host);

  auto* non_app_frame = ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("/banners/isolated/simple.html"));
  EXPECT_TRUE(non_app_frame);
  EXPECT_EQ(default_storage_partition(), non_app_frame->GetStoragePartition());

  auto* app_window = NavigateInNewWindowAndAwaitInstallabilityCheck(
      https_server()->GetURL(kAppHost, "/banners/isolated/simple.html"));
  auto* app_frame = GetMainFrame(app_window);
  EXPECT_NE(default_storage_partition(), app_frame->GetStoragePartition());

  auto* app2_window = NavigateInNewWindowAndAwaitInstallabilityCheck(
      https_server()->GetURL(kApp2Host, "/banners/isolated/simple.html"));
  auto* app2_frame = GetMainFrame(app2_window);
  EXPECT_NE(default_storage_partition(), app2_frame->GetStoragePartition());

  EXPECT_NE(app_frame->GetStoragePartition(),
            app2_frame->GetStoragePartition());
}

class IsolatedAppBrowserCookieTest : public IsolatedAppBrowserTest {
 public:
  using CookieHeaders = std::vector<std::string>;

  void SetUpOnMainThread() override {
    https_server()->RegisterRequestMonitor(base::BindRepeating(
        &IsolatedAppBrowserCookieTest::MonitorRequest, base::Unretained(this)));

    IsolatedAppBrowserTest::SetUpOnMainThread();
  }

 protected:
  // Returns the "Cookie" headers that were received for the given URL.
  const CookieHeaders& GetCookieHeadersForUrl(const GURL& url) {
    return cookie_map_[url.spec()];
  }

  void CreateIframe(content::RenderFrameHost* parent_frame,
                    const std::string& iframe_id,
                    const GURL& url) {
    EXPECT_EQ(true, content::EvalJs(parent_frame,
                                    content::JsReplace(R"(
            new Promise(resolve => {
              let f = document.createElement('iframe');
              f.id = $1;
              f.src = $2;
              f.addEventListener('load', () => resolve(true));
              document.body.appendChild(f);
            });
        )",
                                                       iframe_id, url)));
  }

  content::RenderFrameHost* NavigateToURLInNewTab(const GURL& url) {
    auto new_contents = content::WebContents::Create(
        content::WebContents::CreateParams(browser()->profile()));
    browser()->tab_strip_model()->AppendWebContents(std::move(new_contents),
                                                    /*foreground=*/true);
    return ui_test_utils::NavigateToURL(browser(), url);
  }

 private:
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    // Replace the host in |request.GetURL()| with the value from the Host
    // header, as GetURL()'s host will be 127.0.0.1.
    std::string host = GURL("https://" + GetHeader(request, "Host")).host();
    GURL::Replacements replace_host;
    replace_host.SetHostStr(host);
    GURL url = request.GetURL().ReplaceComponents(replace_host);
    cookie_map_[url.spec()].push_back(GetHeader(request, "cookie"));
  }

  std::string GetHeader(const net::test_server::HttpRequest& request,
                        std::string header_name) {
    auto header = request.headers.find(header_name);
    return header != request.headers.end() ? header->second : "";
  }

  // Maps GURLs to a vector of cookie strings. The nth item in the vector will
  // contain the contents of the "Cookies" header for the nth request to the
  // given GURL.
  std::unordered_map<std::string, CookieHeaders> cookie_map_;
};

IN_PROC_BROWSER_TEST_F(IsolatedAppBrowserCookieTest, Cookies) {
  InstallIsolatedApp(kAppHost);

  // Load a page that sets a cookie, then create a cross-origin iframe that
  // loads the same page.
  GURL app_url =
      https_server()->GetURL(kAppHost, "/banners/isolated/cookie.html");
  auto* app_window = NavigateInNewWindowAndAwaitInstallabilityCheck(app_url);
  auto* app_frame = GetMainFrame(app_window);
  GURL non_app_url =
      https_server()->GetURL(kNonAppHost, "/banners/isolated/cookie.html");
  CreateIframe(app_frame, "child", non_app_url);

  const auto& app_cookies = GetCookieHeadersForUrl(app_url);
  EXPECT_EQ(1u, app_cookies.size());
  EXPECT_TRUE(app_cookies[0].empty());
  const auto& non_app_cookies = GetCookieHeadersForUrl(non_app_url);
  EXPECT_EQ(1u, non_app_cookies.size());
  EXPECT_TRUE(non_app_cookies[0].empty());

  // Load the pages again. Both frames should send the cookie in their requests.
  auto* app_window2 = NavigateInNewWindowAndAwaitInstallabilityCheck(app_url);
  auto* app_frame2 = GetMainFrame(app_window2);
  CreateIframe(app_frame2, "child", non_app_url);

  EXPECT_EQ(2u, app_cookies.size());
  EXPECT_EQ("foo=bar", app_cookies[1]);
  EXPECT_EQ(2u, non_app_cookies.size());
  EXPECT_EQ("foo=bar", non_app_cookies[1]);

  // Load the cross-origin's iframe as a top-level page. Because this page was
  // previously loaded in an isolated app, it shouldn't have cookies set when
  // loaded in a main frame here.
  ASSERT_TRUE(NavigateToURLInNewTab(non_app_url));

  EXPECT_EQ(3u, non_app_cookies.size());
  EXPECT_TRUE(non_app_cookies[2].empty());
}

}  // namespace web_app
