// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_proto_provider.h"

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "ui/display/display_switches.h"

namespace optimization_guide {

namespace {

// Allow 1px differences from rounding.
#define EXPECT_ALMOST_EQ(a, b) EXPECT_LE(abs(a - b), 1);

base::FilePath GetTestDataDir() {
  return base::FilePath(
      FILE_PATH_LITERAL("components/test/data/optimization_guide"));
}

void AssertHasText(const optimization_guide::proto::ContentNode& node,
                   std::string text) {
  EXPECT_EQ(node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);
  EXPECT_EQ(node.children_nodes().size(), 1);
  const auto& text_node = node.children_nodes().at(0);
  EXPECT_EQ(text_node.content_attributes().text_data().text_content(), text);
}

void AssertRectsEqual(const optimization_guide::proto::BoundingRect& proto_rect,
                      gfx::Rect rect) {
  EXPECT_EQ(proto_rect.width(), rect.width());
  EXPECT_EQ(proto_rect.height(), rect.height());
  EXPECT_EQ(proto_rect.x(), rect.x());
  EXPECT_EQ(proto_rect.y(), rect.y());
}

void AssertRectsEqual(const optimization_guide::proto::BoundingRect& a,
                      const optimization_guide::proto::BoundingRect& b) {
  EXPECT_EQ(a.width(), b.width());
  EXPECT_EQ(a.height(), b.height());
  EXPECT_EQ(a.x(), b.x());
  EXPECT_EQ(a.y(), b.y());
}

void AssertValidURL(std::string url, std::string host) {
  GURL gurl(url);
  EXPECT_TRUE(gurl.is_valid());
  EXPECT_TRUE(gurl.SchemeIsHTTPOrHTTPS());
  EXPECT_EQ(gurl.host(), host);
}

class PageContentProtoProviderBrowserTest : public content::ContentBrowserTest {
 public:
  PageContentProtoProviderBrowserTest() = default;

  PageContentProtoProviderBrowserTest(
      const PageContentProtoProviderBrowserTest&) = delete;
  PageContentProtoProviderBrowserTest& operator=(
      const PageContentProtoProviderBrowserTest&) = delete;

  ~PageContentProtoProviderBrowserTest() override = default;

  content::WebContents* web_contents() { return shell()->web_contents(); }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->AddDefaultHandlers(GetTestDataDir());
    content::SetupCrossSiteRedirector(https_server_.get());

    ASSERT_TRUE(https_server_->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);

    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);

    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1.0");
  }

  void SetPageContent(base::OnceClosure quit_closure,
                      std::optional<proto::AnnotatedPageContent> page_content) {
    page_content_ = std::move(page_content);
    std::move(quit_closure).Run();
  }

  const proto::AnnotatedPageContent& page_content() { return *page_content_; }

