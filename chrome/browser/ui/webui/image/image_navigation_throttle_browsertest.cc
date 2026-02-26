// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace {

const std::string_view kImageName = "/handbag.png";

GURL IndirectedImageUrlForServer(net::test_server::EmbeddedTestServer& server) {
  return GURL("chrome://image/?" + server.GetURL(kImageName).spec());
}

class ImageNavigationThrottleTest : public InProcessBrowserTest {
  void SetUp() override {
    server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    server_->ServeFilesFromSourceDirectory("chrome/test/data");

    server_handle_ = server_->StartAndReturnHandle();

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {}

 protected:
  net::test_server::EmbeddedTestServer& server() { return *server_.get(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> server_;
  net::test_server::EmbeddedTestServerHandle server_handle_;
};

IN_PROC_BROWSER_TEST_F(ImageNavigationThrottleTest, FrameNavigationFails) {
  auto* contents = chrome_test_utils::GetActiveWebContents(this);
  EXPECT_FALSE(chrome_test_utils::NavigateToURL(
      contents, IndirectedImageUrlForServer(server())));
  EXPECT_TRUE(contents->GetPrimaryMainFrame()->IsErrorDocument());
}

IN_PROC_BROWSER_TEST_F(ImageNavigationThrottleTest, SubresourceLoadSucceeds) {
  // To test that subresource loads of chrome://image succeed, we need to load a
  // WebUI, then cause it to load a chrome://image subresource, so...

  // Ensure that chrome://image URLs are available in WebUIs. WebUI data sources
  // are registered per-profile and available in all WebUIs once registered.
  auto* profile = browser()->profile();
  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));

  // Navigate a tab to a very simple WebUI page - this uses
  // chrome://chrome-urls.
  auto* contents = chrome_test_utils::GetActiveWebContents(this);
  EXPECT_TRUE(chrome_test_utils::NavigateToURL(
      contents, GURL(chrome::kChromeUIChromeURLsURL)));

  // Add a JS function doesload(url) which returns whether a given image URL
  // loads.
  EXPECT_TRUE(content::ExecJs(contents,
                              R"(function tryload(url) {
          return new Promise((resolve, reject) => {
            let img = new Image();
            img.src = url;
            img.onload = resolve;
            img.onerror = reject;
          })
        };
        function doesload(url) {
          return tryload(url).then((v) => { return true; })
                             .catch((e) => { return false; })
        };
        )"));

  auto doesload = [&](GURL url) {
    std::string js = absl::StrFormat("doesload(\"%s\")", url.spec());
    // This may look silly, but using operator== here is necessary to turn the
    // EvalJsResult into a bool.
    return content::EvalJs(contents, js) == true;
  };

  // Loading an image URL is blocked...
  EXPECT_FALSE(doesload(server().GetURL(kImageName)));

  // ... but loading one via chrome://image works.
  EXPECT_TRUE(doesload(IndirectedImageUrlForServer(server())));
}

}  // namespace
