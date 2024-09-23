// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/renderer/searchbox/searchbox.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_thread.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_test_sink.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/extension_test_util.h"
#include "extensions/common/extensions_client.h"
#endif

using ChromeContentRendererClientSearchBoxTest = ChromeRenderViewTest;

const char kHtmlWithIframe[] ="<iframe srcdoc=\"Nothing here\"></iframe>";

TEST_F(ChromeContentRendererClientSearchBoxTest, RewriteThumbnailURL) {
  // Instantiate a SearchBox for the main render frame.
  content::RenderFrame* render_frame =
      content::RenderFrame::FromWebFrame(GetMainFrame());
  new SearchBox(render_frame);

  // Load a page that contains an iframe.
  LoadHTML(kHtmlWithIframe);

  ChromeContentRendererClient* client =
      static_cast<ChromeContentRendererClient*>(content_renderer_client_.get());

  // Create a thumbnail URL containing the correct render frame ID and an
  // arbitrary instant restricted ID.
  GURL thumbnail_url(base::StringPrintf(
      "chrome-search:/thumb/%s/1",
      render_frame->GetWebFrame()->GetLocalFrameToken().ToString().c_str()));

  GURL result;
  // Make sure the SearchBox rewrites a thumbnail request from the main frame.
  client->WillSendRequest(GetMainFrame(), ui::PAGE_TRANSITION_LINK,
                          /*upstream_url=*/GURL(), blink::WebURL(thumbnail_url),
                          net::SiteForCookies(), nullptr, &result);
  EXPECT_NE(result, thumbnail_url);

  // Make sure the SearchBox rewrites a thumbnail request from the iframe.
  blink::WebFrame* child_frame = GetMainFrame()->FirstChild();
  ASSERT_TRUE(child_frame);
  ASSERT_TRUE(child_frame->IsWebLocalFrame());
  blink::WebLocalFrame* local_child =
      static_cast<blink::WebLocalFrame*>(child_frame);
  client->WillSendRequest(local_child, ui::PAGE_TRANSITION_LINK,
                          /*upstream_url=*/GURL(), blink::WebURL(thumbnail_url),
                          net::SiteForCookies(), nullptr, &result);
  EXPECT_NE(result, thumbnail_url);
}

// The tests below examine Youtube requests that use the Flash API and ensure
// that the requests have been modified to instead use HTML5. The tests also
// check the MIME type of the request to ensure that it is "text/html".
namespace {

struct FlashEmbedsTestData {
  std::string name;
  std::string host;
  std::string path;
  std::string type;
  std::string expected_url;
};

const FlashEmbedsTestData kFlashEmbedsTestData[] = {
    {"Valid URL, no parameters", "www.youtube.com", "/v/deadbeef",
     "application/x-shockwave-flash", "/embed/deadbeef"},
    {"Valid URL, no parameters, subdomain", "www.foo.youtube.com",
     "/v/deadbeef", "application/x-shockwave-flash", "/embed/deadbeef"},
    {"Valid URL, many parameters", "www.youtube.com",
     "/v/deadbeef?start=4&fs=1", "application/x-shockwave-flash",
     "/embed/deadbeef?start=4&fs=1"},
    {"Invalid parameter construct, many parameters", "www.youtube.com",
     "/v/deadbeef&bar=4&foo=6", "application/x-shockwave-flash",
     "/embed/deadbeef?bar=4&foo=6"},
    {"Valid URL, enablejsapi=1", "www.youtube.com", "/v/deadbeef?enablejsapi=1",
     "application/x-shockwave-flash", "/embed/deadbeef?enablejsapi=1"}};

}  // namespace

class ChromeContentRendererClientBrowserTest :
    public InProcessBrowserTest,
    public ::testing::WithParamInterface<FlashEmbedsTestData> {
 public:
  ChromeContentRendererClientBrowserTest()
      : https_server_(std::make_unique<net::EmbeddedTestServer>(
            net::EmbeddedTestServer::TYPE_HTTPS)) {}

  void MonitorRequestHandler(const net::test_server::HttpRequest& request) {
    // We're only interested in YouTube video embeds
    if (request.headers.at("Host").find("youtube.com") == std::string::npos)
      return;

    if (request.relative_url.find("/embed") != 0 &&
        request.relative_url.find("/v") != 0)
      return;

    auto type = request.headers.find("Accept");
    EXPECT_NE(std::string::npos, type->second.find("text/html"))
        << "Type is not text/html for test " << GetParam().name;

    EXPECT_EQ(request.relative_url, GetParam().expected_url)
        << "URL is wrong for test " << GetParam().name;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, message_runner_->QuitClosure());
  }

  void WaitForYouTubeRequest() {
    message_runner_->Run();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    https_server_->RegisterRequestMonitor(base::BindRepeating(
        &ChromeContentRendererClientBrowserTest::MonitorRequestHandler,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());
    message_runner_ = new content::MessageLoopRunner();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

 protected:
  net::EmbeddedTestServer* https_server() const { return https_server_.get(); }

 private:
  scoped_refptr<content::MessageLoopRunner> message_runner_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_P(ChromeContentRendererClientBrowserTest,
                       RewriteYouTubeFlashEmbed) {
  GURL url(https_server()->GetURL("/flash_embeds.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL video_url = https_server()->GetURL(GetParam().host, GetParam().path);
  EXPECT_TRUE(ExecJs(web_contents, "appendEmbedToDOM('" + video_url.spec() +
                                       "','" + GetParam().type + "');"));
  WaitForYouTubeRequest();
}

IN_PROC_BROWSER_TEST_P(ChromeContentRendererClientBrowserTest,
                       RewriteYouTubeFlashEmbedObject) {
  GURL url(https_server()->GetURL("/flash_embeds.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* web_contents =
     browser()->tab_strip_model()->GetActiveWebContents();

  GURL video_url = https_server()->GetURL(GetParam().host, GetParam().path);
  EXPECT_TRUE(ExecJs(web_contents, "appendDataEmbedToDOM('" + video_url.spec() +
                                       "','" + GetParam().type + "');"));
  WaitForYouTubeRequest();
}

INSTANTIATE_TEST_SUITE_P(FlashEmbeds,
                         ChromeContentRendererClientBrowserTest,
                         ::testing::ValuesIn(kFlashEmbedsTestData));

#if BUILDFLAG(ENABLE_EXTENSIONS)
IN_PROC_BROWSER_TEST_F(ChromeContentRendererClientBrowserTest,
                       AvailabilityMapCreated) {
  auto* extensions_client = extensions::ExtensionsClient::Get();
  ASSERT_TRUE(extensions_client);

  // ChromeContentRendererClient initializes the ExtensionClient with the
  // FeatureDelegatedAvailabilityMap, which will maintain ownership of the map.
  // Verify that the map is created correctly.
  {
    const auto& map =
        extensions_client->GetFeatureDelegatedAvailabilityCheckMap();
    EXPECT_EQ(7u, map.size());
    for (const auto* feature :
         extension_test_util::GetExpectedDelegatedFeaturesForTest()) {
      EXPECT_EQ(1u, map.count(feature));
    }
  }
}
#endif