  void LoadData(blink::mojom::AIPageContentOptionsPtr request =
                    DefaultAIPageContentOptions()) {
    base::RunLoop run_loop;
    GetAIPageContent(
        web_contents(), std::move(request),
        base::BindOnce(&PageContentProtoProviderBrowserTest::SetPageContent,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    CHECK(page_content_);
  }

  void LoadPage(GURL url, bool with_page_content = true) {
    content::NavigateToURLBlockUntilNavigationsComplete(web_contents(), url, 1);

    {
      base::test::TestFuture<bool> future;
      web_contents()
          ->GetPrimaryMainFrame()
          ->GetRenderWidgetHost()
          ->InsertVisualStateCallback(future.GetCallback());
      ASSERT_TRUE(future.Wait()) << "Timeout waiting for syncing with renderer";
    }

    if (with_page_content) {
      LoadData();
    }
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::optional<proto::AnnotatedPageContent> page_content_;
};

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest, AIPageContent) {
  const gfx::Size window_bounds(web_contents()->GetSize());
  LoadPage(https_server()->GetURL("/simple.html"));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  AssertHasText(page_content().root_node(), "Non empty simple page\n\n");

  const auto& root_geometry =
      page_content().root_node().content_attributes().geometry();
  EXPECT_EQ(root_geometry.outer_bounding_box().x(), 0);
  EXPECT_EQ(root_geometry.outer_bounding_box().y(), 0);
  EXPECT_EQ(root_geometry.outer_bounding_box().width(), window_bounds.width());
  EXPECT_EQ(root_geometry.outer_bounding_box().height(),
            window_bounds.height());

  EXPECT_EQ(root_geometry.visible_bounding_box().x(), 0);
  EXPECT_EQ(root_geometry.visible_bounding_box().y(), 0);
  EXPECT_EQ(root_geometry.visible_bounding_box().width(),
            window_bounds.width());
  EXPECT_EQ(root_geometry.visible_bounding_box().height(),
            window_bounds.height());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentNoGeometry) {
  LoadPage(https_server()->GetURL("/simple.html"),
           /* with_page_content = */ false);

  auto request = blink::mojom::AIPageContentOptions::New();
  request->include_geometry = false;
  LoadData(std::move(request));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  AssertHasText(page_content().root_node(), "Non empty simple page\n\n");
  EXPECT_FALSE(page_content().root_node().content_attributes().has_geometry());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentNoCriticalPath) {
  LoadPage(https_server()->GetURL("/simple.html"),
           /* with_page_content = */ false);

  auto request = blink::mojom::AIPageContentOptions::New();
  request->on_critical_path = false;
  LoadData(std::move(request));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  AssertHasText(page_content().root_node(), "Non empty simple page\n\n");
  EXPECT_TRUE(page_content().root_node().content_attributes().has_geometry());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentImageDataURL) {
  LoadPage(https_server()->GetURL("a.com", "/data_image.html"));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  const auto& image_node = page_content().root_node().children_nodes().at(0);

  ASSERT_TRUE(image_node.content_attributes().has_image_data());
  const auto& image_data = image_node.content_attributes().image_data();
  // TODO(crbug.com/382558422): Propagate image source URLs, this should be
  // a.com.
  EXPECT_TRUE(image_data.source_url().empty());
}

namespace {

std::string GetFilePathWithHostAndPortReplacement(
    const std::string& original_file_path,
    const net::HostPortPair& host_port_pair) {
  base::StringPairs replacement_text;
  replacement_text.push_back(
      make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
  return net::test_server::GetFilePathWithReplacements(original_file_path,
                                                       replacement_text);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentCrossOriginImage) {
  // Add a "replace_text=" query param that the test server will use to replace
  // the string "REPLACE_WITH_HOST_AND_PORT" in the destination page.
  net::HostPortPair host_port_pair =
      net::HostPortPair::FromURL(https_server()->GetURL("b.com", "/"));
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/cross_origin_image.html", host_port_pair);

  LoadPage(https_server()->GetURL("a.com", replacement_path));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  const auto& image_node = page_content().root_node().children_nodes().at(0);

  ASSERT_TRUE(image_node.content_attributes().has_image_data());
  const auto& image_data = image_node.content_attributes().image_data();
  // TODO(crbug.com/382558422): Propagate image source URLs, this should be
  // b.com.
  EXPECT_TRUE(image_data.source_url().empty());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentSandboxedIframe) {
  LoadPage(https_server()->GetURL("a.com", "/paragraph_iframe_sandbox.html"));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);

  const auto& iframe = page_content().root_node().children_nodes()[0];
  EXPECT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& iframe_data = iframe.content_attributes().iframe_data();
  EXPECT_TRUE(iframe_data.url().empty());
  EXPECT_FALSE(iframe_data.likely_ad_frame());

  EXPECT_EQ(iframe.children_nodes().size(), 1);
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentIframeDataURL) {
  LoadPage(https_server()->GetURL("a.com", "/paragraph_iframe_data_url.html"));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);

  const auto& iframe = page_content().root_node().children_nodes()[0];
  EXPECT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& iframe_data = iframe.content_attributes().iframe_data();
  EXPECT_TRUE(iframe_data.url().empty());
  EXPECT_FALSE(iframe_data.likely_ad_frame());

  EXPECT_EQ(iframe.children_nodes().size(), 1);
}

class PageContentProtoProviderBrowserTestSiteIsolation
    : public PageContentProtoProviderBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  bool EnableCrossSiteFrames() const { return GetParam(); }

  std::string QueryParam() const {
    return EnableCrossSiteFrames() ? "?domain=/cross-site/b.com/" : "";
  }
};

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestSiteIsolation,
                       LatencyMetrics) {
  base::HistogramTester tester;

  LoadPage(https_server()->GetURL(
      "a.com",
      base::StringPrintf("/paragraph_iframe_partially_offscreen.html%s",
                         QueryParam())));
  ASSERT_EQ(page_content().root_node().children_nodes().size(), 1);
  content::FetchHistogramsFromChildProcesses();

  constexpr char kMainFrame[] =
      "OptimizationGuide.AIPageContent.RendererLatency.MainFrame";
  constexpr char kMainFrameSchedulingDelay[] =
      "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
      "Critical.MainFrame";
  constexpr char kRemoteSubframe[] =
      "OptimizationGuide.AIPageContent.RendererLatency.RemoteSubFrame";
  constexpr char kRemoteSubframeSchedulingDelay[] =
      "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
      "Critical.RemoteSubFrame";
  constexpr char kTotal[] = "OptimizationGuide.AIPageContent.TotalLatency";

  tester.ExpectTotalCount(kMainFrame, 1);
  tester.ExpectTotalCount(kMainFrameSchedulingDelay, 1);
  tester.ExpectTotalCount(kTotal, 1);

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/384585933): Enable this assert on Android.
  if (EnableCrossSiteFrames()) {
    return;
  }
#endif
  tester.ExpectTotalCount(kRemoteSubframe, EnableCrossSiteFrames() ? 1 : 0);
  tester.ExpectTotalCount(kRemoteSubframeSchedulingDelay,
                          EnableCrossSiteFrames() ? 1 : 0);
}

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestSiteIsolation,
                       LatencyMetricsNotOnCriticalPath) {
  base::HistogramTester tester;

  LoadPage(https_server()->GetURL(
               "a.com", base::StringPrintf(
                            "/paragraph_iframe_partially_offscreen.html%s",
                            QueryParam())),
           /* with_page_content = */ false);

  auto request = optimization_guide::DefaultAIPageContentOptions();
  request->on_critical_path = false;
  LoadData(std::move(request));
  content::FetchHistogramsFromChildProcesses();

  ASSERT_EQ(page_content().root_node().children_nodes().size(), 1);

  constexpr char kMainFrame[] =
      "OptimizationGuide.AIPageContent.RendererLatency.MainFrame";
  constexpr char kMainFrameSchedulingDelay[] =
      "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
      "NonCritical.MainFrame";
  constexpr char kRemoteSubframe[] =
      "OptimizationGuide.AIPageContent.RendererLatency.RemoteSubFrame";
  constexpr char kRemoteSubframeSchedulingDelay[] =
      "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
      "Critical.RemoteSubFrame";
  constexpr char kRemoteSubframeSchedulingDelayNonCritical[] =
      "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
      "NonCritical.RemoteSubFrame";
  constexpr char kTotal[] = "OptimizationGuide.AIPageContent.TotalLatency";

  tester.ExpectTotalCount(kMainFrame, 1);
  tester.ExpectTotalCount(kMainFrameSchedulingDelay, 1);
  tester.ExpectTotalCount(kTotal, 1);

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/384585933): Enable this assert on Android.
  if (EnableCrossSiteFrames()) {
    return;
  }
#endif
  // TODO(crbug.com/389737599): We should have a metric for subframes once they
  // can use off critical path scheduling.
  tester.ExpectTotalCount(kRemoteSubframeSchedulingDelayNonCritical, 0);
  tester.ExpectTotalCount(kRemoteSubframe, EnableCrossSiteFrames() ? 1 : 0);
  tester.ExpectTotalCount(kRemoteSubframeSchedulingDelay,
                          EnableCrossSiteFrames() ? 1 : 0);
}

// Ensure that clip from an ancestor frame is included in visible rect
// computation.
IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestSiteIsolation,
                       AIPageContentIframePartiallyOffscreen) {
  LoadPage(https_server()->GetURL(
      "a.com",
      base::StringPrintf("/paragraph_iframe_partially_offscreen.html%s",
                         QueryParam())));
  ASSERT_EQ(page_content().root_node().children_nodes().size(), 1);

  const auto& iframe = page_content().root_node().children_nodes()[0];
  ASSERT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);

  ASSERT_EQ(iframe.children_nodes().size(), 1);
  const auto& iframe_root = iframe.children_nodes()[0];
  ASSERT_EQ(iframe_root.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  ASSERT_EQ(iframe_root.children_nodes().size(), 1);
  const auto& p = iframe_root.children_nodes()[0];
  EXPECT_EQ(p.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);
  EXPECT_EQ(p.content_attributes().annotated_roles().size(), 0);
  const auto& geometry = p.content_attributes().geometry();
  AssertRectsEqual(geometry.outer_bounding_box(),
                   gfx::Rect(-20, -10, 100, 200));
  AssertRectsEqual(geometry.visible_bounding_box(), gfx::Rect(0, 0, 80, 190));
}

// Ensure that clip from an ancestor frame's root scroller are included in
// visible rect computation.
IN_PROC_BROWSER_TEST_P(
    PageContentProtoProviderBrowserTestSiteIsolation,
    AIPageContentIframePartiallyOffscreenAncestorRootScroller) {
  LoadPage(https_server()->GetURL(
      "a.com", base::StringPrintf(
                   "/paragraph_iframe_partially_scrolled_offscreen.html%s",
                   QueryParam())));

  ASSERT_EQ(page_content().root_node().children_nodes().size(), 1);

  const auto& iframe = page_content().root_node().children_nodes()[0];
  ASSERT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);

  ASSERT_EQ(iframe.children_nodes().size(), 1);
  const auto& iframe_root = iframe.children_nodes()[0];
  ASSERT_EQ(iframe_root.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  const auto& p = iframe_root.children_nodes()[0];
  EXPECT_EQ(p.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);
  EXPECT_EQ(p.content_attributes().annotated_roles().size(), 0);

// TODO(khushalsagar): This is an existing bug where the scroll offset of the
// root scroller in the ancestor remote frame is not applied.
#if !BUILDFLAG(IS_ANDROID)
  const auto& geometry = p.content_attributes().geometry();
  AssertRectsEqual(geometry.outer_bounding_box(),
                   gfx::Rect(-20, -10, 100, 200));
  AssertRectsEqual(geometry.visible_bounding_box(), gfx::Rect(0, 0, 80, 190));
#endif
}

INSTANTIATE_TEST_SUITE_P(All,
                         PageContentProtoProviderBrowserTestSiteIsolation,
                         testing::Bool());

class PageContentProtoProviderBrowserTestMultiProcess
    : public PageContentProtoProviderBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  bool EnableProcessIsolation() const { return GetParam(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PageContentProtoProviderBrowserTest::SetUpCommandLine(command_line);

    if (EnableProcessIsolation()) {
      content::IsolateAllSitesForTesting(command_line);
    } else {
      // TODO(khushalsagar): Enable tests which force a single renderer process
      // for all frames.
      // content::RenderProcessHost::SetMaxRendererProcessCount(1) is not
      // sufficient for that.
      GTEST_SKIP();
    }
  }

 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestMultiProcess,
                       AIPageContentMultipleCrossSiteFrames) {
  LoadPage(https_server()->GetURL("a.com", "/iframe_cross_site.html"));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 2);

  const auto& b_frame = page_content().root_node().children_nodes()[0];
  EXPECT_EQ(b_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& b_frame_data = b_frame.content_attributes().iframe_data();
  AssertValidURL(b_frame_data.url(), "b.com");
  EXPECT_FALSE(b_frame_data.likely_ad_frame());

  EXPECT_EQ(b_frame.children_nodes().size(), 1);
  AssertHasText(b_frame.children_nodes()[0], "This page has no title.\n\n");
  const auto& b_geometry = b_frame.content_attributes().geometry();
  AssertRectsEqual(b_geometry.outer_bounding_box(),
                   b_geometry.visible_bounding_box());

  const auto& c_frame = page_content().root_node().children_nodes()[1];
  EXPECT_EQ(c_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& c_frame_data = c_frame.content_attributes().iframe_data();
  AssertValidURL(c_frame_data.url(), "c.com");
  EXPECT_FALSE(c_frame_data.likely_ad_frame());
  EXPECT_EQ(b_frame.children_nodes().size(), 1);
  AssertHasText(c_frame.children_nodes()[0], "This page has no title.\n\n");
  const auto& c_geometry = c_frame.content_attributes().geometry();
  AssertRectsEqual(c_geometry.outer_bounding_box(),
                   c_geometry.visible_bounding_box());

  EXPECT_ALMOST_EQ(b_geometry.outer_bounding_box().width(),
                   c_geometry.outer_bounding_box().width());
  EXPECT_ALMOST_EQ(b_geometry.outer_bounding_box().height(),
                   c_geometry.outer_bounding_box().height());
  EXPECT_ALMOST_EQ(b_geometry.outer_bounding_box().y(),
                   c_geometry.outer_bounding_box().y());
  EXPECT_NE(b_geometry.outer_bounding_box().x(),
            c_geometry.outer_bounding_box().x());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PageContentProtoProviderBrowserTestMultiProcess,
                         testing::Bool());

class PageContentProtoProviderBrowserTestFencedFrame
    : public PageContentProtoProviderBrowserTest {
 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestFencedFrame,
                       AIPageContentFencedFrame) {
  LoadPage(https_server()->GetURL("a.com", "/fenced_frame/basic.html"),
           /* with_page_content = */ false);

  const GURL fenced_frame_url =
      https_server()->GetURL("b.com", "/fenced_frame/simple.html");
  auto* fenced_frame_rfh = fenced_frame_helper_.CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_NE(nullptr, fenced_frame_rfh);
  LoadData();

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);

  const auto& b_frame = page_content().root_node().children_nodes()[0];
  EXPECT_EQ(b_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& b_frame_data = b_frame.content_attributes().iframe_data();
  AssertValidURL(b_frame_data.url(), "b.com");
  EXPECT_FALSE(b_frame_data.likely_ad_frame());
  EXPECT_EQ(b_frame.children_nodes().size(), 1);
  AssertHasText(b_frame.children_nodes()[0], "Non empty simple page\n\n");
  const auto& b_geometry = b_frame.content_attributes().geometry();
  AssertRectsEqual(b_geometry.outer_bounding_box(),
                   b_geometry.visible_bounding_box());
}

}  // namespace

}  // namespace optimization_guide
